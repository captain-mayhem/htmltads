/*
 *   guios.h - guit3 OS-service platform hooks
 *
 *   A neutral, windows.h-free home for the small OS-integration calls the
 *   guit3 GUI layer still needs but that GLFW/ImGui/FreeType don't provide:
 *   shell integration, system colors, the clipboard, mouse cursors, the
 *   millisecond tick clock, character-set conversion and keyboard-layout
 *   queries.  Each hook is declared here once and implemented per platform
 *   in a companion file selected by CMake (today only the Win32 backend,
 *   guios_w32.cpp, exists); this mirrors the os_font_family_is_present()
 *   precedent in tadsfont.h / guifont.cpp.  See migration.md section 5.4
 *   (items D, E, F, K, L) and the "M2 / A2" note in section 5.5.
 *
 *   Introducing these hooks does not change behavior on Windows: every
 *   backend below is the current call-site code lifted verbatim.
 */

#ifndef GUIOS_H
#define GUIOS_H

#include <stddef.h>
#include <GLFW/glfw3.h>


/* ------------------------------------------------------------------------ */
/*
 *   D. Millisecond tick clock
 *
 *   A free-running millisecond counter for measuring short intervals (link
 *   double-click timing, "Working..." throttling, elapsed play time, drag
 *   auto-scroll pacing).  Only the difference between two readings is
 *   meaningful.  Implemented once for every platform (guios_common.cpp) on
 *   std::chrono::steady_clock, measured from the first call.
 */
unsigned long os_get_tick_ms(void);


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard (plain text, UTF-8)
 *
 *   set/get move UTF-8 bytes to/from the system clipboard - on every
 *   platform this is glfwSet/GetClipboardString(), so those two live in the
 *   shared guios_common.cpp.  The engine's own text is local-codepage, so
 *   the copy/paste call sites convert with os_local_to_utf8() /
 *   os_utf8_to_local() (item K) on the way through; CR/LF normalization
 *   stays the caller's job, as it always was.
 *
 *   has_text stays per-platform: GLFW has no "is there text" query short of
 *   a full fetch, and can_paste() - hence this - runs every frame from the
 *   toolbar, so the Win32 backend keeps the cheap IsClipboardFormatAvailable()
 *   probe (guios_w32.cpp) and only the non-Windows backend pays for the
 *   fetch (guios_portable.cpp).
 */

/* Replace the clipboard contents with the given NUL-terminated UTF-8 string.
   Returns nonzero on success.  (guios_common.cpp) */
int os_clipboard_set_text(const char *text);

/* Nonzero if the clipboard currently holds text.  (per-platform backend) */
int os_clipboard_has_text(void);

/* Return a copy of the clipboard's text as UTF-8, allocated with th_malloc()
   (free it with th_free()), or null if the clipboard holds no text.
   (guios_common.cpp) */
char *os_clipboard_get_text(void);


/* ------------------------------------------------------------------------ */
/*
 *   D. Wait cursor
 *
 *   Every hover-shape cursor (arrow, I-beam, hand) is handled entirely by
 *   ImGui::SetMouseCursor() from inside the frame loop.  The one case ImGui
 *   can't cover is the busy cursor shown around long synchronous operations
 *   (text search, formatting, cache pruning): those block the render loop, so
 *   an ImGui cursor request - which only takes effect at the next NewFrame -
 *   would never be applied.  os_set_wait_cursor() sets the OS-level busy
 *   cursor immediately and returns an opaque token for the previously-active
 *   cursor; os_restore_cursor() takes that token back once the operation
 *   completes.  Callers that don't restore just ignore the token.
 */
typedef void *os_cursor_token_t;

os_cursor_token_t os_set_wait_cursor(void);
void os_restore_cursor(os_cursor_token_t prev);


/* ------------------------------------------------------------------------ */
/*
 *   F. Shell integration
 */

/*
 *   Open a URL in the user's default web browser (Windows: ShellExecute
 *   "open"; later: xdg-open / "open").  Returns nonzero on success.  Callers
 *   that don't surface an error can ignore the result.
 */
int os_open_url(const char *url);


/* ------------------------------------------------------------------------ */
/*
 *   E. System colors
 *
 *   The handful of OS palette entries guit3 reads: the text-selection
 *   highlight, and the window fg/bg used when the "Use Windows colors"
 *   preference is on.  The Win32 backend forwards to GetSysColor(); a
 *   non-Windows backend can return fixed sensible values or pull from the
 *   ImGui style palette.
 */
enum os_sys_color_t
{
    OS_SYS_COLOR_HIGHLIGHT,        /* selected-text background   (COLOR_HIGHLIGHT)     */
    OS_SYS_COLOR_HIGHLIGHT_TEXT,   /* selected-text foreground   (COLOR_HIGHLIGHTTEXT) */
    OS_SYS_COLOR_WINDOW,           /* window background          (COLOR_WINDOW)        */
    OS_SYS_COLOR_WINDOW_TEXT       /* window text                (COLOR_WINDOWTEXT)    */
};

