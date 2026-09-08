/* $Header: d:/cvsroot/tads/html/win32/tadswav.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadswav.h - TADS WAV (waveform audio) support
Function
  
Notes
  
Modified
  01/19/98 MJRoberts  - Creation
*/

#ifndef TADSWAV_H
#define TADSWAV_H

#include "tadsplat.h"

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSCSND_H
#include "tadscsnd.h"
#endif


/* the one WAVE format tag we treat specially (uncompressed PCM) */
#define TADS_WAVE_FORMAT_PCM  1

/*
 *   The fields of the RIFF "fmt " chunk that the decoder actually uses.  This
 *   used to be a Win32 WAVEFORMATEX read straight off disk as a packed struct;
 *   it is now filled in field by field with explicit little-endian reads
 *   (osrp2/osrp4), so the code no longer depends on <mmreg.h> struct layout or
 *   host byte order.  Any format-specific extra bytes after this common
 *   header are skipped - nothing here consumes them.
 */
struct tads_wav_format
{
    unsigned short format_tag;         /* wFormatTag        */
    unsigned short channels;           /* nChannels         */
    unsigned long  samples_per_sec;    /* nSamplesPerSec    */
    unsigned long  avg_bytes_per_sec;  /* nAvgBytesPerSec   */
    unsigned short block_align;        /* nBlockAlign       */
    unsigned short bits_per_sample;    /* wBitsPerSample    */
};

/*
 *   WAV file header information
 */
struct tads_wav_hdr_info
{
    tads_wav_hdr_info()
    {
        have_fmt_ = FALSE;
        memset(&fmt_, 0, sizeof(fmt_));
    }

    /*
     *   seek position in file of start of byte stream containing the actual
     *   digitized sound data
     */
    unsigned long data_fpos_;

    /* size of the data chunk */
    unsigned long data_len_;

    /* the parsed "fmt " chunk (valid only if have_fmt_ is set) */
    tads_wav_format fmt_;
    int have_fmt_;

    /* flags: we've found the header and data chunks in the file */
    int found_header_ : 1;
    int found_data_ : 1;
};

/*
 *   Wave file decoder and playback engine
 */
class CWavW32: public CTadsCompressedAudio
{
public:
    CWavW32(const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
            class CTadsAudioControl *ctl,
            void (*done_func)(void *, int), void *done_func_ctx);

    /* get the track time in milliseconds */
    long get_track_len_ms();

    /* decode the file */
    virtual void do_decoding(osfildef *fp, DWORD file_size);

    /* get/set our 'stop' flag */
    virtual int get_decoder_stopping() { return stop_flag_; }
    void set_decoder_stopping(int f) { stop_flag_ = f; }

protected:
    /*
     *   Read a WAV file header.  This parses the RIFF header and fills in the
     *   tads_wav_format fields.  Returns zero on success, non-zero on failure.
     */
    int read_header(osfildef *fp, struct tads_wav_hdr_info *info);

    /*
     *   get a pointer to the parsed "fmt " chunk (valid only after the header
     *   has been read in successfully)
     */
    const tads_wav_format *get_wave_format() const
        { return hdr_.have_fmt_ ? &hdr_.fmt_ : 0; }

    /* seek to the start of the wave data stream */
    void seek_data_start() { data_read_ofs_ = 0; }

    /*
     *   Read from the wave data stream.  Returns zero on success, nonzero
     *   on error.  *bytes_read returns with the actual number of bytes
     *   that we read; if we reach the end of the file before the full
     *   request is satisfied, we'll return success with *bytes_read
     *   indicating that the buffer is only partially filled.
     */
    int read_data(osfildef *fp, char *buf, unsigned long bytes_to_read,
                  unsigned long *bytes_read, int repeat, int *repeats_done);

    /* get the total length of the wave data byte stream */
    unsigned long get_wave_len() const { return hdr_.data_len_; }

private:
    /* decoder stop flag */
    volatile int stop_flag_;

    /* decoding work buffer */
    char pcmbuf_[4096];

    /* header information */
    tads_wav_hdr_info hdr_;

    /*
     *   Data read offset - this is the byte offset in the data stream
     *   from which the next data read routine will start reading. 
     */
    unsigned long data_read_ofs_;
};

#endif /* TADSWAV_H */
