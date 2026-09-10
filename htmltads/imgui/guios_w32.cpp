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
