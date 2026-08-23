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

#include <Windows.h>
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

    /* create the system font */
    handle_ = CreateFontIndirect(&logfont->lf);
    m_font = nullptr;

    /*
     *   Extract the raw font file bytes for the system font we just
     *   created, so FreeType can rasterize it.  There's no FreeType (or
     *   other OS-agnostic) equivalent for resolving a font *name* to its
     *   underlying file data - GDI's font-matching is what finds the
     *   right font file here, via the desktop DC's currently selected
     *   font.
     */
    ImGuiIO& io = ImGui::GetIO();
    HDC deskdc = GetDC(GetDesktopWindow());
    SelectObject(deskdc, handle_);

    const DWORD size = ::GetFontData(deskdc, 0, 0, NULL, 0);
    if (size != GDI_ERROR) {
        char* buffer = (char*)ImGui::MemAlloc(size);
        if (GetFontData(deskdc, 0, 0, buffer, size) == size) {
            ImFontConfig font_cfg;
            strncpy(font_cfg.Name, logfont->lf.lfFaceName, 40);
            font_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_Bitmap;
            m_font = io.Fonts->AddFontFromMemoryTTF(buffer, (int)size, -logfont->lf.lfHeight, &font_cfg, 0);
        }
        else {
            ImGui::MemFree(buffer);
        }
    }
    /*
     *   else: this font has no scalable outline data for GetFontData() to
     *   extract - notably the "System" pseudo-font (a legacy bitmap/raster
     *   font, not a real TrueType/OpenType face), which is exactly what
     *   CreateFontIndirect() selects for the literal face name "System"
     *   (e.g. from the Customize Theme dialog's font dropdown).  Leave
     *   m_font null rather than feeding GDI_ERROR's bit pattern through as
     *   a bogus size - select()'s ImGui::PushFont(nullptr) is a documented
     *   no-op that keeps whatever font is already active, so this just
     *   falls back gracefully instead of asserting/crashing.
     */
    ReleaseDC(GetDesktopWindow(), deskdc);
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
    /*
     *   If this font has no loaded ImFont (its underlying system font -
     *   e.g. the "System" pseudo-font - had no scalable outline data for
     *   FreeType to use; see the constructor), fall back to the atlas's
     *   default font rather than pushing null.  ImGui::PushFont(nullptr)
     *   doesn't mean "use the default font" - it means "keep whatever
     *   font is currently on the context's font stack," which can itself
     *   still be null this early (e.g. during the very first HTML layout
     *   pass at startup, before any ImGui::NewFrame() has pushed
     *   anything), and it asserts rather than tolerating that.
     *   io.Fonts->Fonts[0] is always valid once past htmlgui.cpp's
     *   do_create(), which calls AddFontDefault() immediately after
     *   creating the ImGui context, long before any CTadsFont exists.
     */
    ImGui::PushFont(m_font != nullptr ? m_font : ImGui::GetIO().Fonts->Fonts[0]);
    return SelectObject(dc, handle_);
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
    return 96.0f * ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
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
