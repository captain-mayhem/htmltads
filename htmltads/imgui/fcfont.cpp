/*
 *   fcfont.cpp - Linux (fontconfig) backend for the guit3 font platform
 *   hooks declared in tadsfont.h (os_font_family_is_present() /
 *   os_font_data_for_name()).
 *
 *   This is the cross-platform counterpart of guifont.cpp's Win32
 *   implementation of the same two hooks: GDI's EnumFontFamiliesEx() and
 *   CreateFontIndirect()+GetFontData() become fontconfig's FcFontList() and
 *   FcFontMatch(), respectively.  CMake selects exactly one backend per
 *   build (Linux: this file; macOS: ctfont.cpp; Windows: guifont.cpp) - see
 *   htmltads/imgui/CMakeLists.txt and migration.md 5.4/G.
 *
 *   Unverified: there is no Linux build of guit3 yet (migration.md M4 lifts
 *   the "Windows only" CMake gate that makes that possible), so this file
 *   has been syntax- and type-checked against a real fontconfig install
 *   (WSL Ubuntu 24.04, libfontconfig-dev) but never actually run.
 */

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>
#include <imgui/imgui.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   os_font_family_is_present() - is a font family with this name actually
 *   installed?
 *
 *   Deliberately does NOT call FcConfigSubstitute()/FcDefaultSubstitute():
 *   those add generic fallback families (sans-serif, and so on) to the
 *   pattern, which would make FcFontList() report every family as present
 *   via the fallback rather than a real match.  A bare FcFontList() with
 *   only FC_FAMILY set only returns fonts whose family list actually
 *   contains a matching entry - the same "does this exact name exist"
 *   question EnumFontFamiliesEx() answers on Windows.
 */
int os_font_family_is_present(const char *fontname, size_t len)
{
    /* make a null-terminated copy of the font name */
    char namebuf[256];
    if (len > sizeof(namebuf) - 1)
        len = sizeof(namebuf) - 1;
    memcpy(namebuf, fontname, len);
    namebuf[len] = '\0';

    FcPattern *pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)namebuf);

    FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, (char *)0);
    FcFontSet *fs = FcFontList(0, pat, os);

    int present = (fs != 0 && fs->nfont > 0);

    if (fs != 0)
        FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);

    return present;
}


/* ------------------------------------------------------------------------ */
/*
 *   os_font_data_for_name() - resolve a family/weight/italic request to the
 *   raw bytes of the TrueType/OpenType file fontconfig's matcher picks for
 *   it, for FreeType to parse.
 *
 *   Unlike os_font_family_is_present(), this DOES run the normal
 *   substitution + match pass: CreateFontIndirect() on Windows likewise
 *   substitutes a fallback face when the exact one isn't installed rather
 *   than failing outright, and every live call site already only reaches
 *   here with a concrete face name (see CHtmlSysWin_win32::get_font()), so
 *   there's no risk of matching on an empty/wildcard family.
 *
 *   'charset' (a Win32 xxx_CHARSET value) has no fontconfig equivalent - it
 *   selects a legacy code page's glyph subset, not a family or style - so
 *   it's intentionally unused here.
 */
unsigned char *os_font_data_for_name(const char *name, int weight, int italic,
                                     int charset, size_t *data_size)
{
    if (name == 0)
        name = "";

    FcPattern *pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)name);
    FcPatternAddInteger(pat, FC_WEIGHT, FcWeightFromOpenType(weight));
    FcPatternAddInteger(pat, FC_SLANT,
                        italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);

    FcConfigSubstitute(0, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result;
    FcPattern *match = FcFontMatch(0, pat, &result);
    FcPatternDestroy(pat);
    if (match == 0)
        return 0;

    FcChar8 *file = 0;
    unsigned char *buffer = 0;
    if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch)
    {
        /*
         *   Read the whole file into an ImGui::MemAlloc()'d buffer, the same
         *   allocator the Win32 backend uses - the caller hands this
         *   straight to ImFontAtlas::AddFontFromMemoryTTF(), which takes
         *   ownership, or frees it with ImGui::MemFree() if unused.
         *
         *   A collection file (.ttc) can hold more than one face at
         *   FC_INDEX; os_font_data_for_name()'s contract has no index
         *   out-param, so this always hands FreeType face 0 of whatever file
         *   fontconfig matched, same as if AddFontFromMemoryTTF() were
         *   called with its default font_no.  Single-face files (by far the
         *   common case) are unaffected.
         */
        FILE *fp = fopen((const char *)file, "rb");
        if (fp != 0)
        {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (size > 0)
            {
                buffer = (unsigned char *)ImGui::MemAlloc((size_t)size);
                if (fread(buffer, 1, (size_t)size, fp) == (size_t)size)
                {
                    *data_size = (size_t)size;
                }
                else
                {
                    ImGui::MemFree(buffer);
                    buffer = 0;
                }
            }

            fclose(fp);
        }
    }

    FcPatternDestroy(match);
    return buffer;
}
