/*
 *   fcfont.cpp - Linux (fontconfig) backend for the guit3 font platform
 *   hooks declared in tadsfont.h (os_font_family_is_present() /
 *   os_font_data_for_name() / os_enum_font_families()).
 *
 *   This is the cross-platform counterpart of guifont.cpp's Win32
 *   implementation of the same hooks: GDI's EnumFontFamiliesEx() and
 *   CreateFontIndirect()+GetFontData() become fontconfig's FcFontList() and
 *   FcFontMatch(), respectively (os_enum_font_families() below also pulls in
 *   FreeType, to classify each family the way GDI's TrueType driver does -
 *   see that function's own comment).  CMake selects exactly one backend
 *   per build (Linux: this file; macOS: ctfont.cpp; Windows: guifont.cpp) -
 *   see htmltads/imgui/CMakeLists.txt and migration.md 5.4/G.
 *
 *   Unverified: there is no Linux build of guit3 yet (migration.md M4 lifts
 *   the "Windows only" CMake gate that makes that possible), so this file
 *   has been syntax- and type-checked against a real fontconfig install
 *   (WSL Ubuntu 24.04, libfontconfig-dev) but never actually run.
 */

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
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


/* ------------------------------------------------------------------------ */
/*
 *   os_enum_font_families() - report every installed family once, in the
 *   ENUMLOGFONTEX/NEWTEXTMETRIC shape Win32's EnumFontFamiliesEx() uses, so
 *   CHtmlPreferences::cust_refresh_font_lists() (htmlpref.cpp) can sort
 *   families into its serif/sans/script/typewriter buckets the same way on
 *   every platform.  That code (and the selector callbacks it uses) only
 *   look at two fields, both of which GDI derives from a TrueType font's
 *   OS/2 table on Windows too, so deriving them the same way here keeps the
 *   classification meaningfully equivalent rather than just plausible:
 *
 *   - tmPitchAndFamily's TMPF_FIXED_PITCH bit - note the confusing Win32
 *     convention this mirrors: the bit is SET for a proportional font and
 *     CLEAR for a fixed-pitch one.  Fontconfig's FC_SPACING already
 *     classifies this per family (FC_MONO/FC_CHARCELL vs FC_PROPORTIONAL/
 *     FC_DUAL), no font-file access needed.
 *
 *   - lfPitchAndFamily's family nibble (FF_ROMAN/FF_SWISS/FF_SCRIPT/
 *     FF_MODERN/FF_DECORATIVE) - a monospaced family is reported as
 *     FF_MODERN outright (matching how Windows classifies Courier New,
 *     Consolas, etc.), same as GDI keeping FF_MODERN mutually exclusive
 *     with the serif/sans/script classes.  Everything else is classified by
 *     reading the matched font file's OS/2 table via FreeType (already
 *     linked into guit3 - see ../CMakeLists.txt), preferring its PANOSE
 *     bytes ('bFamilyType'/'bSerifStyle') over the older 'sFamilyClass'
 *     field GDI's own TrueType driver uses: measured against this repo's
 *     actual installed fonts (WSL Ubuntu 24.04's default set: DejaVu,
 *     Ubuntu, Noto, plus the Ghostscript URW Type 1 faces), 'sFamilyClass'
 *     was left at its unset default (0) by every real TrueType/OpenType
 *     family except one, while PANOSE - which most font tools, including
 *     Google's and Canonical's, populate even when they skip
 *     'sFamilyClass' - classified roughly half of them correctly (all of
 *     DejaVu, Ubuntu's monospace faces, and Noto). 'sFamilyClass' is kept
 *     as a fallback for the reverse case (some fonts set it without
 *     PANOSE). Bare Type 1 faces (e.g. the Ghostscript URW35 set) have no
 *     OS/2 table at all and can't be classified by either method - no
 *     worse than GDI, which has the same trouble with Type 1 off ATM.
 *     Mapping: PANOSE 'bFamilyType' 3 (Script) -> FF_SCRIPT, 4
 *     (Decorative) -> FF_DECORATIVE, 2 (Text and Display) consults
 *     'bSerifStyle' (2-10, the serif subclasses, -> FF_ROMAN; 11-15, the
 *     sans subclasses, -> FF_SWISS); falling back to 'sFamilyClass' high
 *     byte 1/2/3/4/5/7 -> FF_ROMAN, 8 -> FF_SWISS, 9 -> FF_DECORATIVE, 10
 *     -> FF_SCRIPT.  Anything neither scheme classifies comes back
 *     FF_DONTCARE (0) - it still appears in the "all" list, just not in
 *     any of the four style-specific ones, same as an unclassified
 *     TrueType font would on Windows.
 */
