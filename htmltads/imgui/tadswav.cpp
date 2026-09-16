#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadswav.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadswav.cpp - TADS WAV (waveform audio) file support
Function
  
Notes
  
Modified
  01/19/98 MJRoberts  - Creation
*/

#include "tadsplat.h"

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#endif

/* include TADS OS layer for I/O functions */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif
#ifndef TADSWAV_H
#include "tadswav.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Wave file decoder
 */

/*
 *   construction 
 */
CWavW32::CWavW32(
    const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
    class CTadsAudioControl *ctl,
    void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsCompressedAudio(fname, file_start_ofs, file_size, ctl,
                           done_func, done_func_ctx)
{
}

/*
 *   Get the track length 
 */
long CWavW32::get_track_len_ms()
{
    /* presume we won't be able to figure out the play time */
    long ms = 0;

    /* open a separate handle to the file, to avoid thread interference */
    osfildef *fp = osfoprb(fname_.get(), OSFTBIN);
    if (fp == 0)
        return 0;

    /* seek to the start of the sound, in case it's an embedded resource */
    osfseek(fp, in_file_start_, OSFSK_SET);

    /* read the header */
    tads_wav_hdr_info hdr;
    if (!read_header(fp, &hdr) && hdr.have_fmt_)
    {
        /*
         *   The play time is the data length (in bytes) divided by the
         *   average data rate (in bytes per second).  If the average data
         *   rate in the header is zero, and the format is PCM, we can
         *   calculate the average data rate as the samples per second times
         *   the block alignment; for other formats we can't compute this,
         *   since the data could be compressed.
         */
        DWORD avg_rate = hdr.fmt_.avg_bytes_per_sec;
        if (avg_rate == 0 && hdr.fmt_.format_tag == TADS_WAVE_FORMAT_PCM)
        {
            /* it's PCM, so calculate the average based on the sample rate */
            avg_rate = hdr.fmt_.samples_per_sec * hdr.fmt_.block_align;
        }

        /* if we have an average data rate, figure the play time */
        if (avg_rate != 0)
        {
            /* figure the time in seconds as a double */
            double s = (double)hdr.data_len_ / (double)avg_rate;

            /* convert to milliseconds and truncate to integer */
            ms = (long)(s * 1000.0);
        }
    }

    /* done with the file */
    osfcls(fp);

    /* return the time in millisecond */
    return ms;
}

/*
 *   Decode the file
 */
void CWavW32::do_decoding(osfildef *fp, DWORD file_size)
{
    int repeats_done;

    /* not stopping yet */
    stop_flag_ = FALSE;

    /*
     *   read the header - if that fails, or we can't find the format
     *   information, we can't read the file
     */
    if (read_header(fp, &hdr_)
        || !hdr_.have_fmt_
        || !hdr_.found_data_
        || !hdr_.found_header_
        || hdr_.fmt_.bits_per_sample == 0)
    {
        // $$$ should probably report the error

        /* nothing more to do */
        goto done;
    }

    /* open the playback buffer */
    open_playback_buffer(hdr_.fmt_.samples_per_sec,
                         hdr_.fmt_.bits_per_sample,
                         hdr_.fmt_.channels);

    /* start at the beginning of the file */
    data_read_ofs_ = 0;

    /* read data and play it back until we run out of data */
    for (repeats_done = 0 ; !stop_flag_ && repeats_done < 1 ; )
    {
        unsigned long actual;

        /* read bytes into our buffer */
        read_data(fp, pcmbuf_, sizeof(pcmbuf_), &actual,
                  1, &repeats_done);

        /* if we didn't get any data, we've reached the end */
        if (actual == 0)
            break;

        /* write the data to our playback buffer */
        write_playback_buffer(pcmbuf_, actual);
    }

done: ;
}

/*
 *   Read a WAV file header.  Before calling this, seek to the point in the
 *   file where the WAV data begins.  This parses the RIFF header and fills in
 *   the tads_wav_format fields, using explicit little-endian reads rather than
 *   a packed-struct overlay.  Returns zero on success, non-zero on failure.
 */
int CWavW32::read_header(osfildef *fp, tads_wav_hdr_info *hdr)
{
    unsigned char buf[16];
    unsigned long chunklen;
    unsigned long fpos, endpos;

    /* read the RIFF header (12 bytes: "RIFF" + 4-byte length + "WAVE") */
    if (osfrb(fp, buf, 12))
        return 1;

    /*
     *   check the header - make sure it's a RIFF file whose first chunk
     *   is WAVE
     */
    if (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
        return 2;

    /* note our current location */
    fpos = (unsigned long)osfpos(fp);

    /*
     *   Get the length of the RIFF chunk from buf+4 - a 4-byte little-endian
     *   value (osrp4 reads it portably).  From the length, calculate the
     *   ending location; subtract 4 for the "WAVE" tag already consumed.
     */
    chunklen = osrp4(buf + 4);
    endpos = fpos + chunklen - 4;

    /* presume we won't find the subchunks */
    hdr->found_data_ = hdr->found_header_ = FALSE;

    /*
     *   Scan the WAVE chunk for subchunks.  It should have two: one
     *   labelled "fmt " with the WAV header, and one labelled "data" with
     *   the PCM byte stream.  We want to read the "fmt " header and
     *   remember the location of the "data" chunk so that we can stream
     *   it in for playback later.
     */
    while (fpos < endpos)
    {
        unsigned long sublen;

        /* read the next chunk header (4-byte tag plus 4-byte LE length) */
        if (osfrb(fp, buf, 8))
            return 0;

        /* get the subchunk length */
        sublen = osrp4(buf + 4);

        /* note the change in file position */
        fpos += 8;

        /* see what we have */
        if (memcmp(buf, "fmt ", 4) == 0)
        {
            unsigned char fmtbuf[16];

            /* this is the header chunk */
            hdr->found_header_ = TRUE;

            /*
             *   The common part of a "fmt " chunk is 16 bytes (what Win32
             *   calls PCMWAVEFORMAT).  Non-PCM formats add a 2-byte cbSize
             *   and cbSize more bytes of format-specific data after that;
             *   nothing here uses those, and the seek to the next subchunk
             *   at the bottom of the loop skips over them.
             */
            if (sublen < 16)
                return 0;

            if (osfrb(fp, fmtbuf, 16))
                return 0;

            /* pull out the fields with explicit little-endian reads */
            hdr->fmt_.format_tag        = (unsigned short)osrp2(fmtbuf + 0);
            hdr->fmt_.channels          = (unsigned short)osrp2(fmtbuf + 2);
            hdr->fmt_.samples_per_sec   = osrp4(fmtbuf + 4);
            hdr->fmt_.avg_bytes_per_sec = osrp4(fmtbuf + 8);
            hdr->fmt_.block_align       = (unsigned short)osrp2(fmtbuf + 12);
            hdr->fmt_.bits_per_sample   = (unsigned short)osrp2(fmtbuf + 14);
            hdr->have_fmt_ = TRUE;
        }
        else if (memcmp(buf, "data", 4) == 0)
        {
            /* this is the data chunk - note its location */
            hdr->found_data_ = TRUE;
            hdr->data_fpos_ = fpos;
            hdr->data_len_ = sublen;
        }
        else
        {
            /* ignore other subchunk types */
        }

        /* seek to the start of the next subchunk (chunks are word-aligned) */
        fpos += ((sublen + 1) & ~1);
        osfseek(fp, fpos, OSFSK_SET);
    }

    /* if we didn't find either chunk, it's not a valid file */
    if (!hdr->found_data_ || !hdr->found_header_)
        return 5;

    /* success */
    return 0;
}

/*
 *   Read from the data section
 */
int CWavW32::read_data(osfildef *fp, char *buf,
                       unsigned long bytes_to_read,
                       unsigned long *bytes_read,
                       int repeat, int *repeats_done)
{
    unsigned long actual_read;
    unsigned long avail;
    unsigned char *cur_buf;

    /* seek to the next read position */
    if (osfseek(fp, hdr_.data_fpos_ + data_read_ofs_, OSFSK_SET))
        return 1;

    /*
     *   Figure out how many bytes are available to be read.  We can read
     *   the amount remaining on this iteration, plus the full data size
     *   times the number of additional iterations remaining.  If we're
     *   repeating indefinitely, we can read any amount. 
     */
    if (repeat == 0)
    {
        /* repeating forever - we can read as much as they want */
        avail = bytes_to_read;
    }
    else
    {
        /* start off with the amount left on the current iteration */
        avail = hdr_.data_len_ - data_read_ofs_;

        /* add additional iterations, if any are available */
        if (repeat > *repeats_done)
            avail += hdr_.data_len_ * (repeat - *repeats_done - 1);
    }
    
    /* 
     *   Determine if we can satisfy the request, based on how many bytes
     *   we have remaining in the chunk.  If we can't satisfy the request,
     *   reduce the number of bytes to read to the number actually
     *   remaining, and load the bytes into the end of the buffer.  We
     *   load into the end of the buffer because of the way direct sound
     *   notifications work: we can only set up a notification at certain
     *   fixed points, so we need to align the end of the file against one
     *   of these fixed points.  The buffer that was passed in ends at one
     *   of these notification points, so put our data at the end of the
     *   caller's buffer.  
     */
    if (bytes_to_read > avail)
    {
        /* shorten the amount to read by the actual amount remaining */
        bytes_to_read = avail;
    }

    /* 
     *   Read the file.  First read as much as we have left on the current
     *   iteration, then loop back to the start of the file and read from
     *   the beginning until we satisfy the request, looping back again
     *   and again if necessary. 
     */
    for (cur_buf = (unsigned char *)buf, actual_read = 0 ;
         bytes_to_read != 0 ; )
    {
        unsigned long cur_len;

        /* read up to the amount left on the current iteration */
        cur_len = bytes_to_read;
        if (cur_len > hdr_.data_len_ - data_read_ofs_)
            cur_len = hdr_.data_len_ - data_read_ofs_;

        /* read this block - a short read here is a failure */
        if (osfrbc(fp, cur_buf, cur_len) != cur_len)
            return 1;

        /* add this read into the total */
        actual_read += cur_len;

        /* advance the read offset */
        data_read_ofs_ += cur_len;

        /* advance the buffer pointer */
        cur_buf += cur_len;

        /* decrement the amount left to read */
        bytes_to_read -= cur_len;

        /* if we've exhausted the file, count the completed iteration */
        if (data_read_ofs_ == hdr_.data_len_)
            ++(*repeats_done);

        /*
         *   if there's anything left to read, wrap to the start of the
         *   data -- the only way we will need to read in multiple blocks
         *   in this loop is if we need to loop back to the start of the
         *   file, so do so here
         */
        if (bytes_to_read != 0)
        {
            /* reset to the start of the data stream */
            data_read_ofs_ = 0;
            if (osfseek(fp, hdr_.data_fpos_, OSFSK_SET))
                return 1;
        }
    }

    /* set the amount we read for the caller's information */
    *bytes_read = actual_read;

    /* success */
    return 0;
}


#if 0
/* ------------------------------------------------------------------------ */
/*
 *   Test section 
 */

static void errexit(const char *msg)
{
    printf("error: %s\n", msg);
    exit(1);
}

int main()
{
    osfildef *fp;
    CWavW32 *reader;
    int err;

    /* open a file */
    fp = osfoprb("c:\\win95\\msremind.wav", OSFTBIN);
    if (fp == 0)
        errexit("can't open file");

    /* create a reader */
    reader = new CWavW32();

    /* read the header */
    if ((err = reader->read_header(fp)) != 0)
    {
        char buf[128];
        sprintf(buf, "return code %d reading header", err);
        errexit(buf);
    }

    /* done */
    delete reader;
    osfcls(fp);
    return 0;
}

void oshtml_dbg_printf(const char *, ...)
{
}


#endif
