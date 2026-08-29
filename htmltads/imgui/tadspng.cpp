#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadspng.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadspng.cpp - Win32 PNG object implementation
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
#ifndef HTMLPNG_H
#include "htmlpng.h"
#endif
#ifndef TADSPNG_H
#include "tadspng.h"
#endif


CTadsPng::CTadsPng()
{
}

CTadsPng::~CTadsPng()
{
}

/*
 *   Load the image from a PNG object
 */
int CTadsPng::load_from_png(CHtmlPng *png)
{
    int pix_bytes_per_pixel;

    /* drop any previously loaded image */
    delete_image();

    /*
     *   Create a DWORD-aligned pixel buffer from the PNG rows and upload it to
     *   an OpenGL texture (create_pix_dword_aligned() calls create_texture()
     *   for us).  Any transparency in the PNG comes through as a real alpha
     *   channel that create_texture() preserves - the old 1bpp AND-mask path
     *   (create_mask()) only existed for Windows versions without AlphaBlend
     *   and has no equivalent, or need, in the OpenGL renderer.
     */
    if (create_pix_dword_aligned(png->get_rows(),
                                 png->get_width(),
                                 png->get_row_bytes(),
                                 png->get_height(),
                                 &pix_bytes_per_pixel))
        return 1;

    /* success */
    return 0;
}

/*
 *   Initialize alpha channel support
 */
void CTadsPng::init_alpha_support()
{
    /* 
     *   check for availability of the AlphaBlend function - if it's
     *   available, we can support alpha blending, otherwise we cannot 
     */
    if (get_alphablend_proc() == 0)
    {
        /* 
         *   Alpha blending isn't available on this version of Windows.  Tell
         *   the PNG loader to discard alpha channel information by blending
         *   the alpha explicitly during loading into a default background.  
         */
        CHtmlPng::set_options(CHtmlPng::get_options() | HTMLPNG_OPT_NO_ALPHA);
    }
}

