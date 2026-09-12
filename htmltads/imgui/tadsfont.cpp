#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsfont.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsfont.cpp - TADS font implementation for Win 32
Function
  
Notes
  
Modified
  09/20/97 MJRoberts  - Creation
*/

#include "tadsplat.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <GLFW/glfw3.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/misc/freetype/imgui_freetype.h>

#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/*
 *   Create a TADS font object for a given logical font description
 */
CTadsFont::CTadsFont(const CTadsLOGFONT *logfont)
{
    /* store a canonical copy of the LOGFONT for later comparison */
    copy_canonical_logfont(&logfont_, logfont);

    /*
     *   Create the system font handle.  This is still needed on Windows for
     *   the GDI text-metrics path in CHtmlSysWin_win32::measure_text()
     *   (GetTextMetrics/GetTextExtentPoint32); everything ImGui draws goes
     *   through m_font below.
     */
    handle_ = CreateFontIndirect(&logfont->lf);
    m_font = nullptr;

    /*
     *   Resolve the logical font to the raw bytes of an actual
     *   TrueType/OpenType file so FreeType has something to rasterize.
     *   There is no FreeType (or other OS-agnostic) way to turn a font
     *   *name* into font-file data - that matching is inherently OS
     *   integration - so it lives behind the os_font_data_for_name()
     *   platform hook (declared in tadsfont.h; Win32 backend in
     *   guifont.cpp, where it is the old CreateFontIndirect()+GetFontData()
     *   trick moved verbatim).
     */
    size_t data_size = 0;
    unsigned char *buffer = os_font_data_for_name(
        logfont->lf.lfFaceName, (int)logfont->lf.lfWeight,
        logfont->lf.lfItalic, logfont->lf.lfCharSet, &data_size);
    if (buffer != nullptr) {
        ImFontConfig font_cfg;
        strncpy(font_cfg.Name, logfont->lf.lfFaceName, 40);
        font_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_Bitmap;
        m_font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
            buffer, (int)data_size, -logfont->lf.lfHeight, &font_cfg, 0);
    }
    /*
     *   else: this face has no scalable outline data for the hook to
     *   extract - notably the "System" pseudo-font (a legacy bitmap/raster
     *   font, not a real TrueType/OpenType face), which is exactly what
     *   CreateFontIndirect() selects for the literal face name "System"
     *   (e.g. from the Customize Theme dialog's font dropdown).  Leave
     *   m_font null - select()/get_baked()/push_imgui_font() all fall back
     *   to the atlas default font rather than pushing null (which
     *   ImGui::PushFont() treats as "keep the current font", asserting if
     *   that is itself null).
     */
}

CTadsFont::~CTadsFont()
{
    /* we're done with the font */
    if (handle_ != 0)
    {
        DeleteObject(handle_);
        handle_ = 0;
    }
    if (m_font != nullptr && ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->RemoveFont(m_font);
    }
}

/*
 *   select the font into a device context, returning the previously
 *   selected font's handle
 */
HGDIOBJ CTadsFont::select(HDC dc)
{
    /* push the ImGui font, then select the GDI font into the DC */
    push_imgui_font();
    return SelectObject(dc, handle_);
}

/*
 *   Push this font onto the ImGui font stack, with no DC / GDI involved.
 *
 *   If this font has no loaded ImFont (its underlying system font - e.g.
 *   the "System" pseudo-font - had no scalable outline data for FreeType to
 *   use; see the constructor), fall back to the atlas's default font rather
 *   than pushing null.  ImGui::PushFont(nullptr) doesn't mean "use the
 *   default font" - it means "keep whatever font is currently on the
 *   context's font stack," which can itself still be null this early (e.g.
 *   during the very first HTML layout pass at startup, before any
 *   ImGui::NewFrame() has pushed anything), and it asserts rather than
 *   tolerating that.  io.Fonts->Fonts[0] is always valid once past
 *   htmlgui.cpp's do_create(), which calls AddFontDefault() immediately
 *   after creating the ImGui context, long before any CTadsFont exists.
 */
