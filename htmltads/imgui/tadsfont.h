/* $Header: d:/cvsroot/tads/html/win32/tadsfont.h,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsfont.h - TADS font class for 32-bit windows
Function
  
Notes
  
Modified
  09/20/97 MJRoberts  - Creation
*/

#ifndef TADSFONT_H
#define TADSFONT_H

#include <imgui/imgui.h>
#include "tadsplat.h"

/*
 *   Platform hook: determine whether a font family with the given name is
 *   installed on the system.  This is the one piece of font handling that
 *   can't be done through FreeType/ImGui - system font enumeration is
 *   inherently OS-specific (GDI on Windows, fontconfig on Linux, CoreText
 *   on macOS) - so it's factored out behind this narrow interface instead
 *   of being inlined into CTadsFont.  One implementation per OS/GUI backend,
 *   selected by CMake: guifont_w32.cpp (Win32, EnumFontFamiliesEx),
 *   fcfont.cpp (Linux, fontconfig's FcFontList), ctfont.cpp (macOS,
 *   CTFontManagerCopyAvailableFontFamilyNames) - see migration.md 5.4/G; the
 *   non-Windows backends are landed but unverified until there's a real
 *   non-Windows build to run them on (M4).  CTadsFont::font_is_present() is
 *   the stable, OS-agnostic entry point callers should use - it just
 *   forwards here.
 */
int os_font_family_is_present(const char *fontname, size_t len);

/*
 *   Platform hook: resolve a font family name (with the given weight, italic
 *   flag and character set) to the raw bytes of the TrueType/OpenType file
 *   the system's font matcher picks for it.  FreeType can rasterize a font
 *   file but has no way to *find* one from a name, and neither does ImGui -
 *   that matching is inherently OS-specific (GDI's CreateFontIndirect() +
 *   GetFontData() on Windows, fontconfig's FcFontMatch on Linux, CoreText's
 *   font URL on macOS) - so, like os_font_family_is_present(), it is
 *   factored out behind this narrow interface.  One implementation per
 *   OS/GUI backend, same three files as os_font_family_is_present() above.
 *   It is called once from CTadsFont's constructor.
 *
 *   On success, returns a newly allocated buffer holding the complete font
 *   file and stores its length in *data_size.  The buffer is allocated with
 *   ImGui::MemAlloc(): hand it straight to ImFontAtlas::AddFontFromMemoryTTF()
 *   (which takes ownership and frees it with the atlas) or, if it is not
 *   used, release it with ImGui::MemFree().  Returns null if the face has no
 *   scalable outline data (e.g. the legacy "System" bitmap pseudo-font) or
 *   cannot be resolved at all - the caller then leaves the font unbaked and
 *   falls back to the atlas default.
 */
unsigned char *os_font_data_for_name(const char *name, int weight, int italic,
                                     int charset, size_t *data_size);

/*
 *   Extended logical font.  We include attributes that we use for rendering,
 *   such as color and superscript, that aren't in a standard windows LOGFONT
 *   structure.  This ensures that we create a unique system font handle for
 *   each extended attribute combination, which means that we won't confuse
 *   one cached font object for another that differs only in, say, color.  
 */
struct CTadsLOGFONT
{
    /* standard windows logical font structure */
    LOGFONT lf;

    /* windows code page ID */
    UINT codepage;

    /* flag: face name is set explicitly in LOGFONT */
    int face_set_explicitly : 1;

    /* flag: color is set explicitly in LOGFONT */
    int color_set_explicitly : 1;

    /* flag: color is the dynamic command-input color */
    int color_is_input : 1;

    /* 
     *   flag: we have a background color (if not, we draw transparently on
     *   the existing background) 
     */
    int bgcolor_set : 1;

    /* extended attributes */
    int superscript : 1;
    int subscript : 1;
    COLORREF color;
    COLORREF bgcolor;
};

class CTadsFont
{
public:
    CTadsFont(const CTadsLOGFONT *logfont);
    virtual ~CTadsFont();

    /*
     *   select this font into a DC - returns the old font object, which
     *   should be popped with unselect when the caller is done with this
     *   font
     */
    HGDIOBJ select(HDC dc);

    /*
     *   Push this font's ImGui font onto the ImGui font stack WITHOUT
     *   touching a GDI DC.  This is the DC-free half of select(): callers
     *   that only need the right font active for ImGui measuring/drawing
     *   (never GDI) use this and balance it with ImGui::PopFont().
     */
    void push_imgui_font();

    /* restore the previous font to a DC */
    void unselect(HDC dc, HGDIOBJ oldfont);

    /* check if I exactly match a logical font description */
    int matches(const CTadsLOGFONT *logfont);

    /* calculate a setting for LOGFONT.lfHeight, given a point size */
    static long calc_lfHeight(int pointsize);

    /* calculate the point size for a given pixel height */
    static int calc_pointsize(int pix_height);

    /*
     *   Get the screen's logical DPI (pixels per inch), for converting
     *   between point sizes and pixel sizes.  This is the GLFW/ImGui
     *   equivalent of querying LOGPIXELSY from the desktop DC: ImGui
     *   renders at a fixed baseline of 96 DPI ("100% scaling"), with the
     *   primary monitor's content scale factor accounting for the rest.
     */
    static float get_screen_dpi();

    /*
     *   The primary monitor's content scale factor on its own (1.0 at "100%"
     *   scaling, 1.5 at 150%, ...) - i.e. get_screen_dpi() / 96.  This is the
     *   single source of truth for every "scale it for the display" factor in
     *   guit3: font sizes bake it in here, and the chrome that ImGui draws
     *   itself (menu/dialog font, toolbar icons) multiplies its fixed pixel
     *   sizes by it so it doesn't come out tiny on a HiDPI display.
     */
    static float get_dpi_scale();

    /* determine if a font is present on the system */
    static int font_is_present(const char *fontname, size_t namelen);

    /* get the system font handle */
    HFONT get_handle() const { return handle_; }

protected:
    /* make sure no one uses the default constructor */
    CTadsFont();

    /* make a canonical copy of a LOGFONT structure */
    void copy_canonical_logfont(CTadsLOGFONT *dst, const CTadsLOGFONT *src);
    
    /* my font handle */
    HFONT handle_;

    ImFont* m_font;

    /* logical font description */
    CTadsLOGFONT logfont_;
};


#endif /* TADSFONT_H */

