/*
 *   guios_w32.cpp - Win32 backend for the guit3 OS-service hooks (guios.h)
 *
 *   Each function here is the Windows code that used to be inlined at the
 *   call site, moved behind the neutral guios.h interface unchanged.  A
 *   future non-Windows port supplies its own file implementing the same
 *   hooks (xdg-open, the ImGui style palette, glfwSetClipboardString, ...);
 *   CMake picks exactly one backend per build.  See migration.md 5.4/D-F.
 */

#include <windows.h>
#include <string.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "htmlres.h"      /* IDB_TERP_TOOLBAR, IDX_LICENSE_TEXT */
#include "guios.h"

/* this is missing from the Windows Platform SDK in MSVC .Net 2003 */
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR   2
#endif


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard - has_text only
 *
 *   set/get are the shared glfwSet/GetClipboardString() path in
 *   guios_common.cpp.  Only this probe stays per-platform: it runs every
 *   frame from render_toolbar()'s Paste-button enable check, and
 *   IsClipboardFormatAvailable() answers it without opening the clipboard,
 *   which the GLFW fetch the portable backend uses cannot.
 */

int os_clipboard_has_text(void)
{
    return IsClipboardFormatAvailable(CF_TEXT)
        || IsClipboardFormatAvailable(CF_UNICODETEXT);
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Wait cursor
 */

os_cursor_token_t os_set_wait_cursor(void)
{
    /* stock cursors are cached by the OS, so LoadCursor() on demand is fine */
    return (os_cursor_token_t)SetCursor(LoadCursor(NULL, IDC_WAIT));
}

void os_restore_cursor(os_cursor_token_t prev)
{
    SetCursor((HCURSOR)prev);
}


/* ------------------------------------------------------------------------ */
/*
 *   F. Shell integration
 */

int os_open_url(const char *url)
{
    /*
     *   ShellExecute returns a value <= 32 on failure; that's the same
     *   success test the call sites (process_command(), the Help > TADS on
     *   the Web menu item) used inline.
     */
    return (INT_PTR)ShellExecute(0, "open", url, 0, 0, SW_SHOWNORMAL) > 32;
}


/* ------------------------------------------------------------------------ */
/*
 *   E. System colors
 */

unsigned long os_get_sys_color(os_sys_color_t which)
{
    switch (which)
    {
    case OS_SYS_COLOR_HIGHLIGHT:       return GetSysColor(COLOR_HIGHLIGHT);
    case OS_SYS_COLOR_HIGHLIGHT_TEXT:  return GetSysColor(COLOR_HIGHLIGHTTEXT);
    case OS_SYS_COLOR_WINDOW:          return GetSysColor(COLOR_WINDOW);
    case OS_SYS_COLOR_WINDOW_TEXT:     return GetSysColor(COLOR_WINDOWTEXT);
    }
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   B. Bundled resources
 */

int os_load_string(int id, char *buf, size_t buflen)
{
    return LoadString(CTadsApp::get_app()->get_instance(),
                      id, buf, (int)buflen);
}

unsigned char *os_load_toolbar_rgba(int *width, int *height)
{
    /*
     *   This is the resource+GDI half of CHtmlSys_mainwin::load_toolbar_texture()
     *   lifted verbatim: LoadImage() the 4bpp indexed IDB_TERP_TOOLBAR
     *   (win32/runtbar.bmp - 304x15, nineteen 16x15 frames), expand it to a
     *   32bpp top-down DIB with GetDIBits(), then turn the color key (the
     *   top-left pixel) into a real alpha channel while swapping BGRA->RGBA.
     *   The caller keeps the GL upload.
     */
    HBITMAP hbmp = (HBITMAP)LoadImage(
        CTadsApp::get_app()->get_instance(), MAKEINTRESOURCE(IDB_TERP_TOOLBAR),
        IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    if (hbmp == 0)
        return 0;

    BITMAP bm;
    GetObject(hbmp, sizeof(bm), &bm);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.bmWidth;
    bi.bmiHeader.biHeight = -bm.bmHeight;      /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(0);
    unsigned char *pixels =
        (unsigned char *)th_malloc(bm.bmWidth * bm.bmHeight * 4);
    GetDIBits(hdc, hbmp, 0, bm.bmHeight, pixels, &bi, DIB_RGB_COLORS);
    ReleaseDC(0, hdc);
    DeleteObject(hbmp);

    /* the top-left pixel (BGRA order, alpha byte unused) is the mask color */
    unsigned char mask_b = pixels[0], mask_g = pixels[1], mask_r = pixels[2];

    /* convert BGRA -> RGBA in place, turning the color key into real alpha */
    int npix = bm.bmWidth * bm.bmHeight;
    for (int i = 0 ; i < npix ; ++i)
    {
        unsigned char *p = pixels + i*4;
        unsigned char b = p[0], g = p[1], r = p[2];
        bool is_mask = (b == mask_b && g == mask_g && r == mask_r);
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = is_mask ? 0 : 255;
    }

    *width = bm.bmWidth;
    *height = bm.bmHeight;
    return pixels;
}

char *os_load_license_text(size_t *len)
{
    *len = 0;

    HINSTANCE inst = CTadsApp::get_app()->get_instance();
    HRSRC hres = FindResource(
        inst, MAKEINTRESOURCE(IDX_LICENSE_TEXT), "TEXTFILE");
    if (hres == 0)
        return 0;

    HGLOBAL hgl = LoadResource(inst, hres);
    if (hgl == 0)
        return 0;

    const void *mem = LockResource(hgl);
    DWORD sz = SizeofResource(inst, hres);
    if (mem == 0 || sz == 0)
        return 0;

    char *result = (char *)th_malloc(sz);
    memcpy(result, mem, sz);
    *len = sz;
    return result;
}


/* ------------------------------------------------------------------------ */
/*
 *   K. Character encoding
 *
 *   The local-codepage -> UTF-16 -> UTF-8 pair that htmlgui.cpp's
 *   measure_text() / draw_text() ran inline, and the local-codepage ->
 *   UTF-16 half that get_max_chars_in_width() ran inline, moved here
 *   unchanged.
 */

char *os_local_to_utf8(unsigned int codepage,
                       const char *src, size_t srclen, size_t *out_len)
{
    int wlen = MultiByteToWideChar(codepage, MB_PRECOMPOSED,
                                   src, (int)srclen, NULL, 0);
    wchar_t *wbuf = (wchar_t *)th_malloc((wlen > 0 ? wlen : 1) * sizeof(wchar_t));
    MultiByteToWideChar(codepage, MB_PRECOMPOSED,
                        src, (int)srclen, wbuf, wlen);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen,
                                    NULL, 0, NULL, NULL);
    char *u8buf = (char *)th_malloc(u8len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, u8buf, u8len, NULL, NULL);
    u8buf[u8len] = '\0';

    th_free(wbuf);

    if (out_len != 0)
        *out_len = (size_t)u8len;
    return u8buf;
}

os_utf16_t *os_local_to_utf16(unsigned int codepage,
                              const char *src, size_t srclen, size_t *out_cnt)
{
    int wlen = MultiByteToWideChar(codepage, MB_PRECOMPOSED,
                                   src, (int)srclen, NULL, 0);
    os_utf16_t *wbuf =
        (os_utf16_t *)th_malloc((wlen > 0 ? wlen : 1) * sizeof(os_utf16_t));
    MultiByteToWideChar(codepage, MB_PRECOMPOSED,
                        src, (int)srclen, (wchar_t *)wbuf, wlen);

    if (out_cnt != 0)
        *out_cnt = (size_t)(wlen > 0 ? wlen : 0);
    return wbuf;
}

char *os_utf8_to_local(unsigned int codepage, const char *utf8, size_t *out_len)
{
    /* UTF-8 -> UTF-16 -> local code page, the inverse of os_local_to_utf8() */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t *wbuf = (wchar_t *)th_malloc((wlen > 0 ? wlen : 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf, wlen);

    /* wlen counts the terminating NUL (source was -1), so the result is
       NUL-terminated too and clen includes it */
    int clen = WideCharToMultiByte(codepage, 0, wbuf, wlen,
                                   NULL, 0, NULL, NULL);
    char *cbuf = (char *)th_malloc(clen > 0 ? clen : 1);
    WideCharToMultiByte(codepage, 0, wbuf, wlen, cbuf, clen, NULL, NULL);

    th_free(wbuf);

    if (out_len != 0)
        *out_len = (size_t)(clen > 0 ? clen - 1 : 0);
    return cbuf;
}


/* ------------------------------------------------------------------------ */
/*
 *   L. Keyboard
 *
 *   Letters and digits need no table entry: VK_A..VK_Z/VK_0..VK_9,
 *   GLFW_KEY_A..GLFW_KEY_Z/GLFW_KEY_0..GLFW_KEY_9, and their ASCII codes are
 *   all numerically identical, so vk_to_glfw_key()/glfw_key_to_vk() treat
 *   them as the identity.  Everything else - named keys (VK_BACK, VK_F1,
 *   ...), the numeric keypad, and the punctuation keys reachable through
 *   os_char_to_key()'s xch[] table in tadskb.cpp - goes through the table
 *   below.  The punctuation entries (VK_OEM_*) assume a US keyboard layout,
 *   the same assumption GLFW's own Win32 backend makes internally when it
 *   translates scan codes to GLFW_KEY_* constants - there is no portable way
 *   to ask "what layout is this," so the two sides have to agree on one.
 */
struct vk_glfw_pair_t { int vk; int glfw_key; };
static const vk_glfw_pair_t vk_glfw_table[] =
{
    { VK_BACK, GLFW_KEY_BACKSPACE },
    { VK_TAB, GLFW_KEY_TAB },
    { VK_RETURN, GLFW_KEY_ENTER },
    { VK_PAUSE, GLFW_KEY_PAUSE },
    { VK_ESCAPE, GLFW_KEY_ESCAPE },
    { VK_SPACE, GLFW_KEY_SPACE },
    { VK_PRIOR, GLFW_KEY_PAGE_UP },
    { VK_NEXT, GLFW_KEY_PAGE_DOWN },
    { VK_END, GLFW_KEY_END },
    { VK_HOME, GLFW_KEY_HOME },
    { VK_LEFT, GLFW_KEY_LEFT },
    { VK_UP, GLFW_KEY_UP },
    { VK_RIGHT, GLFW_KEY_RIGHT },
    { VK_DOWN, GLFW_KEY_DOWN },
    { VK_SNAPSHOT, GLFW_KEY_PRINT_SCREEN },
    { VK_INSERT, GLFW_KEY_INSERT },
    { VK_DELETE, GLFW_KEY_DELETE },
    { VK_NUMPAD0, GLFW_KEY_KP_0 },
    { VK_NUMPAD1, GLFW_KEY_KP_1 },
    { VK_NUMPAD2, GLFW_KEY_KP_2 },
    { VK_NUMPAD3, GLFW_KEY_KP_3 },
    { VK_NUMPAD4, GLFW_KEY_KP_4 },
    { VK_NUMPAD5, GLFW_KEY_KP_5 },
    { VK_NUMPAD6, GLFW_KEY_KP_6 },
    { VK_NUMPAD7, GLFW_KEY_KP_7 },
    { VK_NUMPAD8, GLFW_KEY_KP_8 },
    { VK_NUMPAD9, GLFW_KEY_KP_9 },
    { VK_MULTIPLY, GLFW_KEY_KP_MULTIPLY },
    { VK_ADD, GLFW_KEY_KP_ADD },
    { VK_SUBTRACT, GLFW_KEY_KP_SUBTRACT },
    { VK_DECIMAL, GLFW_KEY_KP_DECIMAL },
    { VK_DIVIDE, GLFW_KEY_KP_DIVIDE },
    { VK_F1, GLFW_KEY_F1 }, { VK_F2, GLFW_KEY_F2 },
    { VK_F3, GLFW_KEY_F3 }, { VK_F4, GLFW_KEY_F4 },
    { VK_F5, GLFW_KEY_F5 }, { VK_F6, GLFW_KEY_F6 },
    { VK_F7, GLFW_KEY_F7 }, { VK_F8, GLFW_KEY_F8 },
    { VK_F9, GLFW_KEY_F9 }, { VK_F10, GLFW_KEY_F10 },
    { VK_F11, GLFW_KEY_F11 }, { VK_F12, GLFW_KEY_F12 },
    { VK_F13, GLFW_KEY_F13 }, { VK_F14, GLFW_KEY_F14 },
    { VK_F15, GLFW_KEY_F15 }, { VK_F16, GLFW_KEY_F16 },
    { VK_F17, GLFW_KEY_F17 }, { VK_F18, GLFW_KEY_F18 },
    { VK_F19, GLFW_KEY_F19 }, { VK_F20, GLFW_KEY_F20 },
    { VK_F21, GLFW_KEY_F21 }, { VK_F22, GLFW_KEY_F22 },
    { VK_F23, GLFW_KEY_F23 }, { VK_F24, GLFW_KEY_F24 },

    /* punctuation - US layout VK_OEM_* codes */
    { VK_OEM_1, GLFW_KEY_SEMICOLON },        /* ;: */
    { VK_OEM_PLUS, GLFW_KEY_EQUAL },         /* =+ */
    { VK_OEM_COMMA, GLFW_KEY_COMMA },        /* ,< */
    { VK_OEM_MINUS, GLFW_KEY_MINUS },        /* -_ */
    { VK_OEM_PERIOD, GLFW_KEY_PERIOD },      /* .> */
    { VK_OEM_2, GLFW_KEY_SLASH },            /* /? */
    { VK_OEM_3, GLFW_KEY_GRAVE_ACCENT },     /* `~ */
    { VK_OEM_4, GLFW_KEY_LEFT_BRACKET },     /* [{ */
    { VK_OEM_5, GLFW_KEY_BACKSLASH },        /* \| */
    { VK_OEM_6, GLFW_KEY_RIGHT_BRACKET },    /* ]} */
    { VK_OEM_7, GLFW_KEY_APOSTROPHE },       /* '" */
};
static const int vk_glfw_table_cnt =
    sizeof(vk_glfw_table) / sizeof(vk_glfw_table[0]);

static bool is_alnum_vk(int vk)
{
    return (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9');
}

static os_key_t vk_to_glfw_key(int vk)
{
    if (is_alnum_vk(vk))
        return vk;
    for (int i = 0 ; i < vk_glfw_table_cnt ; ++i)
        if (vk_glfw_table[i].vk == vk)
            return vk_glfw_table[i].glfw_key;
    return 0;
}

static int glfw_key_to_vk(os_key_t key)
{
    if ((key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        || (key >= GLFW_KEY_0 && key <= GLFW_KEY_9))
        return key;
    for (int i = 0 ; i < vk_glfw_table_cnt ; ++i)
        if (vk_glfw_table[i].glfw_key == key)
            return vk_glfw_table[i].vk;
    return 0;
}

int os_key_to_char(os_key_t key)
{
    int vk = glfw_key_to_vk(key);
    if (vk == 0)
        return 0;

    return (int)(MapVirtualKey(vk, MAPVK_VK_TO_CHAR) & 0x7FFFFFFF);
}

os_key_t os_char_to_key(int ch, int *shift_out)
{
    *shift_out = 0;

    UINT s = VkKeyScan((char)ch);
    if (s == 0)
        return 0;

    *shift_out = (s & 0x100 ? OS_KEY_SHIFT : 0)
               | (s & 0x200 ? OS_KEY_CTRL : 0)
               | (s & 0x400 ? OS_KEY_ALT : 0);

    return vk_to_glfw_key(s & 0xFF);
}

int os_load_accel_table(int accel_id, os_accel_entry_t *entries,
                        int max_entries)
{
    HACCEL h = LoadAccelerators(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(accel_id));
    if (h == 0)
        return 0;

    int n = CopyAcceleratorTable(h, 0, 0);
    if (n <= 0)
        return 0;

    ACCEL *raw = (ACCEL *)th_malloc(n * sizeof(ACCEL));
    n = CopyAcceleratorTable(h, raw, n);

    int out = 0;
    for (int i = 0 ; i < n && out < max_entries ; ++i)
    {
        os_key_t key;
        int shift;

        if (raw[i].fVirt & FVIRTKEY)
        {
            /* virtual-key entry: fVirt's shift bits are explicit */
            key = vk_to_glfw_key(raw[i].key);
            shift = ((raw[i].fVirt & FSHIFT) ? OS_KEY_SHIFT : 0)
                  | ((raw[i].fVirt & FCONTROL) ? OS_KEY_CTRL : 0)
                  | ((raw[i].fVirt & FALT) ? OS_KEY_ALT : 0);
        }
        else
        {
            /*
             *   Character-mode entry (win32/htmlcmn.rc's Alt+./Alt+,/Alt+>/
             *   Alt+< rows): 'key' is an ASCII character matched via
             *   WM_SYSCHAR rather than a VK_xxx, so its shift state (if any)
             *   is implied by the character itself - '>' already means
             *   Shift+'.' - the same layout query CTadsKeyboard uses for
             *   punctuation (tadskb.cpp).  Only Alt shows up as an explicit
             *   fVirt bit here (Ctrl+char and plain char accelerators would
             *   just be ordinary typing, not an accelerator).
             */
            int char_shift = 0;
            key = os_char_to_key((int)raw[i].key, &char_shift);
            shift = char_shift | ((raw[i].fVirt & FALT) ? OS_KEY_ALT : 0);
        }

        if (key == 0)
            continue;

        entries[out].key = key;
        entries[out].shift = shift;
        entries[out].cmd = raw[i].cmd;
        ++out;
    }

    th_free(raw);
    return out;
}
