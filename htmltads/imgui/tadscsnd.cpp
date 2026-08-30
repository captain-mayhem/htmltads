/*
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadscsnd.cpp - TADS Compressed Sound base class
Function
  Streams decoded PCM from a decoder subclass to the sound card.  The output
  path is CTadsAudioDevice (miniaudio); the DirectSound triple-buffer that
  this file used to carry from the Win32 htmlt3 client is gone.  See
  htmltads/imgui/migration.md section 3.7.
Notes

Modified
  04/26/02 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>

#include <Windows.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "tadssnd.h"
#include "tadscsnd.h"


/* ------------------------------------------------------------------------ */
/*
 *   Implementation
 */


/*
 *   create
 */
CTadsCompressedAudio::CTadsCompressedAudio(
    const textchar_t *fname, DWORD file_start_ofs, DWORD file_size,
    class CTadsAudioControl *ctl,
    void (*done_func)(void *, int), void *done_func_ctx)
    : CTadsAudioPlayer(done_func, done_func_ctx)
{
    /* set a reference on behalf of our caller */
    refcnt_.store(1);

    /* create the output device */
    dev_ = CTadsAudioDevice::create();

    /* not registered with the audio controller yet */
    registered_ = FALSE;

    /* use full volume initially, unless we hear otherwise */
    init_vol_ = 10000;
    cur_vol_ = 10000;

    /* no format yet */
    freq_ = 0;
    bits_per_sample_ = 0;
    num_channels_ = 0;

    /* remember and place a reference on the audio controller */
    audio_control_ = ctl;
    ctl->audioctl_add_ref();

    /* we're not finished yet */
    is_done_ = FALSE;

    /* no URL yet */
    url_ = 0;

    /* remember the filename */
    fname_.set(fname);

    /* open the file */
    in_file_ = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ,
                          0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    /* remember the file starting seek position and size */
    in_file_start_ = file_start_ofs;
    in_file_size_ = file_size;

    /* seek to the start of the stream */
    if (in_file_ != INVALID_HANDLE_VALUE)
        SetFilePointer(in_file_, file_start_ofs, 0, FILE_BEGIN);
}

/*
 *   destroy
 */
CTadsCompressedAudio::~CTadsCompressedAudio()
{
    /*
     *   If we have a background thread, wait for it - unless the current
     *   thread *is* the background thread, in which case we'd deadlock
     *   waiting for ourself (and there'd be no point, since we're the one
     *   doing the work).
     */
    join_worker();

    /* make sure we've closed our buffer */
    close_playback_buffer();

    /* release the output device */
    if (dev_ != 0)
    {
        delete dev_;
        dev_ = 0;
    }

    /* release our audio controller interface */
    audio_control_->audioctl_release();

    /* close our file */
    if (in_file_ != INVALID_HANDLE_VALUE)
        CloseHandle(in_file_);
}

/*
 *   open an audio buffer
 */
int CTadsCompressedAudio::open_playback_buffer(int freq, int bits_per_sample,
                                               int num_channels)
{
    /* remember the format */
    freq_ = freq;
    bits_per_sample_ = bits_per_sample;
    num_channels_ = num_channels;

    /* open the output stream */
    if (dev_ == 0 || dev_->open(freq, bits_per_sample, num_channels) != 0)
        return 2;

    /* we're now playing, so register with the audio controller */
    audio_control_->register_active_sound(this);
    registered_ = TRUE;

    /*
     *   Set the initial volume.  If application-wide muting is active, set
     *   the volume to zero; otherwise use the initial volume, so that we
     *   respect any volume change that was set before we opened the stream.
     */
    cur_vol_ = init_vol_;
    dev_->set_volume(audio_control_->get_mute_sound() ? 0 : init_vol_);

    /* success */
    return 0;
}

/*
 *   Change the muting status
 */
void CTadsCompressedAudio::on_mute_change(int mute)
{
    if (dev_ != 0)
        dev_->set_volume(mute ? 0 : cur_vol_);
}

/*
 *   close the audio buffer
 */
