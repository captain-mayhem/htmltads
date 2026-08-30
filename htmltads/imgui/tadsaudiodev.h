/*
 *   Copyright (c) 2024 by the tads-runner contributors.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsaudiodev.h - abstract PCM playback device for guit3
Function
  Defines a small, backend-neutral interface that CTadsCompressedAudio (the
  base class shared by the WAV, MPEG and Ogg Vorbis decoders) uses to stream
  decoded PCM to the sound card.  It replaces the DirectSound triple-buffer
  that the code carried from the original Win32 htmlt3 client.

  The only implementation today is a miniaudio backend (tadsaudiodev.cpp),
  which is cross-platform (Windows/macOS/Linux/BSD and, later, WASM).  The
  interface is deliberately shaped like the old DirectSound streaming buffer -
  open a PCM stream, block-write interleaved samples as they are decoded,
  drain or halt at the end - so the decoders did not have to change.

  See htmltads/imgui/migration.md section 3.7 for the migration rationale.
Notes

Modified
  2024 - creation, as part of the guit3 audio-backend migration
*/

#ifndef TADSAUDIODEV_H
#define TADSAUDIODEV_H

/*
 *   Abstract streaming PCM playback device.
 *
 *   All of the calls except set_volume() are made from a single decoder
 *   thread, in this order: open() once, then write() repeatedly as the
 *   decoder produces PCM, then either drain() (natural end - let queued audio
 *   finish) or halt() (cancelled - drop queued audio immediately), then
 *   close().  Ogg Vorbis may loop back to open() again mid-stream when the
 *   sample rate changes between logical bitstreams, so open()/close() must be
 *   safe to call more than once on the same object.
 *
 *   set_volume() may be called from any thread (the fader threads and the
 *   application-wide mute broadcast both use it), so implementations must
 *   guard it against a concurrent close().
 */
class CTadsAudioDevice
{
public:
    virtual ~CTadsAudioDevice() { }

    /*
     *   Open a PCM playback stream.  'bits_per_sample' is 8 or 16; samples
     *   are native-endian, interleaved by channel.  Returns 0 on success,
     *   non-zero on failure.
     */
    virtual int open(int freq, int bits_per_sample, int num_channels) = 0;

    /*
     *   Queue interleaved PCM for playback.  Blocks until the data has been
     *   accepted into the internal buffer (i.e. until playback has consumed
     *   enough of the backlog to make room).  Returns early without
     *   consuming everything if a stop has been requested via halt().
     */
    virtual void write(const char *buf, int bytes) = 0;

    /*
     *   Stop immediately, discarding anything still queued.  After halt(),
     *   write() returns without queueing anything.
     */
    virtual void halt() = 0;

    /*
     *   Block until everything already written has actually played out, then
     *   stop.  Used at the natural end of a track.
     */
    virtual void drain() = 0;

    /* Close the stream and release the underlying device. */
    virtual void close() = 0;

    /*
     *   Set the playback volume, 0..10000 (silence to unattenuated).  The
     *   perceptual (logarithmic) curve is applied upstream by
     *   CTadsAudioVolumeControl, so this mapping to backend gain is linear.
     */
    virtual void set_volume(int vol) = 0;

    /* Is a stream currently open? */
    virtual int is_open() const = 0;

    /*
     *   Create the platform's playback device.  Returns a new object the
     *   caller owns (delete when done), or null if no audio backend could be
     *   initialised.
     */
    static CTadsAudioDevice *create();
};


/* ------------------------------------------------------------------------ */
/*
 *   'done' callback marshalling.
 *
 *   The original client posted a Win32 message (HTMLM_SOUND_DONE) from the
 *   playback thread so that the sound resource's 'done' callback would run on
 *   the main thread.  guit3 has no message pump, so instead the playback and
 *   fader threads enqueue their completed CTadsAudioPlayer here, and
 *   CHtmlSys_mainwin::event_loop() drains the queue once per frame on the
 *   main thread.
 *
 *   tads_audio_post_done_callback() is called from a worker thread with a
 *   reference already added on behalf of the queued entry (matching the old
 *   send_done_message() contract); tads_audio_run_done_callbacks() invokes
 *   CTadsAudioPlayer::on_sound_done() for each and releases that reference.
 */
class CTadsAudioPlayer;
void tads_audio_post_done_callback(CTadsAudioPlayer *player, int repeat_count);
void tads_audio_run_done_callbacks();

#endif /* TADSAUDIODEV_H */