namespace {

int fc_spacing_to_family(FcPattern *font)
{
    int spacing = FC_PROPORTIONAL;
    FcPatternGetInteger(font, FC_SPACING, 0, &spacing);
    return (spacing == FC_MONO || spacing == FC_CHARCELL) ? FF_MODERN : -1;
}

int fc_os2_class_to_family(FT_Library ft, const char *file, int index)
{
    if (ft == 0 || file == 0)
        return 0;

    FT_Face face;
    if (FT_New_Face(ft, file, index, &face) != 0)
        return 0;

    int family = 0;
    TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
    if (os2 != 0)
    {
        int bFamilyType = os2->panose[0], bSerifStyle = os2->panose[1];
        if (bFamilyType == 3)
            family = FF_SCRIPT;
        else if (bFamilyType == 4)
            family = FF_DECORATIVE;
        else if (bFamilyType == 2)
        {
            if (bSerifStyle >= 2 && bSerifStyle <= 10)
                family = FF_ROMAN;
            else if (bSerifStyle >= 11 && bSerifStyle <= 15)
                family = FF_SWISS;
        }

        if (family == 0)
        {
            switch ((os2->sFamilyClass >> 8) & 0xff)
            {
            case 1: case 2: case 3: case 4: case 5: case 7:
                family = FF_ROMAN;
                break;

            case 8:
                family = FF_SWISS;
                break;

            case 9:
                family = FF_DECORATIVE;
                break;

            case 10:
                family = FF_SCRIPT;
                break;
            }
        }
    }

    FT_Done_Face(face);
    return family;
}

} // namespace

void os_enum_font_families(unsigned int charset_id, FONTENUMPROC callback,
                           LPARAM lparam)
{
    FcPattern *pat = FcPatternCreate();
    FcObjectSet *os = FcObjectSetBuild(
        FC_FAMILY, FC_FILE, FC_INDEX, FC_SPACING, (char *)0);
    FcFontSet *fs = FcFontList(0, pat, os);
    if (fs == 0)
    {
        FcObjectSetDestroy(os);
        FcPatternDestroy(pat);
        return;
    }

    FT_Library ft = 0;
    if (FT_Init_FreeType(&ft) != 0)
        ft = 0;

    /*
     *   FcFontList() reports each family once per style/weight/width
     *   variant, same as EnumFontFamiliesEx() does on Windows - dedupe here
     *   rather than opening a FreeType face (or invoking the callback) more
     *   than once per family.
     */
    enum { MAX_SEEN = 4096, NAME_LEN = 256 };
    char (*seen)[NAME_LEN] =
        (char (*)[NAME_LEN])ImGui::MemAlloc(MAX_SEEN * NAME_LEN);
    int seen_count = 0;

    for (int i = 0 ; i < fs->nfont ; ++i)
    {
        FcPattern *font = fs->fonts[i];
        FcChar8 *family = 0, *file = 0;
        if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch
            || family == 0)
            continue;

        int dup = 0;
        for (int j = 0 ; j < seen_count ; ++j)
        {
            if (strcmp(seen[j], (const char *)family) == 0)
            {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;

        if (seen_count < MAX_SEEN)
        {
            strncpy(seen[seen_count], (const char *)family, NAME_LEN - 1);
            seen[seen_count][NAME_LEN - 1] = '\0';
            ++seen_count;
        }

        FcPatternGetString(font, FC_FILE, 0, &file);
        int index = 0;
        FcPatternGetInteger(font, FC_INDEX, 0, &index);

        ENUMLOGFONTEX elf;
        memset(&elf, 0, sizeof(elf));
        strncpy(elf.elfLogFont.lfFaceName, (const char *)family,
                sizeof(elf.elfLogFont.lfFaceName) - 1);

        int fam_family = fc_spacing_to_family(font);
        if (fam_family < 0)
            fam_family = fc_os2_class_to_family(
                ft, file != 0 ? (const char *)file : 0, index);
        elf.elfLogFont.lfPitchAndFamily = (BYTE)fam_family;

        NEWTEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        tm.tmCharSet = (BYTE)charset_id;
        if (fam_family != FF_MODERN)
            tm.tmPitchAndFamily |= TMPF_FIXED_PITCH;

        if (!callback(&elf, &tm, 0, lparam))
            break;
    }

    ImGui::MemFree(seen);

    if (ft != 0)
        FT_Done_FreeType(ft);

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
}