void CTadsCompressedAudio::close_playback_buffer()
{
    /* if we never opened the stream, there's nothing to do */
    if (dev_ == 0 || !dev_->is_open())
    {
        /* still unregister if we somehow registered */
        if (registered_)
        {
            audio_control_->unregister_active_sound(this);
            registered_ = FALSE;
        }
        return;
    }

    /*
     *   If we cancelled playback prematurely, drop whatever is still queued;
     *   otherwise let the queued audio finish playing out.
     */
    if (get_decoder_stopping())
        dev_->halt();
    else
        dev_->drain();

    /* close the output stream */
    dev_->close();

    /* we're no longer playing, so unregister with the controller */
    if (registered_)
    {
        audio_control_->unregister_active_sound(this);
        registered_ = FALSE;
    }
}

/*
 *   Write to the audio buffer.  This puts data into the buffer, blocking
 *   until enough space is available to store the data.  The decoder should
 *   call this frequently to ensure that we keep the buffer prepared with
 *   data to play back while we're decoding.
 */
void CTadsCompressedAudio::write_playback_buffer(char *buf, int bytes)
{
    /* if the decoder is stopping, don't bother */
    if (get_decoder_stopping())
        return;

    if (dev_ != 0)
        dev_->write(buf, bytes);
}

/*
 *   Halt playback
 */
void CTadsCompressedAudio::halt_playback_buffer()
{
    if (dev_ != 0)
        dev_->halt();
}

/* ------------------------------------------------------------------------ */
/*
 *   HTML TADS Interface
 */

/*
 *   Play our file asynchronously
 */
int CTadsCompressedAudio::play(const textchar_t *url)
{
    /* remember the URL */
    url_ = url;

    /* we're not done yet */
    is_done_ = FALSE;

    /* not stopping yet */
    set_decoder_stopping(FALSE);

    /* set a reference on behalf of the thread */
    AddRef();

    /* create the thread that will do the actual work */
    try
    {
        run_worker([this]()
        {
            do_play_thread();

            /* release the thread's reference on 'this' */
            Release();

#ifdef _WIN32
            /* release this thread's TADS TLS object */
            CTadsApp::on_thread_exit();
#endif
        });
    }
    catch (...)
    {
        /* we won't need the thread reference after all */
        Release();

        /* signal the termination event */
        stop_evt_.set();

        /* return failure */
        return 1;
    }

    /* success */
    return 0;
}

/*
 *   Determine if playback is still underway.  Returns true if playback is
 *   active, false if playback has finished.
 */
int CTadsCompressedAudio::is_playing()
{
    /* we're playing if the 'stop' event hasn't been signaled yet */
    return !stop_evt_.is_set();
}

/*
 *   wait until playback is finished
 */
void CTadsCompressedAudio::wait_until_done()
{
    stop_evt_.wait();
}

/*
 *   Set the volume; the range of 'vol' is 0 to 10000
 */
void CTadsCompressedAudio::set_audio_volume(int vol)
{
    /* remember it as our current (non-muted) level */
    cur_vol_ = vol;

    /* also record it as the initial level for a not-yet-open stream */
    init_vol_ = vol;

    /* push it to the device (which stores it if the stream isn't open yet) */
    if (dev_ != 0)
        dev_->set_volume(vol);
}

/*
 *   Playback thread main entrypoint
 */
void CTadsCompressedAudio::do_play_thread()
{
    /* signal that playback has started */
    mark_start_time();
    start_evt_.set();

    /* decode the file */
    do_decoding(in_file_, in_file_size_);

    /* if playback was cancelled, drop anything still queued */
    if (get_decoder_stopping())
        halt_playback_buffer();

    /* close the audio device */
    close_playback_buffer();

    /* signal that playback has finished */
    stop_evt_.set();

    /* call the 'done' callback */
    call_done_callback();
}

/*
 *   Stop playback
 */
void CTadsCompressedAudio::stop(int sync)
{
    /* set the flag to indicate that we shouldn't do any more decoding */
    set_decoder_stopping(TRUE);

    /* make the output stop right away so 'sync' callers don't wait it out */
    halt_playback_buffer();

    /* wait until we get confirmation, if synchronizing */
    if (sync)
        sync_on_stop();
}

/* invoke the 'done' callback */
void CTadsCompressedAudio::call_done_callback()
{
    /*
     *   Protect this as a critical section - we don't want to enqueue the
     *   notification twice if another thread decides to handle it too.
     */
    std::lock_guard<std::mutex> lk(cb_mutex_);

    /*
     *   enqueue the 'done' notification for the main thread.  We never play
     *   a sound more than once, so the repeat count is always 1.
     */
    send_done_message(1);

    /* set our 'done' flag */
    is_done_ = TRUE;
}
