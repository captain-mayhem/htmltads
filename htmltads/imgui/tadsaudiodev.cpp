/*
 *   Copyright (c) 2024 by the tads-runner contributors.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsaudiodev.cpp - miniaudio-backed PCM playback device for guit3
Function
  Implements CTadsAudioDevice (see tadsaudiodev.h) on top of miniaudio, plus
  the cross-thread 'done' callback queue that replaces the old
  HTMLM_SOUND_DONE window message.
Notes
  The decoder thread is the single producer feeding write(); miniaudio's
  audio thread is the single consumer draining the ring buffer in the data
  callback.  ma_pcm_rb is lock-free for exactly that single-producer/
  single-consumer pattern, so the streaming path needs no mutex.  The device
  handle itself is guarded by dev_mutex_ because set_volume() can be called
  from the fader threads while the decoder thread is in close().
Modified
  2024 - creation, as part of the guit3 audio-backend migration
*/

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>

#include "miniaudio.h"

#include "tadsaudiodev.h"
#include "tadssnd.h"


/* ------------------------------------------------------------------------ */
/*
 *   miniaudio implementation of CTadsAudioDevice
 */
namespace
{

class CTadsAudioDevice_miniaudio: public CTadsAudioDevice
{
public:
    CTadsAudioDevice_miniaudio()
        : device_inited_(false), started_(false),
          rb_inited_(false), stop_requested_(false),
          pending_vol_(10000), buffer_frames_(0),
          start_threshold_frames_(0), bytes_per_frame_(0)
    {
    }

    ~CTadsAudioDevice_miniaudio()
    {
        close();
    }

    int open(int freq, int bits_per_sample, int num_channels)
    {
        /* a stale stream from a previous logical bitstream must go first */
        close();

        std::lock_guard<std::mutex> lk(dev_mutex_);

        stop_requested_.store(false);
        started_ = false;

        ma_format fmt = (bits_per_sample == 8)
                        ? ma_format_u8 : ma_format_s16;
        bytes_per_frame_ = num_channels * (bits_per_sample / 8);

        /*
         *   Size the ring buffer at one second of audio.  The old DirectSound
         *   path used three half-second chunks; one second gives the decoder
         *   the same comfortable lead over the play cursor without the
         *   chunk-cursor bookkeeping.  Start playback once a quarter of it is
         *   buffered, mirroring the old "start after two chunks" heuristic.
         */
        buffer_frames_ = (freq > 0 ? (ma_uint32)freq : 44100);
        start_threshold_frames_ = buffer_frames_ / 4;
        if (start_threshold_frames_ == 0)
            start_threshold_frames_ = 1;

        if (ma_pcm_rb_init(fmt, num_channels, buffer_frames_,
                           NULL, NULL, &rb_) != MA_SUCCESS)
            return 1;
        rb_inited_ = true;

        ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
        cfg.playback.format = fmt;
        cfg.playback.channels = num_channels;
        cfg.sampleRate = (ma_uint32)freq;
        cfg.dataCallback = &data_callback_thunk;
        cfg.pUserData = this;

        if (ma_device_init(NULL, &cfg, &device_) != MA_SUCCESS)
        {
            ma_pcm_rb_uninit(&rb_);
            rb_inited_ = false;
            return 2;
        }
        device_inited_ = true;

        ma_device_set_master_volume(
            &device_, (float)pending_vol_ / 10000.0f);

        return 0;
    }

