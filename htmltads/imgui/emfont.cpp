/*
 *   emfont.cpp - Emscripten placeholder backend for the guit3 font platform
 *   hooks declared in tadsfont.h (os_font_family_is_present() /
 *   os_font_data_for_name()).
 *
 *   Unlike Win32 (guifont_w32.cpp/GDI), Linux (fcfont.cpp/fontconfig) and
 *   macOS (ctfont.cpp/CoreText), there is no installed-font store to query
 *   inside a browser sandbox - Emscripten ships no fontconfig-equivalent
 *   API. Both hooks therefore always report "not found"/"no data", exactly
 *   as they would on a real system with no matching font installed;
 *   CTadsFont's caller already handles that case by leaving the font unbaked
 *   and falling back to the ImGui atlas default. A real implementation would
 *   need font files bundled as Emscripten-packaged data and matched by name
 *   in JS/C++ instead of queried from the OS - not attempted yet, this only
 *   exists so guit3 has something to link against when targeting Emscripten
 *   (see htmltads/imgui/CMakeLists.txt and migration.md).
 */

#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

int os_font_family_is_present(const char *fontname, size_t len)
{
    return 0;
}

unsigned char *os_font_data_for_name(const char *name, int weight, int italic,
                                     int charset, size_t *data_size)
{
    return 0;
}
