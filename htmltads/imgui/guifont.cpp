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
  This file is windows.h-free and compiles on every platform: everything
  here is FreeType/ImGui calls plus the portable LOGFONT-shaped types
  tadsplat.h defines off Windows.  The genuinely OS-specific half of font
  handling - os_font_family_is_present() / os_font_data_for_name(),
  declared in tadsfont.h - lives in one per-platform backend file instead
  (guifont_w32.cpp / fcfont.cpp / ctfont.cpp; see migration.md 5.4/G),
  selected by CMake.
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <memory.h>
#include <string.h>
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