    void write(const char *buf, int bytes)
    {
        if (stop_requested_.load() || !rb_inited_)
            return;

        while (bytes > 0 && !stop_requested_.load())
        {
            ma_uint32 want = (ma_uint32)(bytes / bytes_per_frame_);
            if (want == 0)
                break;

            void *dst = NULL;
            ma_uint32 got = want;
            if (ma_pcm_rb_acquire_write(&rb_, &got, &dst) != MA_SUCCESS)
                break;

            if (got == 0)
            {
                /* buffer full - let playback drain a little, then retry */
                maybe_start();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            ma_uint32 nbytes = got * bytes_per_frame_;
            memcpy(dst, buf, nbytes);
            ma_pcm_rb_commit_write(&rb_, got);

            buf += nbytes;
            bytes -= (int)nbytes;

            maybe_start();
        }
    }

    void halt()
    {
        stop_requested_.store(true);

        std::lock_guard<std::mutex> lk(dev_mutex_);
        if (device_inited_ && started_)
        {
            ma_device_stop(&device_);
            started_ = false;
        }
        /*
         *   Don't touch the ring buffer here - the producer (the decoder
         *   thread) may still be mid-write.  It checks stop_requested_ and
         *   bails, and close() (always called next, from that same thread)
         *   tears the buffer down; nothing reads it in the meantime.
         */
    }

    void drain()
    {
        if (stop_requested_.load())
            return;

        /*
         *   Make sure playback is actually running - a sound shorter than the
         *   normal start threshold still has to be played out.
         */
        maybe_start(true);

        {
            std::lock_guard<std::mutex> lk(dev_mutex_);
            if (!device_inited_ || !started_)
                return;
        }

        /* wait for the ring buffer to empty out */
        while (!stop_requested_.load()
               && rb_inited_
               && ma_pcm_rb_available_read(&rb_) > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        /* let the frames already handed to the OS finish, then stop */
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        std::lock_guard<std::mutex> lk(dev_mutex_);
        if (device_inited_ && started_)
        {
            ma_device_stop(&device_);
            started_ = false;
        }
    }

    void close()
    {
        std::lock_guard<std::mutex> lk(dev_mutex_);

        if (device_inited_)
        {
            ma_device_uninit(&device_);
            device_inited_ = false;
        }
        started_ = false;

        if (rb_inited_)
        {
            ma_pcm_rb_uninit(&rb_);
            rb_inited_ = false;
        }
    }

    void set_volume(int vol)
    {
        if (vol < 0)
            vol = 0;
        if (vol > 10000)
            vol = 10000;

        std::lock_guard<std::mutex> lk(dev_mutex_);
        pending_vol_ = vol;
        if (device_inited_)
            ma_device_set_master_volume(&device_, (float)vol / 10000.0f);
    }

    int is_open() const { return device_inited_ ? 1 : 0; }

private:
    /*
     *   Start the device (idempotent).  Normally we wait until a quarter of
     *   the ring buffer is primed - or it's completely full - before
     *   starting, to keep the decoder comfortably ahead of the play cursor.
     *   'force' skips that check, for a sound so short it decoded to less
     *   than the threshold and now just needs to be played out.
     */
    void maybe_start(bool force = false)
    {
        if (started_ || stop_requested_.load())
            return;

        std::lock_guard<std::mutex> lk(dev_mutex_);
        if (started_ || !device_inited_ || !rb_inited_)
            return;
        if (!force
            && ma_pcm_rb_available_read(&rb_) < start_threshold_frames_
            && ma_pcm_rb_available_write(&rb_) > 0)
            return;

        if (ma_device_start(&device_) == MA_SUCCESS)
            started_ = true;
    }

    static void data_callback_thunk(
        ma_device *dev, void *out, const void *in, ma_uint32 frame_count)
    {
        (void)in;
        ((CTadsAudioDevice_miniaudio *)dev->pUserData)
            ->data_callback(out, frame_count);
    }

    void data_callback(void *out, ma_uint32 frame_count)
    {
        ma_uint32 filled = 0;
        char *dst = (char *)out;

        while (filled < frame_count && rb_inited_)
        {
            ma_uint32 n = frame_count - filled;
            void *src = NULL;
            if (ma_pcm_rb_acquire_read(&rb_, &n, &src) != MA_SUCCESS || n == 0)
                break;

            memcpy(dst + (size_t)filled * bytes_per_frame_,
                   src, (size_t)n * bytes_per_frame_);
            ma_pcm_rb_commit_read(&rb_, n);
            filled += n;
        }

        /* underrun (or drained) - output silence for the remainder */
        if (filled < frame_count)
        {
            ma_silence_pcm_frames(
                dst + (size_t)filled * bytes_per_frame_,
                frame_count - filled,
                dev_format(), dev_channels());
        }
    }

    ma_format dev_format() const
    {
        return device_inited_ ? device_.playback.format : ma_format_s16;
    }
    ma_uint32 dev_channels() const
    {
        return device_inited_ ? device_.playback.channels : 2;
    }

    ma_device device_;
    ma_pcm_rb rb_;

    bool device_inited_;
    bool started_;
    bool rb_inited_;

    std::atomic<bool> stop_requested_;

    int pending_vol_;
    ma_uint32 buffer_frames_;
    ma_uint32 start_threshold_frames_;
    ma_uint32 bytes_per_frame_;

    std::mutex dev_mutex_;
};

} /* anonymous namespace */

CTadsAudioDevice *CTadsAudioDevice::create()
{
    return new CTadsAudioDevice_miniaudio();
}

bool CTadsAudioDevice::is_available()
{
    ma_context ctx;
    if (ma_context_init(NULL, 0, NULL, &ctx) != MA_SUCCESS)
        return false;

    ma_context_uninit(&ctx);
    return true;
}


/* ------------------------------------------------------------------------ */
/*
 *   Cross-thread 'done' callback queue
 */
namespace
{
struct done_entry_t
{
    CTadsAudioPlayer *player;
    int repeat_count;
};

std::mutex g_done_mutex;
std::deque<done_entry_t> g_done_queue;
}

void tads_audio_post_done_callback(CTadsAudioPlayer *player, int repeat_count)
{
    if (player == 0)
        return;

    std::lock_guard<std::mutex> lk(g_done_mutex);
    done_entry_t e;
    e.player = player;
    e.repeat_count = repeat_count;
    g_done_queue.push_back(e);
}

void tads_audio_run_done_callbacks()
{
    for (;;)
    {
        done_entry_t e;
        {
            std::lock_guard<std::mutex> lk(g_done_mutex);
            if (g_done_queue.empty())
                return;
            e = g_done_queue.front();
            g_done_queue.pop_front();
        }

        /*
         *   on_sound_done() invokes the resource's 'done' callback and then
         *   releases the reference the worker thread added on our behalf.
         */
        e.player->on_sound_done(e.repeat_count);
    }
}
