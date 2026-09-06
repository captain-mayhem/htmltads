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

#include "guios.h"


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
