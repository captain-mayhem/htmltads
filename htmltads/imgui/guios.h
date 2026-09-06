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


#endif /* GUIOS_H */
