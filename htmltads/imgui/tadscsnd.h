/*
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadscsnd.h - TADS Compressed Sound base class
Function
  This is a base class for compressed audio datatypes, such as WAV, MPEG and
  Ogg Vorbis.  It provides the framework for playing back audio by streaming
  it from disk, through the decoder, and into the sound card.

  The output path used to be a DirectSound streaming buffer; it now goes
  through CTadsAudioDevice (tadsaudiodev.h), a small backend-neutral
  interface currently implemented on miniaudio.  The decoders themselves
  (CWavW32, CMpegAmpW32, CVorbisW32) were not changed - they still call
  open_playback_buffer() / write_playback_buffer() / close_playback_buffer()
  / halt_playback_buffer() exactly as before.  See
  htmltads/imgui/migration.md section 3.7.
Notes
  The file-reading layer still uses a Win32 file HANDLE; porting that to the
  TADS osfile API is a separate step (see the migration notes).
Modified
  04/26/02 MJRoberts  - Creation
*/

#ifndef TADSCSND_H
#define TADSCSND_H

#include <stdio.h>
#include <string.h>

#include <atomic>
#include <mutex>

#include <Windows.h>

#include "tadshtml.h"
#include "tadssnd.h"
#include "tadsaudiodev.h"


/* ------------------------------------------------------------------------ */
/*
 *   TADS Compressed Audio class.  This is designed as a mix-in class that
 *   can be multiply inherited into a class implementing a specific decoder.
 */
class CTadsCompressedAudio: public CTadsAudioPlayer
{
public:
    /* -------------------------------------------------------------------- */
    /*
     *   HTML TADS interface.  The HTML TADS compressed audio objects use
     *   this interface to control sound playback.
     */

    /* reference management */
    void AddRef()
    {
        refcnt_.fetch_add(1);
    }
    void Release()
    {
        if (refcnt_.fetch_sub(1) == 1)
            delete this;
    }

    /* play back a file */
    int play(const textchar_t *url);

    /* wait until playback is finished */
    void wait_until_done();

    /* call the 'done' callback */
    void call_done_callback();

    /* determine if we're still playing */
    int is_playing();

    /* determine if we've been stopped */
    int is_stopped() { return is_done_; }

    /* halt playback */
    void stop(int sync);

    /* get our file handle */
    HANDLE get_file_handle() const { return in_file_; }

    /*
     *   CTadsAudioPlayer implementation
     */

    /* set the volume level, as a value from 0 to 10000 */
    void set_audio_volume(int level);

    /* turn muting on or off */
    void on_mute_change(int mute);

protected:
    /* destroy - only accessible via Release() */
    virtual ~CTadsCompressedAudio();

    /* -------------------------------------------------------------------- */
    /*
     *   Decoder overrides.  This is the interface the decoder subclass must
     *   implement.
     */

    /*
     *   Decode the file.  This should simply decode the entire file; it
     *   should open the audio buffer once it knows the playback format,
     *   then decode the data and write it to the playback buffer as it
     *   does.  Data should be written in small chunks to ensure that
     *   decoding stays ahead of playback.
     */
    virtual void do_decoding(HANDLE hfile, DWORD file_size) = 0;

    /*
     *   Get/set the decoder 'stop' flag.  This flag tells us we should stop
     *   playback as soon as possible.
     */
    virtual int get_decoder_stopping() = 0;
    virtual void set_decoder_stopping(int f) = 0;

    /* -------------------------------------------------------------------- */
    /*
     *   Decoder interface - this is for the decoder subclass's use
     */

    /* initialize */
    CTadsCompressedAudio(
        const textchar_t *fname, DWORD file_startofs, DWORD file_size,
        class CTadsAudioControl *ctl,
        void (*done_func)(void *, int), void *done_func_ctx);

    /* open the output audio buffer */
    int open_playback_buffer(int freq, int bits_per_sample,
                             int number_of_channels);

    /* close the output audio buffer */
    void close_playback_buffer();

    /* write decoded PCM data to the playback buffer */
    void write_playback_buffer(char *buf, int bytes);

    /* halt playback of the output stream */
    void halt_playback_buffer();

private:
    /* -------------------------------------------------------------------- */
    /*
     *   private internal operations
     */

    /* main playback thread entrypoint */
    void do_play_thread();

protected:
    /* -------------------------------------------------------------------- */
    /*
     *   member variables
     */

    /*
     *   the file we're to play back, its starting seek location, and its
     *   size in bytes
     */
    CStringBuf fname_;
    HANDLE in_file_;
    DWORD in_file_start_;
    DWORD in_file_size_;

    /* our application-wide audio controller */
    class CTadsAudioControl *audio_control_;

    /* the output playback device */
    CTadsAudioDevice *dev_;

    /* true once we've registered with the audio controller */
    int registered_;

    /* current PCM format */
    int freq_;
    int bits_per_sample_;
    int num_channels_;

    /*
     *   volume level, 0..10000.  init_vol_ is applied when the device is
     *   opened; cur_vol_ tracks the last non-muted level so we can restore
     *   it when muting is turned back off.
     */
    int init_vol_;
    int cur_vol_;

    /* flag: we're finished with playback */
    int is_done_;

    /* protects call_done_callback() against concurrent invocation */
    std::mutex cb_mutex_;

    /* the URL of the sound resource (used only for displaying errors) */
    const char *url_;

    /* reference count */
    std::atomic<long> refcnt_;
};

#endif /* TADSCSND_H */
