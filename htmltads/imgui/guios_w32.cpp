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
#include "guios.h"


/* ------------------------------------------------------------------------ */
/*
 *   D. Millisecond tick clock
 */

unsigned long os_get_tick_ms(void)
{
    return GetTickCount();
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard (plain text)
 */

int os_clipboard_set_text(const char *text)
{
    size_t len = strlen(text) + 1;

    /* stage the text in a movable global block, as CF_TEXT requires */
    HGLOBAL hmem = GlobalAlloc(GHND, len);
    if (hmem == 0)
        return 0;
    memcpy(GlobalLock(hmem), text, len);
    GlobalUnlock(hmem);

    if (!OpenClipboard(NULL))
    {
        GlobalFree(hmem);
        return 0;
    }
    EmptyClipboard();
    if (SetClipboardData(CF_TEXT, hmem) == 0)
    {
        /* the clipboard didn't take ownership - release it ourselves */
        CloseClipboard();
        GlobalFree(hmem);
        return 0;
    }
    CloseClipboard();
    return 1;
}

int os_clipboard_has_text(void)
{
    return IsClipboardFormatAvailable(CF_TEXT);
}

char *os_clipboard_get_text(void)
{
    char *result = 0;

    if (!OpenClipboard(NULL))
        return 0;

    HANDLE hmem = GetClipboardData(CF_TEXT);
    if (hmem != 0)
    {
        const char *src = (const char *)GlobalLock(hmem);
        if (src != 0)
        {
            size_t len = strlen(src) + 1;
            result = (char *)th_malloc(len);
            memcpy(result, src, len);
            GlobalUnlock(hmem);
        }
    }

    CloseClipboard();
    return result;
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Mouse cursor
 */

static HCURSOR cursor_for(os_mouse_cursor_t which)
{
    switch (which)
    {
    case OS_MOUSE_CURSOR_IBEAM:
        return LoadCursor(NULL, IDC_IBEAM);

    case OS_MOUSE_CURSOR_HAND:
        {
            /* prefer the app's custom "HAND_CURSOR" resource, as the old
               inline LoadCursor() did, then fall back to the stock hand */
            HCURSOR c = LoadCursor(GetModuleHandle(NULL), "HAND_CURSOR");
            return c != NULL ? c : LoadCursor(NULL, IDC_HAND);
        }

    case OS_MOUSE_CURSOR_WAIT:
        return LoadCursor(NULL, IDC_WAIT);

    case OS_MOUSE_CURSOR_ARROW:
    default:
        return LoadCursor(NULL, IDC_ARROW);
    }
}

os_cursor_token_t os_set_mouse_cursor(os_mouse_cursor_t which)
{
    return (os_cursor_token_t)SetCursor(cursor_for(which));
}

void os_restore_mouse_cursor(os_cursor_token_t prev)
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
