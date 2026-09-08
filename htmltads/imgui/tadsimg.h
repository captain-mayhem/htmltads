/* $Header: d:/cvsroot/tads/html/win32/tadsimg.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsimg.h - TADS image base class
Function
  This class serves as a base class for images stored as bitmaps.
  Subclasses are responsible for loading a specific type of image
  into our internal bitmap.
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#ifndef TADSIMG_H
#define TADSIMG_H

#include "tadsplat.h"

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif

/*
 *   base image class 
 */
class CTadsImage
{
public:
    CTadsImage();
    virtual ~CTadsImage();

    /* draw the image into a window */
    void draw(class CTadsWin *win, class CHtmlRect *pos,
              htmlimg_draw_mode_t mode);

    /* 
     *   Map the image's color palette into the system palette.  Returns
     *   zero if there was no change, non-zero if any colors changed.  
     *   
     *   We never use palettes in the image objects in the Windows
     *   implementation, because we do all of our drawing work in 24-bit RGB
     *   mode.  
     */
    int map_palette(class CTadsWin *win, int foreground) { return FALSE; }

    /*
     *   Get the display content-scale factor (1.0 at "100%" scaling, 1.5 at
     *   150%, etc.), from the same GLFW content-scale query CTadsFont uses
     *   for fonts.  Image pixel buffers have no intrinsic DPI, so - to match
     *   htmlt3, where the whole window bitmap was scaled up by the OS - every
     *   image is laid out and drawn at its pixel size times this factor, so
     *   that images track the (already content-scaled) text instead of
     *   coming out 1/scale too small on a HiDPI display.
     */
    static float disp_scale();

    /*
     *   Do we have alpha support?  Always yes in guit3: images are drawn as
     *   straight-alpha RGBA textures and the GL/ImGui blend function always
     *   applies them.  (htmlt3's answer depended on the dynamically-linked
     *   Win32 AlphaBlend API being present and non-buggy; the GDI blit path
     *   it backed is gone - see migration.md section 5.4/H.)
     */
    static int is_alpha_supported() { return TRUE; }

    /*
     *   Formerly disabled the Win32 AlphaBlend path (for hardware where it
     *   was too slow, or implementations - WineX in 2003 - where it was
     *   buggy).  guit3 composites every image through OpenGL, which always
     *   blends, so there is nothing to disable; kept as a no-op so the
     *   "-noalphablend" command-line option is still accepted and ignored.
     */
    static void disable_alpha_support() { }

protected:
    /* create the DWORD-aligned version of the image data */
    int create_pix_dword_aligned(const unsigned char *const *src_rows,
                                 unsigned long width_pix,
                                 unsigned long width_bytes,
                                 unsigned long height,
                                 int *pix_bytes_per_pixel);

    /*
     *   Allocate the pixel buffer (pix_).  The buffer keeps the classic
     *   Windows-DIB memory layout - rows bottom-up, each row padded to a
     *   4-byte boundary - because create_texture() and the decoders' row
     *   walkers still expect it; it is now just a plain os_alloc_huge()
     *   block, not a GDI DIB section (nothing blits it any more).
     */
    int alloc_dib();

    /*
     *   Upload the decoded pixel buffer (pix_) to an OpenGL texture stored in
     *   m_texture.  This is the single point where every image type (JPEG,
     *   PNG, MNG) is turned into something ImGui::Image() can render; call it
     *   once the pixels in pix_ are ready.  Safe to call repeatedly (used by
     *   MNG to refresh the texture on each animation frame).
     */
    void create_texture();

    /* delete any existing image */
    void delete_image();

    /* pixel buffer */
    OS_HUGEPTR(unsigned char) pix_;

    /* pixel buffer for the transparency mask, if present */
    OS_HUGEPTR(unsigned char) mask_;

    /* flag: we have alpha information in the bitmap */
    int has_alpha_;

    /* size of the image */
    unsigned long width_, height_;

    /* 
     *   width of each line of the image in bytes - since Windows bitmaps
     *   must be DWORD-aligned, this may differ from the pixel width 
     */
    unsigned long width_bytes_;

    /*
     *   bits per pixel - if this is zero, we assume that the stored image
     *   has been converted to the same bit depth as the active display
     */
    int bpp_;

    uint32_t m_texture;
};

#endif /* TADSIMG_H */

