#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32font.cpp,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32font.cpp - html tads win32 font implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <Windows.h>
#include <memory.h>
#include <imgui/imgui.h>
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef W32FONT_H
#include "guifont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Font implementation
 */

CHtmlSysFont_win32::CHtmlSysFont_win32(const CTadsLOGFONT *lf)
    : CTadsFont(lf)
{
    ImFontBaked *baked = get_baked();

    /*
     *   A font is fixed-pitch (monospaced) if every glyph has the same
     *   advance width; comparing two glyphs that differ widely in a
     *   proportional font ('i' vs 'M') is enough to tell.
     */
    is_fixed_pitch_ = (baked->GetCharAdvance('i') == baked->GetCharAdvance('M'));

    /* remember the 'em' size as the ascender height of the font */
    em_size_ = (int)baked->Ascent;
}

CHtmlSysFont_win32::~CHtmlSysFont_win32()
{
}

void CHtmlSysFont_win32::get_font_metrics(CHtmlFontMetrics *metrics)
{
    ImFontBaked *baked = get_baked();

    /* return the required information */
    metrics->ascender_height = (int)baked->Ascent;
    metrics->descender_height = (int)-baked->Descent;
    metrics->total_height = (int)(baked->Ascent - baked->Descent);
}

/*
 *   Get my FreeType-baked glyph metrics, replacing the old
 *   GetDC/SelectObject/GetTextMetrics GDI query - FreeType already
 *   computed the same ascent/descent/advance data while loading this
 *   font for rendering (see CTadsFont's constructor), so there's no need
 *   to ask GDI for it again.  Falls back to the atlas's default font when
 *   this font has no loaded ImFont (see CTadsFont::select()'s matching
 *   fallback and the comment there) so metrics stay consistent with what
 *   actually gets rendered, and callers never have to handle a null
 *   result.
 */
ImFontBaked *CHtmlSysFont_win32::get_baked()
{
    ImFont *font = (m_font != nullptr) ? m_font : ImGui::GetIO().Fonts->Fonts[0];
    return font->GetFontBaked(-logfont_.lf.lfHeight);
}


/* ------------------------------------------------------------------------ */
/*
 *   Win32 implementation of the os_font_family_is_present() platform hook
 *   declared in tadsfont.h.  A future non-Windows port supplies its own
 *   implementation of this same function (e.g. via fontconfig or
 *   CoreText) in its own platform file; CTadsFont::font_is_present()
 *   (tadsfont.cpp) is the OS-agnostic entry point everything else calls.
 */

namespace {

class enum_proc_ctx_t
{
public:
    enum_proc_ctx_t() { count = 0; }

    /* count of number of fonts of the family that were enumerated */
    int count;
};

int CALLBACK font_enum_proc(ENUMLOGFONTEX *, NEWTEXTMETRIC *,
                             DWORD, LPARAM lpar)
{
    enum_proc_ctx_t *ctx = (enum_proc_ctx_t *)lpar;

    /* increase the count of fonts found */
    ctx->count++;

    /* return non-zero to continue enumeration */
    return 1;
}

} // namespace

int os_font_family_is_present(const char *fontname, size_t len)
{
    HDC deskdc;
    enum_proc_ctx_t enum_proc_ctx;
    LOGFONT lf;

    /* make a null-terminated version of the font name */
    if (len > sizeof(lf.lfFaceName) - 1)
        len = sizeof(lf.lfFaceName) - 1;
    memcpy(lf.lfFaceName, fontname, len);
    lf.lfFaceName[len] = '\0';

    /* get the desktop window device context */
    deskdc = GetDC(GetDesktopWindow());

    /* set up the LOGFONT to describe the font families were interested in */
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = 0;

    /* enumerate fonts matching the given name */
    EnumFontFamiliesEx(deskdc, &lf, (FONTENUMPROC)font_enum_proc,
                       (LPARAM)&enum_proc_ctx, 0);

    /* done with the desktop dc */
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* if we enumerated any fonts, this face name exists */
    return enum_proc_ctx.count != 0;
}