/*
 *   Return the given system color as a packed 0x00BBGGRR value - the same
 *   encoding as a Win32 COLORREF, so results feed COLORREF_to_HTML_color()
 *   and the GDI color setters directly.
 */
unsigned long os_get_sys_color(os_sys_color_t which);


/* ------------------------------------------------------------------------ */
/*
 *   B. Bundled resources
 *
 *   The three kinds of resource the live ImGui code still pulls out of the
 *   Windows executable: UI strings, the toolbar icon strip, and the license
 *   text.  Each hook's Win32 backend is the current call-site code lifted
 *   verbatim (LoadString / LoadImage+GetDIBits / FindResource).  A
 *   non-Windows backend supplies the same data from a generated string table
 *   and embedded byte arrays - that portable half is migration.md's M3 work;
 *   this seam is just the M2/A2 step of naming the calls.  See
 *   migration.md 5.4/B.
 *
 *   Dead native-menu and superseded native-dialog code still calls
 *   LoadString() directly through <windows.h> - it compiles Windows-only
 *   until the gates flip (migration.md 5.5/A1) and there is nothing for a
 *   portable backend to do there.
 */

/*
 *   Load UI string number 'id' (an IDS_* / RESID_* resource id) into 'buf',
 *   NUL-terminated and truncated to 'buflen'.  Returns the number of
 *   characters copied (0 if the id is unknown), same contract as Win32
 *   LoadString().  Windows: LoadString() against the app instance.
 */
int os_load_string(int id, char *buf, size_t buflen);

/*
 *   Load the toolbar icon strip (IDB_TERP_TOOLBAR) as a newly allocated
 *   top-down RGBA8 pixel buffer, with the bitmap's color-key (the value of
 *   its top-left pixel) already converted to a zero alpha channel - GL has
 *   no color-key equivalent, so the conversion has to happen here.  On
 *   success returns the buffer (free it with th_free()) and fills
 *   *width/*height; on failure returns null.  Windows:
 *   LoadImage(LR_CREATEDIBSECTION) + GetDIBits() to a 32bpp DIB.
 */
unsigned char *os_load_toolbar_rgba(int *width, int *height);

/*
 *   Return the license text (IDX_LICENSE_TEXT) as a newly allocated buffer
 *   of *len bytes, or null if unavailable.  The buffer is the raw resource
 *   bytes and is *not* guaranteed NUL-terminated; free it with th_free().
 *   Windows: FindResource()/LoadResource() of the "TEXTFILE" resource.
 */
char *os_load_license_text(size_t *len);


/* ------------------------------------------------------------------------ */
/*
 *   K. Character encoding
 *
 *   The HTML engine hands the GUI its text as bytes in the local character
 *   set - a Win32 code page, the font's oshtml_charset_id_t::codepage
 *   (CP_ACP for the default ANSI set).  Dear ImGui wants UTF-8, and the
 *   line-break measurement loop wants one array entry per character.  These
 *   these hooks do that conversion.  The Win32 backend is the
 *   MultiByteToWideChar / WideCharToMultiByte pair lifted verbatim from the
 *   call sites in htmlgui.cpp (measure_text(), draw_text(),
 *   get_max_chars_in_width()); os_utf8_to_local() is the same pair run
 *   backwards, for the paste path (do_paste() -> insert_text_from_hglobal(),
 *   which is shared with the OLE drag sink and stays local-codepage).  A
 *   non-Windows backend routes all of this through the TADS charmap layer the
 *   VM already loads (charmap/cmaplib.t3r) rather than a second parallel
 *   encoding assumption - that portable half is migration.md's M3 work; see
 *   migration.md 5.4/K.
 */

/*
 *   One UTF-16 code unit.  Deliberately unsigned short rather than wchar_t:
 *   guios.h stays windows.h-free, and wchar_t's width is platform-dependent
 *   (16 bits on Windows, 32 on most Unix) while this is always UTF-16.
 */
typedef unsigned short os_utf16_t;

/*
 *   Convert 'srclen' bytes of 'src', interpreted in code page 'codepage', to
 *   a newly allocated NUL-terminated UTF-8 string.  On success returns the
 *   buffer (free it with th_free()) and, when 'out_len' is non-null, stores
 *   the length in bytes excluding the terminator there.  Returns null on
 *   failure.  Windows: MultiByteToWideChar(MB_PRECOMPOSED) then
 *   WideCharToMultiByte(CP_UTF8).
 */
char *os_local_to_utf8(unsigned int codepage,
                       const char *src, size_t srclen, size_t *out_len);

/*
 *   Convert 'srclen' bytes of 'src', interpreted in code page 'codepage', to
 *   a newly allocated array of UTF-16 code units (NOT NUL-terminated).  On
 *   success returns the array (free it with th_free()) and stores the unit
 *   count in *out_cnt.  Returns null on failure.  Windows:
 *   MultiByteToWideChar(MB_PRECOMPOSED).
 *
 *   The one caller (get_max_chars_in_width()) treats each returned unit as
 *   one character - one glyph-advance lookup.  That is exact for the local
 *   single- and double-byte code pages in play (none can produce a
 *   character outside the Basic Multilingual Plane, so no surrogate pairs)
 *   and matches the assumption the inline Win32 code already made.
 */
