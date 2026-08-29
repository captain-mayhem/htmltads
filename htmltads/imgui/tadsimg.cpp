#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsimg.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsimg.cpp - TADS image base class
Function
  
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#include <imgui/imgui_internal.h>

/* some versions of the win sdk don't have this defined yet */
#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA 1                                     /* from wingdi.h */
#endif


/*
 *   statics 
 */
BOOL (WINAPI *CTadsImage::alphablend_proc_)
    (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION) = 0;
int CTadsImage::linked_alphablend_proc_ = FALSE;

/*
 *   create 
 */
CTadsImage::CTadsImage()
{
    /* no pixel buffer yet */
    pix_ = 0;

    /* no mask buffer yet */
    mask_ = 0;

    /* no DIB section yet */
    dibsect_ = 0;

    /* no dimensions yet */
    width_ = 0;
    height_ = 0;

    /* presume we'll use the current display's bits per pixel */
    bpp_ = 0;

    /* presume we won't have alpha information */
    has_alpha_ = FALSE;

    m_texture = 0;
}

/*
 *   destroy 
 */
CTadsImage::~CTadsImage()
{
    /* delete any existing image */
    delete_image();
}

/*
 *   delete the image 
 */
void CTadsImage::delete_image()
{
    /* if we have a DIB section, delete it */
    if (dibsect_ != 0)
        DeleteObject(dibsect_);

    /* if we have a mask buffer, delete it */
    if (mask_ != 0)
        os_free_huge(mask_);

    /* clear members */
    dibsect_ = 0;
    pix_ = 0;
    width_ = 0;
    height_ = 0;

    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

/*
 *   Draw the image.
 *
 *   The image lives in an OpenGL texture (see create_texture()); we render it
 *   through ImGui.  'pos' is the destination rectangle in the current ImGui
 *   window's local coordinate space.  The legacy GDI blit path (CreateDC /
 *   StretchBlt / AlphaBlend / a 1bpp AND-mask) has been removed - guit3 has no
 *   HDC to draw into.
 */
void CTadsImage::draw(CTadsWin *win, CHtmlRect *pos,
                      htmlimg_draw_mode_t mode)
{
    /* nothing to draw without an ImGui window to draw into */
    ImGuiContext *ctx = ImGui::GetCurrentContext();
    if (ctx == nullptr || ctx->CurrentWindow == nullptr
        || width_ == 0 || height_ == 0)
        return;

    /*
     *   Make sure the texture exists.  It's normally created when the image
     *   is decoded, but that can happen before the OpenGL context is ready
     *   (e.g. resources loaded during startup), so create it here on first
     *   use as a fallback.
     */
    if (m_texture == 0 && pix_ != 0)
        create_texture();
    if (m_texture == 0)
        return;

    long dst_wid = pos->right - pos->left;
    long dst_ht = pos->bottom - pos->top;
    if (dst_wid <= 0 || dst_ht <= 0)
        return;

    /*
     *   The texture is stored top-down with straight alpha, so the natural UV
     *   range (0,0)-(1,1) maps the whole image over the destination rect,
     *   which is exactly what HTMLIMG_DRAW_STRETCH wants.
     */
    ImVec2 draw_size((float)dst_wid, (float)dst_ht);
    ImVec2 uv0(0.0f, 0.0f), uv1(1.0f, 1.0f);

    switch (mode)
    {
    case HTMLIMG_DRAW_CLIP:
        /*
         *   draw at native size, aligned at the top left of the destination
         *   area, clipping (never scaling) if the image is larger
         */
        {
            long wid = (long)width_ < dst_wid ? (long)width_ : dst_wid;
            long ht = (long)height_ < dst_ht ? (long)height_ : dst_ht;
            draw_size = ImVec2((float)wid, (float)ht);
            uv1.x = (float)wid / (float)width_;
            uv1.y = (float)ht / (float)height_;
        }
        break;

    case HTMLIMG_DRAW_STRETCH:
        /* stretch to fill the destination area - handled by the defaults */
        break;

    case HTMLIMG_DRAW_TILE:
        /*
         *   repeat the image as many times as needed to fill the destination
         *   area; GL_REPEAT wrapping on the texture turns a >1 UV range into
         *   tiling for us
         */
        uv1.x = (float)dst_wid / (float)width_;
        uv1.y = (float)dst_ht / (float)height_;
        break;
    }

    ImGui::SetCursorPos(ImVec2((float)pos->left, (float)pos->top));
    ImGui::Image((ImTextureID)(intptr_t)m_texture, draw_size, uv0, uv1);
}

/*
 *   Upload the decoded pixel buffer to an OpenGL texture.
 *
 *   pix_ is a Windows-DIB-style buffer: rows run bottom-up, each row is padded
 *   to a 4-byte boundary, and pixels are in BGR order (24bpp) or pre-multiplied
 *   BGRA order (32bpp - the decoders pre-multiply by alpha for the old
 *   AlphaBlend path).  We convert to a top-down, straight-alpha RGBA image
 *   before uploading: the Microsoft OpenGL 1.1 headers don't reliably expose
 *   GL_BGRA, and ImGui's blend function expects straight (non-pre-multiplied)
 *   alpha.  This matches the conversion the toolbar-icon loader in htmlgui.cpp
 *   already does.
 */
void CTadsImage::create_texture()
{
    int bpp = (bpp_ != 0 ? bpp_ : 24);

    /* we only know how to handle 24- and 32-bit pixel buffers */
    if (pix_ == 0 || width_ == 0 || height_ == 0 || (bpp != 24 && bpp != 32))
    {
        /* drop any stale texture so we don't render garbage */
        if (m_texture != 0)
        {
            glDeleteTextures(1, &m_texture);
            m_texture = 0;
        }
        return;
    }

    int src_bpp = bpp / 8;
    unsigned long src_stride = ((width_ * (unsigned long)bpp + 31) / 32) * 4;

    /* build a top-down, straight-alpha RGBA copy */
    unsigned char *rgba =
        new unsigned char[(size_t)width_ * (size_t)height_ * 4];
    for (unsigned long y = 0 ; y < height_ ; ++y)
    {
        const unsigned char *srcp = (const unsigned char *)os_add_huge(
            pix_, (unsigned long)(height_ - 1 - y) * src_stride);
        unsigned char *dstp = rgba + (size_t)y * (size_t)width_ * 4;
        for (unsigned long x = 0 ; x < width_ ;
             ++x, srcp += src_bpp, dstp += 4)
        {
            int b = srcp[0], g = srcp[1], r = srcp[2];
            int a = (src_bpp == 4 ? srcp[3] : 255);

            /* undo the pre-multiplication the decoder applied */
            if (src_bpp == 4 && a != 0 && a != 255)
            {
                r = r * 255 / a;
                g = g * 255 / a;
                b = b * 255 / a;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
            }

            dstp[0] = (unsigned char)r;
            dstp[1] = (unsigned char)g;
            dstp[2] = (unsigned char)b;
            dstp[3] = (unsigned char)a;
        }
    }

    /* (re-)create the GL texture, reusing the id if we already have one */
    if (m_texture == 0)
        glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    delete [] rgba;
}

/*
 *   Create a DWORD-aligned pixel buffer from an array of rows.  Each row is
 *   width_bytes bytes wide and width_pix pixels wide, which implies the
 *   number of bytes per pixel.  src_rows points to an array of pointers,
 *   where each pointer points to an array of bytes for a row.  
 */
int CTadsImage::create_pix_dword_aligned(const unsigned char *const *src_rows,
                                         unsigned long width_pix,
                                         unsigned long width_bytes,
                                         unsigned long height,
                                         int *pix_bytes_per_pixel)
{
    long out_width_bytes;
    long in_width_bytes;
    long width_bits;
    long new_size;
    unsigned long row;
    OS_HUGEPTR(unsigned char) dst;
    const unsigned char *const *src;
    int in_bytes_per_pixel;
    int out_bytes_per_pixel;

    /* store the width and height */
    width_ = width_pix;
    height_ = height;

    /* if there are no rows, we can't do anything */
    if (src_rows == 0)
        return 1;

    /* the width in bytes of the input is the same as the in-memory image */
    in_width_bytes = width_bytes;

    /* figure the number of bytes per pixel of the input */
    in_bytes_per_pixel = in_width_bytes / width_pix;

    /* 
     *   keep the alpha channel in the output if (a) we have alpha in the
     *   input (indicated by 32 bits == 4 bytes per pixel), and (b) we have
     *   support on this version of Windows for the AlphaBlend API 
     */
    has_alpha_ = (in_bytes_per_pixel >= 4 && get_alphablend_proc() != 0);

    /* 
     *   Figure the bytes per pixel of the output:
     *   
     *   - if we have an alpha channel and 4 or more bytes per input pixel,
     *   use 4 bytes per output pixel (RGBA format)
     *   
     *   - if we have no alpha channel, use the same number of bytes per
     *   pixel as the input, but no more than 3 (for RGB format) 
     */
    out_bytes_per_pixel = in_bytes_per_pixel;
    if (has_alpha_ && out_bytes_per_pixel > 4)
        out_bytes_per_pixel = 4;
    else if (!has_alpha_ && out_bytes_per_pixel > 3)
        out_bytes_per_pixel = 3;

    /* tell the caller the bytes per pixel in the output image */
    *pix_bytes_per_pixel = out_bytes_per_pixel;

    /* remember the bit depth of the bitmap data we're storing */
    bpp_ = out_bytes_per_pixel * 8;

    /* calculate the smallest DWORD-aligned buffer we can use */
    width_bits = width_pix * out_bytes_per_pixel * 8;
    out_width_bytes = ((width_bits + 31) / 32) * 4;

    /* calculate the size we'll need for the DWORD-aligned buffer */
    new_size = (unsigned long)out_width_bytes * (unsigned long)height;

    /* allocate our DIB section, where we'll stash our pixel data */
    if (alloc_dib())
        return 1;

    /* copy the rows */
    dst = os_add_huge(pix_, out_width_bytes * (height - 1));
    if (in_bytes_per_pixel == out_bytes_per_pixel)
    {
        /* check for 32-bit-per-pixel with alpha */
        if (has_alpha_ && in_bytes_per_pixel == 4)
        {
            /* 
             *   We have a 32bpp image on both input and output, but we have
             *   an alpha channel that we're going to use.  For Windows
             *   rendering, we have to pre-multiply the source pixels by the
             *   alpha channel value.  
             */
            for (src = src_rows, row = 0 ; row < height ; ++row, ++src)
            {
                unsigned long i;
                const unsigned char *srcp;
                unsigned char *dstp;

                /* copy each 32-bit pixel */
                for (i = 0, srcp = (const unsigned char *)*src, dstp = dst ;
                     i < width_pix ; ++i)
                {
                    /* get this pixel's source alpha value (0-255 range) */
                    unsigned int alpha = (unsigned char)*(srcp + 3);

                    /* scale each component by the source alpha */
                    *dstp++ = (unsigned char)(((int)(*srcp++) * alpha)/255);
                    *dstp++ = (unsigned char)(((int)(*srcp++) * alpha)/255);
                    *dstp++ = (unsigned char)(((int)(*srcp++) * alpha)/255);

                    /* copy the alpha */
                    *dstp++ = *srcp++;
                }

                /* move to next row of output */
                dst = os_add_huge(dst, -out_width_bytes);
            }
        }
        else
        {
            /* 
             *   the pixels are the same size, and we have no alpha, so we
             *   can simply copy a row at a time 
             */
            for (src = src_rows, row = 0 ; row < height ; ++row, ++src)
            {
                /* copy this row */
                memcpy(dst, *src, in_bytes_per_pixel * width_pix);

                /* move to next row of output */
                dst = os_add_huge(dst, -out_width_bytes);
            }
        }
    }
    else
    {
        /* 
         *   we need to adjust the size of each pixel, so copy only one pixel
         *   at a time 
         */
        for (src = src_rows, row = 0 ; row < height ; ++row, ++src)
        {
            unsigned long i;
            const unsigned char *srcp;
            unsigned char *dstp;

            /* copy this row */
            for (i = 0, srcp = (const unsigned char *)*src, dstp = dst ;
                 i < width_pix ;
                 ++i, srcp += in_bytes_per_pixel, dstp += out_bytes_per_pixel)
            {
                /* copy this pixel */
                memcpy(dstp, srcp, out_bytes_per_pixel);
            }
            
            /* move to next row */
            dst = os_add_huge(dst, -out_width_bytes);
        }
    }

    /* remember the byte width */
    width_bytes_ = out_width_bytes;

    /* upload the decoded pixels to an OpenGL texture for rendering */
    create_texture();

    /* success */
    return 0;
}

/*
 *   Allocate our DIB section.  This creates the DIB section object and
 *   allocates the memory for our pixels (storing a pointer to the pixel
 *   memory in pix_).  
 */
int CTadsImage::alloc_dib()
{
    BITMAPINFO bmi;
    HDC deskdc;

    /* fill in the bitmap info header */
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;
    bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;

    /* 
     *   set the bits per pixel to the setting stored in the image - if bpp_
     *   is zero, though, assume we're using 24-bit RGB 
     */
    bmi.bmiHeader.biBitCount = (WORD)(bpp_ != 0 ? bpp_ : 24);

    /* get the desktop DC */
    deskdc = GetDC(GetDesktopWindow());

    /* create the DIB section */
    dibsect_ = CreateDIBSection(deskdc, &bmi, DIB_RGB_COLORS,
                                (void **)&pix_, 0, 0);

    /* done with the desktop DC */
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* indicate failure if the DIB handle is null */
    return (dibsect_ == 0);
}

/*
 *   Get the address of the Win32 API function AlphaBlend(), if it's
 *   available.  If we haven't dynamically linked to the routine yet, we'll
 *   do so now.  Returns null if the procedure isn't available.  
 */
BOOL (WINAPI *CTadsImage::get_alphablend_proc())
    (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION)
{
    HMODULE dll;

    /* 
     *   if we've already linked to the routine (or tried), return the
     *   address we found previously 
     */
    if (linked_alphablend_proc_)
        return alphablend_proc_;

    /* note that we've now linked to the routine (or at least tried) */
    linked_alphablend_proc_ = TRUE;

    /*
     *   Check to see if this is Windows 98.  If this is Win98, the
     *   AlphaBlend function will be available, but will be unusable because
     *   it's too buggy.  (In particular, the Win98 AlphaBlend routine is
     *   unable to accept negative destination coordinates, and completely
     *   ignores source coordinates.  This makes it impossible to perform
     *   necessary translations during drawing; for example, it makes it
     *   impossible to draw an image whose top is slightly above the top of
     *   the window due to scrolling of the page.)
     *   
     *   Since we can't use AlphaBlend on Win98, note the OS version, and if
     *   it is indeed Win98, don't even bother looking to see if AlphaBlend
     *   is present, and simply indicate that alpha blending is not
     *   available.  
     */
    if (CTadsApp::get_app()->is_win98())
    {
        /* we're on Win98 - do not use AlphaBlend */
        alphablend_proc_ = 0;
        return 0;
    }

    /* load the DLL containing AlphaBlend */
    dll = LoadLibrary("Msimg32.dll");

    /* if we found the library, try getting the AlphaBlend address */
    if (dll != 0)
        alphablend_proc_ = (BOOL (WINAPI *)
                            (HDC, int, int, int, int,
                             HDC, int, int, int, int, BLENDFUNCTION))
                           GetProcAddress(dll, "AlphaBlend");

    /* return the AlphaBlend address, if we got it */
    return alphablend_proc_;
}

