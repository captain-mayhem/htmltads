/*
 *   guios_portable.cpp - non-Windows backend for the guit3 OS-service hooks
 *   (guios.h)
 *
 *   This is the cross-platform counterpart of guios_w32.cpp: same hooks, no
 *   <windows.h>.  CMake selects exactly one backend per build - guios_w32.cpp
 *   on WIN32, this file everywhere else (see htmltads/imgui/CMakeLists.txt).
 *
 *   Coverage as of M3/D-F (see migration.md 5.4/D-F, 5.5):
 *     - D. clipboard has_text  glfwGetClipboardString probe (set/get are the
 *                              shared glfwSet/GetClipboardString() path in
 *                              guios_common.cpp)
 *     - D. wait cursor ...... no-op (GLFW has no busy cursor shape; see below)
 *     - E. system colors .... fixed sensible values
 *     - F. shell ............ xdg-open / open via fork+exec
 *
 *   D's tick clock is platform-independent (std::chrono) and lives in the
 *   shared guios_common.cpp, not here.
 *
 *   NOT yet implemented here:
 *     - item B (resources): os_load_string(), os_load_toolbar_rgba(),
 *       os_load_license_text() - need the generated string table and the
 *       embedded runtbar.bmp / license.txt byte arrays (migration.md 5.4/B).
 *     - item K (character encoding): os_local_to_utf8(), os_local_to_utf16(),
 *       os_utf8_to_local() - the A2 seam is built (guios.h + guios_w32.cpp)
 *       but the portable backend has to route local-codepage <-> Unicode
 *       through the TADS charmap layer (charmap/cmaplib.t3r) rather than
 *       assume a code page (migration.md 5.4/K).
 *   Both are still M3 work; until they land, a non-Windows link of guit3 is
 *   incomplete.  The build gate in CMakeLists.txt (if NOT WIN32 return()) is
 *   still closed, so nothing links this yet - that gate lifts in M4.
 */

#ifdef _WIN32
#error "guios_portable.cpp is the non-Windows backend; Windows builds use guios_w32.cpp"
#endif

#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <GLFW/glfw3.h>

#include "tadshtml.h"     /* th_malloc / th_free */
#include "guios.h"


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard - has_text only
 *
 *   set/get are the shared glfwSet/GetClipboardString() path in
 *   guios_common.cpp.  GLFW offers no format query, so the only "is there
 *   text" test is a full fetch; unlike Win32 (IsClipboardFormatAvailable),
 *   this backend has nothing cheaper, and can_paste() - which calls this -
 *   runs every frame from the toolbar.  Acceptable for now; a real
 *   non-Windows port can add a lighter probe if it matters.
 */

int os_clipboard_has_text(void)
{
    const char *s = glfwGetClipboardString(NULL);
    return s != NULL && s[0] != '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Wait cursor
 *
 *   GLFW 3.5 has no busy/hourglass standard cursor (only arrow, I-beam,
 *   crosshair, hand, the resize shapes and not-allowed), and the operations
 *   that want a busy cursor block the frame loop, so there is nothing useful
 *   to set here yet.  guit3 already shows a "Working..." status-line message
 *   alongside every one of these, so the visual cue is not lost.  A real busy
 *   cursor would need a bundled image fed through glfwCreateCursor() - future
 *   work, tracked with item B's other embedded assets.
 */
os_cursor_token_t os_set_wait_cursor(void)
{
    return NULL;
}

void os_restore_cursor(os_cursor_token_t /*prev*/)
{
}


/* ------------------------------------------------------------------------ */
/*
 *   F. Shell integration
 */
int os_open_url(const char *url)
{
#if defined(__APPLE__)
    const char *opener = "open";
#else
    const char *opener = "xdg-open";
#endif

    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0)
    {
        /* child - detach from our process group so the browser outlives us,
           then hand off to the platform opener */
        setsid();
        execlp(opener, opener, url, (char *)NULL);
        _exit(127);
    }

    /* parent - xdg-open / open spawn the handler and exit promptly, so a
       blocking wait just reaps the child and reports whether it launched */
    int status = 0;
    if (waitpid(pid, &status, 0) != pid)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   E. System colors
 *
 *   Off Windows there is no single system palette to read (it varies by
 *   desktop environment and theme), so return fixed values that read well on
 *   a light UI.  Same packed 0x00BBGGRR encoding as a Win32 COLORREF, so the
 *   results still feed COLORREF_to_HTML_color() and GetRValue()/GetGValue()/
 *   GetBValue() unchanged.
 */
static unsigned long pack_bgr(unsigned r, unsigned g, unsigned b)
{
    return (unsigned long)r | ((unsigned long)g << 8) | ((unsigned long)b << 16);
}

unsigned long os_get_sys_color(os_sys_color_t which)
{
    switch (which)
    {
    case OS_SYS_COLOR_HIGHLIGHT:       return pack_bgr(0x00, 0x78, 0xD7); /* blue   */
    case OS_SYS_COLOR_HIGHLIGHT_TEXT:  return pack_bgr(0xFF, 0xFF, 0xFF); /* white  */
    case OS_SYS_COLOR_WINDOW:          return pack_bgr(0xFF, 0xFF, 0xFF); /* white  */
    case OS_SYS_COLOR_WINDOW_TEXT:     return pack_bgr(0x00, 0x00, 0x00); /* black  */
    }
    return 0;
}