os_utf16_t *os_local_to_utf16(unsigned int codepage,
                              const char *src, size_t srclen, size_t *out_cnt);

/*
 *   Convert a NUL-terminated UTF-8 string to a newly allocated
 *   NUL-terminated string in code page 'codepage'.  On success returns the
 *   buffer (free it with th_free()) and, when 'out_len' is non-null, stores
 *   the length in bytes excluding the terminator there.  Returns null on
 *   failure.  Characters with no representation in the target code page are
 *   replaced with that code page's default substitute (typically '?') -
 *   the same lossy behavior the old CF_TEXT paste path had.  Windows:
 *   MultiByteToWideChar(CP_UTF8) then WideCharToMultiByte(codepage).
 */
char *os_utf8_to_local(unsigned int codepage,
                       const char *utf8, size_t *out_len);


/* ------------------------------------------------------------------------ */
/*
 *   L. Keyboard - canonical key codes, layout queries, and accelerator
 *   tables
 *
 *   guit3's one canonical key code is a plain GLFW_KEY_* value (already
 *   portable - identical across GLFW's Win32/X11/Cocoa backends, so unlike
 *   items B-K there's no separate "portable backend" to build later: the
 *   enum itself needs no per-platform variant.  Two things still do need an
 *   OS query, because they depend on the live keyboard layout, which GLFW
 *   doesn't expose: what character an unshifted key produces
 *   (os_key_to_char, replacing MapVirtualKey), and what key produces a given
 *   character (os_char_to_key, replacing VkKeyScan).  CTadsKeyboard
 *   (tadskb.cpp) is the one caller of both - it used to call the Win32 APIs
 *   directly; this seam is that call moved behind a name, byte-identical on
 *   Windows, per the os_font_family_is_present() precedent.
 *
 *   The third piece, os_load_accel_table(), answers a different question:
 *   given one of the ACCELERATORS resources (IDR_ACCEL_WIN/IDR_ACCEL_EMACS,
 *   win32/htmlcmn.rc) that map keys straight to do_command() command IDs,
 *   return its bindings as canonical-key entries.  This is what lets
 *   CHtmlSys_mainwin::do_accel_keys() (htmlgui.cpp) dispatch real keyboard
 *   shortcuts every frame without a Win32 message loop to run
 *   TranslateAccelerator() through - see migration.md 5.4/L.
 */

/* shift-key bits - same encoding and values as tadskb.h's CTKB_SHIFT/CTRL/ALT */
#define OS_KEY_SHIFT   0x0001
#define OS_KEY_CTRL    0x0002
#define OS_KEY_ALT     0x0004

/* a canonical key code - one of the portable GLFW_KEY_* values */
typedef int os_key_t;

/*
 *   Return the unshifted, unmodified character 'key' produces on the
 *   current keyboard layout (e.g. GLFW_KEY_A -> 'A'), or 0 if it doesn't
 *   produce a printable ASCII character.  Windows:
 *   MapVirtualKey(vk, MAPVK_VK_TO_CHAR).
 */
int os_key_to_char(os_key_t key);

/*
 *   Return the key that generates ASCII character 'ch' on the current
 *   keyboard layout, or 0 if no key does.  On success, also stores any
 *   shift bits (OS_KEY_SHIFT/CTRL/ALT) required to generate it in
 *   *shift_out.  Windows: VkKeyScan(ch).
 */
os_key_t os_char_to_key(int ch, int *shift_out);

/* one binding in a portable accelerator table: key + shift bits -> command id */
struct os_accel_entry_t
{
    os_key_t key;
    int shift;
    unsigned int cmd;
};

/*
 *   Load the bindings of ACCELERATORS resource 'accel_id' (an IDR_ACCEL_*
 *   id) into 'entries', writing at most 'max_entries', and return the
 *   number of entries written (0 on failure or an empty table).  Windows:
 *   LoadAccelerators() + CopyAcceleratorTable(), converting each VK_xxx key
 *   to its canonical os_key_t.
 */
int os_load_accel_table(int accel_id, os_accel_entry_t *entries,
                        int max_entries);


/* ------------------------------------------------------------------------ */
/*
 *   M. Debug console
 *
 *   A raw system console window used to print low-level diagnostics
 *   (memory-block dumps via os_dbg_sys_msg(), tadshtml2.cpp) before/after a
 *   run when built with TADSHTML_DEBUG defined.  guimain.cpp's main() calls
 *   these unconditionally; the backend decides whether TADSHTML_DEBUG is on
 *   and does the real work or nothing.  Windows: AllocConsole(), and a
 *   drain-then-wait-for-a-keystroke loop on the console input buffer so the
 *   user can read the console before it disappears with the process.  A
 *   non-Windows backend doesn't need a console at all - stdout already goes
 *   somewhere the user can see it - so both can be no-ops there.
 */

void os_init_debug_console(void);
void os_close_debug_console(void);


#endif /* GUIOS_H */
