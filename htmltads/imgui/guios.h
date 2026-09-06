/*
 *   guios.h - guit3 OS-service platform hooks
 *
 *   A neutral, windows.h-free home for the small OS-integration calls the
 *   guit3 GUI layer still needs but that GLFW/ImGui/FreeType don't provide:
 *   shell integration, system colors, the clipboard, mouse cursors and the
 *   millisecond tick clock.  Each hook is declared here once and implemented
 *   per platform in a companion file selected by CMake (today only the Win32
 *   backend, guios_w32.cpp, exists); this mirrors the os_font_family_is_present()
 *   precedent in tadsfont.h / guifont.cpp.  See migration.md section 5.4
 *   (items D, E, F) and the "M2 / A2" note in section 5.5.
 *
 *   Introducing these hooks does not change behavior on Windows: every
 *   backend below is the current call-site code lifted verbatim.
 */

#ifndef GUIOS_H
#define GUIOS_H

#include <stddef.h>


/* ------------------------------------------------------------------------ */
/*
 *   D. Millisecond tick clock
 *
 *   A free-running millisecond counter for measuring short intervals (link
 *   double-click timing, "Working..." throttling, elapsed play time, drag
 *   auto-scroll pacing).  Only the difference between two readings is
 *   meaningful; the value wraps roughly every 49 days.  Windows:
 *   GetTickCount().
 */
unsigned long os_get_tick_ms(void);


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard (plain text)
 *
 *   The engine only ever puts/gets CF_TEXT.  The caller is responsible for
 *   CR/LF normalization (as it always was); these hooks just move bytes.
 *   Windows: the OpenClipboard()/GlobalAlloc() dance; later:
 *   glfwSetClipboardString()/glfwGetClipboardString().
 */

/* Replace the clipboard contents with the given NUL-terminated string.
   Returns nonzero on success. */
int os_clipboard_set_text(const char *text);

/* Nonzero if the clipboard currently holds text. */
int os_clipboard_has_text(void);

/* Return a copy of the clipboard's text, allocated with th_malloc() (free
   it with th_free()), or null if the clipboard holds no text. */
char *os_clipboard_get_text(void);


/* ------------------------------------------------------------------------ */
/*
 *   D. Mouse cursor
 *
 *   guit3 only ever asks for one of a few semantic shapes.  The wait cursor
 *   is swapped in around long synchronous operations that run outside the
 *   frame loop and restored afterwards, so os_set_mouse_cursor() returns an
 *   opaque token for the previously-active cursor that os_restore_mouse_cursor()
 *   takes back; callers that don't restore just ignore it.
 */
enum os_mouse_cursor_t
{
    OS_MOUSE_CURSOR_ARROW,
    OS_MOUSE_CURSOR_IBEAM,
    OS_MOUSE_CURSOR_HAND,
    OS_MOUSE_CURSOR_WAIT
};

typedef void *os_cursor_token_t;

os_cursor_token_t os_set_mouse_cursor(os_mouse_cursor_t which);
void os_restore_mouse_cursor(os_cursor_token_t prev);


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


#endif /* GUIOS_H */
