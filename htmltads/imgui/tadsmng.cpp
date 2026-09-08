#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsmng.cpp - Win32 MNG image implementation
Function
  
Notes
  
Modified
  05/04/02 MJRoberts  - Creation
*/

#include <Windows.h>

/* 
 *   include libmng.h; note that we must define XMD_H to work around an
 *   incompatibility between <Windows.h> and one of the jpeglib headers;
 *   fortunely, the jpeglib people seem to have encountered the same sort of
 *   incompatibility compiling against x11 headers, so we can use apply their
 *   fix by making it look like we included that x11 header 
 */
#define XMD_H
#include <libmng.h>

/* include TADS OS headers - we need to use TADS OS-layer I/O routines */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef HTMLMNG_H
#include "htmlmng.h"
#endif
#ifndef TADSMNG_H
#include "tadsmng.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif


CTadsMng::CTadsMng()
{
    /* create our MNG helper object */
    mng_ = new CHtmlMng(this);
}

CTadsMng::~CTadsMng()
{
    /* delete our MNG helper object */
    delete mng_;
}


/*
 *   start reading from an MNG helper object 
 */
int CTadsMng::start_reading_mng(const textchar_t *filename,
                                unsigned long seekpos,
                                unsigned long filesize)
{
    int err;

    /* start reading from the file */
    if ((err = mng_->start_mng_read(filename, seekpos, filesize)) != 0)
        return err;

    /* success */
    return 0;
}

/*
 *   initialize the canvas 
 */
int CTadsMng::init_mng_canvas(mng_handle handle, int width, int height)
{
    /* drop any previously loaded image */
    delete_image();

    /* remember our width and height */
    width_ = width;
    height_ = height;

    /*
     *   Figure the number of bits per pixel in the canvas.  Always use 8-bit
     *   RGB format, adding an alpha channel when the image carries any
     *   transparency (at least one bit of alpha depth).
     *
     *   htmlt3 only kept the alpha channel when the Win32 AlphaBlend API was
     *   usable, and otherwise had the MNG decoder pre-composite the alpha
     *   onto a fixed light-gray background (mng_set_bgcolor()).  guit3 always
     *   blends through OpenGL, so it always keeps the real alpha channel -
     *   see migration.md section 5.4/H.
     */
    if (mng_get_alphadepth(handle) != 0)
    {
        /* transparent - 8-bit BGRA canvas with pre-multiplied alpha */
        mng_set_canvasstyle(handle, MNG_CANVAS_BGRA8PM);

        /* this is a 32 bit-per-pixel format */
        bpp_ = 32;

        /* note that we have alpha */
        has_alpha_ = TRUE;
    }
    else
    {
        /* opaque - a plain 8-bit BGR canvas is enough */
        mng_set_canvasstyle(handle, MNG_CANVAS_BGR8);
        bpp_ = 24;
    }
    
    /* figure the number of bytes per row */
    width_bytes_ = width * (bpp_ / 8);

    /* create the DIB section - this will be our canvas */
    if (alloc_dib())
        return 1;
    
    /* success */
    return 0;
}

/*
 *   Get a pointer to a row of canvas pixels 
 */
char *CTadsMng::get_mng_canvas_row(int rownum)
{
    int real_width;
    
    /* round our per-row width in bytes to a DWORD boundary */
    real_width = (width_bytes_ + 3) & ~3;

    /* 
     *   the Windows DIB is arranged from bottom to top, so get the row as an
     *   offset from the end of the DIB
     */
    return (char *)os_add_huge(pix_, real_width * (height_ - 1 - rownum));
}

