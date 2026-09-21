/*
 *   emfont.cpp - Emscripten backend for the guit3 font platform hooks
 *   declared in tadsfont.h (os_font_family_is_present() /
 *   os_font_data_for_name() / os_enum_font_families()).
 *
 *   Unlike Win32 (guifont_w32.cpp/GDI), Linux (fcfont.cpp/fontconfig) and
 *   macOS (ctfont.cpp/CoreText), there is no installed-font store to query
 *   inside a browser sandbox - Emscripten ships no fontconfig-equivalent
 *   API. Instead, a small set of real font files (the standard Dear ImGui
 *   sample fonts - see ../../imgui/misc/fonts/README.txt for the list and
 *   licenses) is packaged into the Emscripten virtual filesystem at build
 *   time (em_package(guit3fonts ...) in htmltads/imgui/CMakeLists.txt,
 *   mapped to the virtual path FONTS_DIR below) and this backend enumerates
 *   *that* directory with plain POSIX opendir()/readdir() - fully supported
 *   against Emscripten's preloaded MEMFS - instead of asking the OS. Each
 *   entry is opened with FreeType (already linked into guit3 - see
 *   ../CMakeLists.txt) to read its real family name and classify it,
 *   exactly the same OS/2-table-based classification fcfont.cpp uses for
 *   its own os_enum_font_families() (see that function's comment for the
 *   full rationale and the real-world testing that led to preferring
 *   PANOSE over the older 'sFamilyClass' field).
 *
 *   Only six files ship today, so this treats "the font list" as small
 *   enough to just re-scan and re-open every file on every call rather than
 *   caching anything - if a real game wants a much larger bundled font set,
 *   revisit that.
 */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <imgui/imgui.h>

#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/* virtual path the fonts are packaged at - see em_package(guit3fonts ...)
   in htmltads/imgui/CMakeLists.txt */
#define FONTS_DIR "fonts"

namespace {

/*
 *   Classify an already-open face into the FF_ROMAN/FF_SWISS/FF_SCRIPT/
 *   FF_MODERN/FF_DECORATIVE family nibble cust_font_select_serif/sans/
 *   script/typewriter() (htmlpref.cpp) look at - identical algorithm to
 *   fcfont.cpp's fc_os2_class_to_family()/fc_spacing_to_family(), just
 *   working from an FT_Face this file already has open rather than a
 *   fontconfig pattern.
 */
int classify_family(FT_Face face)
{
    if (FT_IS_FIXED_WIDTH(face))
        return FF_MODERN;

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

    return family;
}

unsigned char *read_whole_file(const char *path, size_t *data_size)
{
    unsigned char *buffer = 0;
    FILE *fp = fopen(path, "rb");
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
    return buffer;
}

} // namespace

int os_font_family_is_present(const char *fontname, size_t len)
{
    FT_Library ft = 0;
    if (FT_Init_FreeType(&ft) != 0)
        return 0;

    int present = 0;
    DIR *dir = opendir(FONTS_DIR);
    if (dir != 0)
    {
        struct dirent *ent;
        while (!present && (ent = readdir(dir)) != 0)
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", FONTS_DIR, ent->d_name);

            FT_Face face;
            if (FT_New_Face(ft, path, 0, &face) != 0)
                continue;

            if (face->family_name != 0
                && strlen(face->family_name) == len
                && strncasecmp(face->family_name, fontname, len) == 0)
                present = 1;

            FT_Done_Face(face);
        }
        closedir(dir);
    }

    FT_Done_FreeType(ft);
    return present;
}

unsigned char *os_font_data_for_name(const char *name, int weight, int italic,
                                     int charset, size_t *data_size)
{
    if (name == 0)
        name = "";

    FT_Library ft = 0;
    if (FT_Init_FreeType(&ft) != 0)
        return 0;

    /*
     *   Pick the best-scoring face among every one whose family matches -
     *   none of the six bundled files have real bold/italic siblings today,
     *   so this almost always just finds the one plain face, but it's
     *   written to prefer an exact weight/italic match should that change.
     */
    char best_path[512] = "";
    int best_score = -1;

    DIR *dir = opendir(FONTS_DIR);
    if (dir != 0)
    {
        struct dirent *ent;
        while ((ent = readdir(dir)) != 0)
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", FONTS_DIR, ent->d_name);

            FT_Face face;
            if (FT_New_Face(ft, path, 0, &face) != 0)
                continue;

            if (face->family_name != 0 && strcasecmp(face->family_name, name) == 0)
            {
                int is_bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
                int is_italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;

                int score = 0;
                if (is_bold == (weight >= 700 /* FW_BOLD */))
                    score += 2;
                if (is_italic == (italic != 0))
                    score += 1;

                if (score > best_score)
                {
                    best_score = score;
                    strncpy(best_path, path, sizeof(best_path) - 1);
                    best_path[sizeof(best_path) - 1] = '\0';
                }
            }

            FT_Done_Face(face);
        }
        closedir(dir);
    }

    FT_Done_FreeType(ft);

    return best_path[0] != '\0' ? read_whole_file(best_path, data_size) : 0;
}

void os_enum_font_families(unsigned int charset_id, FONTENUMPROC callback,
                           LPARAM lparam)
{
    FT_Library ft = 0;
    if (FT_Init_FreeType(&ft) != 0)
        return;

    DIR *dir = opendir(FONTS_DIR);
    if (dir == 0)
    {
        FT_Done_FreeType(ft);
        return;
    }

    /* dedupe by family name, same reason as fcfont.cpp's own enumerator */
    enum { MAX_SEEN = 64, NAME_LEN = 64 };
    char seen[MAX_SEEN][NAME_LEN];
    int seen_count = 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != 0)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", FONTS_DIR, ent->d_name);

        FT_Face face;
        if (FT_New_Face(ft, path, 0, &face) != 0)
            continue;

        if (face->family_name == 0)
        {
            FT_Done_Face(face);
            continue;
        }

        int dup = 0;
        for (int i = 0 ; i < seen_count ; ++i)
        {
            if (strcasecmp(seen[i], face->family_name) == 0)
            {
                dup = 1;
                break;
            }
        }

        if (dup)
        {
            FT_Done_Face(face);
            continue;
        }

        if (seen_count < MAX_SEEN)
        {
            strncpy(seen[seen_count], face->family_name, NAME_LEN - 1);
            seen[seen_count][NAME_LEN - 1] = '\0';
            ++seen_count;
        }

        ENUMLOGFONTEX elf;
        memset(&elf, 0, sizeof(elf));
        strncpy(elf.elfLogFont.lfFaceName, face->family_name,
                sizeof(elf.elfLogFont.lfFaceName) - 1);

        int fam_family = classify_family(face);
        elf.elfLogFont.lfPitchAndFamily = (BYTE)fam_family;

        NEWTEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        tm.tmCharSet = (BYTE)charset_id;
        if (fam_family != FF_MODERN)
            tm.tmPitchAndFamily |= TMPF_FIXED_PITCH;

        int cont = callback(&elf, &tm, 0, lparam);
        FT_Done_Face(face);

        if (!cont)
            break;
    }

    closedir(dir);
    FT_Done_FreeType(ft);
}