void CTadsFont::push_imgui_font()
{
    ImGui::PushFont(m_font != nullptr ? m_font : ImGui::GetIO().Fonts->Fonts[0]);
}

/*
 *   restore a font that was in effect before selecting this font 
 */
void CTadsFont::unselect(HDC dc, HGDIOBJ oldfont)
{
    SelectObject(dc, oldfont);
}


/*
 *   Get the screen's logical DPI.  See the declaration in tadsfont.h for
 *   why 96 is the right baseline here: it's the same convention
 *   CTadsSyswin::syswin_create_system_window() already uses
 *   (ImGui_ImplGlfw_GetContentScaleForMonitor() returns 1.0 at "100%"
 *   scaling, which GDI would have reported as 96 DPI).
 */
float CTadsFont::get_screen_dpi()
{
    return 96.0f * get_dpi_scale();
}

/*
 *   The primary monitor's content scale factor.  See tadsfont.h.  Guards
 *   against a null monitor (no display / headless) by falling back to 1.0.
 */
float CTadsFont::get_dpi_scale()
{
    GLFWmonitor *mon = glfwGetPrimaryMonitor();
    float s = (mon != 0
               ? ImGui_ImplGlfw_GetContentScaleForMonitor(mon)
               : 1.0f);
    return (s > 0.0f ? s : 1.0f);
}

/*
 *   Calculate a setting for LOGFONT.lfHeight, given a point size
 */
long CTadsFont::calc_lfHeight(int pointsize)
{
    /* one inch is 72 points - calculate how many pixels that is */
    long sz = (long)(pointsize * get_screen_dpi()) / 72;

    /* return a negative value to tell Windows to use character size */
    return -sz;
}

/*
 *   Calculate a point size for a given pixel height
 */
int CTadsFont::calc_pointsize(int pix_height)
{
    /*
     *   there are 72 points in an inch, so calculate our pixel height in
     *   inches and multiply the result by 72
     */
    return (int)(((long)pix_height * 72L) / (long)get_screen_dpi());
}

/*
 *   check if we match a logical font 
 */
int CTadsFont::matches(const CTadsLOGFONT *lf)
{
    CTadsLOGFONT canon_lf;

    /*
     *   make a private copy in canonical form, with the lfFaceName array
     *   set to nulls after the terminator 
     */
    copy_canonical_logfont(&canon_lf, lf);
    
    /* if we match the LOGFONT structure exactly, it's a match */
    return memcmp(&canon_lf, &logfont_, sizeof(logfont_)) == 0;
}

/*
 *   Determine if a font is present on the system.  The actual system
 *   font-enumeration work is OS-specific - see the os_font_family_is_present()
 *   declaration in tadsfont.h for why - so this just forwards to whichever
 *   platform backend is linked in.
 */
int CTadsFont::font_is_present(const char *fontname, size_t len)
{
    return os_font_family_is_present(fontname, len);
}

/*
 *   Make a canonical copy of a LOGFONT structure for comparison purposes
 */
void CTadsFont::copy_canonical_logfont(CTadsLOGFONT *dst,
                                       const CTadsLOGFONT *src)
{
    char *p;
    int rem;

    /* store a copy of the logfont for later comparison */
    memcpy(dst, src, sizeof(*dst));

    /*
     *   Make sure the portion of lfFaceName after the null terminator is
     *   all nulls for easy comparison later.  First, find the null
     *   terminator in the existing name array.  
     */
    for (p = dst->lf.lfFaceName, rem = sizeof(dst->lf.lfFaceName) ;
         *p != 0 && rem > 0 ; ++p, --rem);

    /* now zero out all the bytes from there to the end of the array */
    for ( ; rem > 0 ; ++p, --rem)
        *p = '\0';
}
