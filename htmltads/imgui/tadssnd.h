/* $Header: d:/cvsroot/tads/html/win32/tadssnd.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadssnd.h - TADS sound object
Function

Notes
  The playback plumbing here (players, volume controls, faders) was
  originally built directly on Win32 primitives - CreateEvent/CreateThread/
  CRITICAL_SECTION and a PostMessage() to marshal the 'done' callback onto
  the main thread.  For the guit3 cross-platform port it now uses the C++11
  standard library (<thread>, <mutex>, <condition_variable>, <atomic>) and
  the frame-drained callback queue in tadsaudiodev.h.  See
  htmltads/imgui/migration.md section 3.7.
Modified
  01/10/98 MJRoberts  - Creation
*/

#ifndef TADSSND_H
#define TADSSND_H

#include <math.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

#ifndef TADSAUDIODEV_H
#include "tadsaudiodev.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Manual-reset event, modelled on the Win32 CreateEvent(manual, unsignaled)
 *   objects this code used to use.  set() latches the event; wait()/wait_for()
 *   block until it is set (or return immediately if it already is); reset()
 *   clears the latch.
 */
class tads_event
{
public:
    tads_event() : signaled_(false) { }

    void set()
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            signaled_ = true;
        }
        cv_.notify_all();
    }

    void reset()
    {
        std::lock_guard<std::mutex> lk(mtx_);
        signaled_ = false;
    }

    bool is_set()
    {
        std::lock_guard<std::mutex> lk(mtx_);
        return signaled_;
    }

    void wait()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this] { return signaled_; });
    }

    /* wait up to 'ms' (negative == forever); return true if signaled */
    bool wait_for(long ms)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        if (ms < 0)
        {
            cv_.wait(lk, [this] { return signaled_; });
            return true;
        }
        return cv_.wait_for(lk, std::chrono::milliseconds(ms),
                            [this] { return signaled_; });
    }

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    bool signaled_;
};


/*------------------------------------------------------------------------ */
/*
 *   Audio Controller - provides global control over certain aspects of sound
 *   playback.  This is an abstract interface that must be provided to sound
 *   objects when they're created.
 */
class CTadsAudioControl
{
public:
    /* reference control */
    virtual void audioctl_add_ref() = 0;
    virtual void audioctl_release() = 0;

    /* get the current muting status */
    virtual int get_mute_sound() = 0;

    /*
     *   Register/unregister a playing sound.  When the controller needs to
     *   change a global playback parameter globally (such as the muting
     *   status), it'll inform each registered sound object of the change.
     */
    virtual void register_active_sound(class CTadsAudioPlayer *sound) = 0;
    virtual void unregister_active_sound(class CTadsAudioPlayer *sound) = 0;
};

/*
 *   The generic sound player interface.  System sound player objects can
 *   register themselves with the audio controller to receive notifications
 *   of changes to global system sound settings.
 */
class CTadsAudioPlayer
{
public:
    CTadsAudioPlayer(void (*done_func)(void *, int), void *done_func_ctx);
    virtual ~CTadsAudioPlayer();

    /* is the track still playing? */
    virtual int is_playing() = 0;

    /* get the starting time of playback, in milliseconds (steady clock) */
    unsigned long get_start_time() const { return start_time_; }

    /* terminate playback */
    virtual void stop(int sync) = 0;

    /* have we been terminated? */
    virtual int is_stopped() = 0;

    /*
     *   Get the real-time playback length of the sound, in milliseconds.
     *   Return 0 if it's not possible to determine the length of the track.
     */
    virtual long get_track_len_ms() = 0;

    /* receive notification of a change in muting status */
    virtual void on_mute_change(int mute) = 0;

    /* set the playback volume - 0..10000 (silence to unattenuated) */
    virtual void set_audio_volume(int vol) = 0;

    /*
     *   Immediately call the 'done' callback.  The callback should never be
     *   invoked more than once per play, so if the callback has already been
     *   invoked, this should do nothing.
     */
    virtual void call_done_callback() = 0;

    /* note that we're starting a background fade on this player */
    virtual void note_background_fade() { }

    /* reference counting */
    virtual void AddRef() = 0;
    virtual void Release() = 0;

    /*
     *   Fader coordination.  wait_for_playback_start() blocks until the
     *   track starts playing or is cancelled, returning true if it actually
     *   started (and wasn't also cancelled).  wait_stop() blocks up to 'ms'
     *   (negative == forever) for the stop event, returning true if it fired.
     *   stop_signaled() is a non-blocking poll of the same event.
     */
    bool wait_for_playback_start();
    bool wait_stop(long ms);
    bool stop_signaled() { return stop_evt_.is_set(); }

    /*
     *   Handle the 'done' notification.  The worker thread enqueues this via
     *   tads_audio_post_done_callback() (which adds a reference on our
     *   behalf); CHtmlSys_mainwin::event_loop() drains the queue once per
     *   frame on the main thread and calls this, which finally invokes the
     *   callback function provided when the sound was initiated.  Going
     *   through the queue rather than calling the callback directly from the
     *   playback thread ensures the callback only ever runs on the main
     *   thread.
     */
    void on_sound_done(int repeat_count);

    /* enqueue the "sound done" notification for the main thread */
    void send_done_message(int repeat_count);

    /*
     *   Synchronize on stop.  This waits until playback has stopped, the
     *   'done' callback has been invoked, and any background thread has
     *   terminated.
     */
    void sync_on_stop();

    /* current time in milliseconds, on the same clock as get_start_time() */
    static unsigned long now_ms()
    {
        return (unsigned long)
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }

protected:
    /* record the playback start time (call when playback actually begins) */
    void mark_start_time()
    {
        start_time_ = now_ms();
    }

    /* launch the worker (playback/monitor) thread running 'fn' */
    void run_worker(std::function<void()> fn)
    {
        worker_ = std::thread(std::move(fn));
        worker_id_ = worker_.get_id();
    }

    /* join the worker thread, unless we're being called from it */
    void join_worker()
    {
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id())
            worker_.join();
        else if (worker_.joinable())
            worker_.detach();
    }

    /* are we currently executing on the worker thread? */
    bool worker_is_self() const
    {
        return worker_id_ == std::this_thread::get_id();
    }

    /*
     *   Playback tracking events: we set start_evt_ when playback starts, and
     *   we set stop_evt_ when playback ends.  These let other threads (e.g.
     *   the fader threads) coordinate their timing.  cb_evt_ is set once the
     *   'done' callback has been invoked.
     */
    tads_event start_evt_;
    tads_event stop_evt_;
    tads_event cb_evt_;

    /* the background worker thread */
    std::thread worker_;
    std::thread::id worker_id_;

    /* starting time, in steady-clock milliseconds */
    unsigned long start_time_;

    /* the 'done' callback function */
    void (*done_func_)(void *ctx, int repeat_count);
    void *done_func_ctx_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Volume control.  Each player has an associated volume control, which
 *   mediates muting, fade in, and fade out.
 */
class CTadsAudioVolumeControl
{
public:
    CTadsAudioVolumeControl(CTadsAudioPlayer *player,
                            int init_vol, int fading_in, int muted);
    virtual ~CTadsAudioVolumeControl();

    /* reference counting - for thread safety, use atomic inc/dec */
    void AddRef()
    {
        refcnt_.fetch_add(1);
    }
    void Release()
    {
        if (refcnt_.fetch_sub(1) == 1)
            delete this;
    }

    /*
     *   Set the base playback volume.  This is the volume we use when we're
     *   not muting or fading.  By default, we play back at 100% volume.
     *   This sets the volume on a 0..100 scale, where 0 is silence and 100
     *   is unattenuated.
     */
    void set_base_volume(int level);

    /* mute the sound */
    void mute(int muted);

    /*
     *   Adjust the fade-in or fade-out level.  The actual playback level is
     *   the lesser of the current fade-in and fade-out levels, so that if we
     *   are simultaneously fading in and out, we'll have a nice smooth
     *   triangle envelope that peaks at the intersection of the two fade
     *   curves.  If we have multiple concurrent fades in the same direction,
     *   we'll keep the fade smooth by using the LEADING fade: when fading
     *   in, we'll ignore a new fade-in level that's less than the previous
     *   fade-in level, and when fading out we'll ignore new levels that are
     *   higher than previous levels.  (Of course, muting always takes
     *   precedence over fading.)
     *
     *   The level is given in the range 0..10000, where 0 is silence and
     *   10000 is unattenuated.
     */
    void set_fade_in(int level);
    void set_fade_out(int level);

private:
    /*
     *   Update the playback volume level to match the current parameters -
     *   this computes the volume level based on the current fade and mute
     *   settings, and sets it in the underlying playback object
     */
    void update_level();

    /* the base volume level (0-100) */
    int base_vol_;

    /* are we muted? */
    int muted_;

    /* current fade-in and fade-out levels */
    int fade_in_level_;
    int fade_out_level_;

    /* we are a reference-counted object */
    std::atomic<int> refcnt_;

    /* synchronization for the fade levels */
    std::mutex mtx_;

    /* the underlying audio player object */
    CTadsAudioPlayer *player_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Fader control
 */
class CTadsAudioFader
{
public:
    CTadsAudioFader(CTadsAudioPlayer *player,
                    CTadsAudioVolumeControl *volctl, long ms);
    virtual ~CTadsAudioFader();

    /*
     *   start the fade - call this after creating the object to initiate the
     *   background thread; after this is called, the fade will run by itself
     */
    void start_fade();

    /* reference counting */
    void AddRef() { refcnt_.fetch_add(1); }
    void Release()
    {
        if (refcnt_.fetch_sub(1) == 1)
            delete this;
    }

protected:
    /*
     *   Wait until the appointed start time for the fade action.  Return
     *   true if the fade should proceed, false if not.
     */
    virtual int wait_until_ready();

    /*
     *   Do any final action after the fade is finished.  'ok' is the status
     *   returned from wait_until_ready() - it indicates whether or not we
     *   actually did the fade.
     */
    virtual void after_fade(int ok) { }

    /*
     *   Get the volume at the given point in the fade.  The step is given as
     *   a fraction of the time in the fade, from 0.0 to 1.0.  Returns a
     *   value from 0 (total silence) to 10000 (full volume).
     */
    virtual int get_step_vol(double step) = 0;

    /* set the volume in the volume control */
    virtual void set_volctl_volume(int vol) = 0;

    /* thread entrypoint - runs the fade, then releases the thread's ref */
    static void thread_main(CTadsAudioFader *self)
    {
        self->do_thread_main();
        self->Release();
    }

    /* member function thread handler */
    void do_thread_main();

    /* duration of the fade, in milliseconds */
    long ms_;

    /* reference count */
    std::atomic<int> refcnt_;

    /* the player associated with the fade */
    CTadsAudioPlayer *player_;

    /* volume control object for the track player */
    CTadsAudioVolumeControl *volctl_;
};

/*
 *   Fade In fader
 */
class CTadsAudioFaderIn: public CTadsAudioFader
{
public:
    CTadsAudioFaderIn(CTadsAudioPlayer *player,
                      CTadsAudioVolumeControl *volctl, long ms)
        : CTadsAudioFader(player, volctl, ms) { }

protected:
    /* when fading in, the volume goes up on each step */
    virtual int get_step_vol(double step)
    {
        /*
         *   Figure the point on the log10 scale from 1 to 10, then normalize
         *   to the 0-10000 range.
         */
        return (int)(log10(1.0 + step*9.0)*10000.0);
    }

    /* set the volume in the volume control */
    virtual void set_volctl_volume(int vol) { volctl_->set_fade_in(vol); }
};

/*
 *   Base Fade Out fader
 */
class CTadsAudioFaderOut: public CTadsAudioFader
{
public:
    CTadsAudioFaderOut(CTadsAudioPlayer *player,
                       CTadsAudioVolumeControl *volctl, long ms)
        : CTadsAudioFader(player, volctl, ms) { }

protected:
    /* when fading down, the volume goes down on each step */
    virtual int get_step_vol(double step)
    {
        /*
         *   Figure the point on the log10 scale from 1 to 10, then normalize
         *   to the 0-10000 range.
         */
        return (int)(log10(10.0 - step*9.0)*10000.0);
    }

    /* after the fade is finished, kill the playback in the player */
    virtual void after_fade(int ok);

    /* set the volume in the volume control */
    virtual void set_volctl_volume(int vol) { volctl_->set_fade_out(vol); }
};

/*
 *   Cancellation Fade Out fader.  This starts a fade-out immediately, then
 *   stops the underlying player once we get to silence.
 */
class CTadsAudioFaderOutCxl: public CTadsAudioFaderOut
{
public:
    CTadsAudioFaderOutCxl(class CTadsAudioPlayer *player,
                          CTadsAudioVolumeControl *volctl, long ms)
        : CTadsAudioFaderOut(player, volctl, ms) { }
};


/*
 *   End-of-track Fade Out fader.  This waits for the duration of the track
 *   minus the fade time, then starts the fade-out.
 */
class CTadsAudioFaderOutEnd: public CTadsAudioFaderOut
{
public:
    CTadsAudioFaderOutEnd(
        CTadsAudioPlayer *player, CTadsAudioVolumeControl *volctl,
        long ms, int crossfade)
        : CTadsAudioFaderOut(player, volctl, ms)
    {
        /* remember the cross-fade status */
        crossfade_ = crossfade;
    }

protected:
    /*
     *   when fading out, we need to wait until 'ms' millseconds before the
     *   end of the track
     */
    virtual int wait_until_ready();

    /* are we doing a cross-fade on the fade-out? */
    int crossfade_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Base sound object
 */
class CTadsSound
{
public:
    CTadsSound();
    virtual ~CTadsSound();

    /* load from a sound resource loader */
    int load_from_res(class CHtmlSound *loader);

    /* cancel with fade-out */
    void cancel_sound(class CHtmlSysWin *win, int sync,
                      long fade_out_ms, int fade_in_bg);

    /* add a crossfade */
    void add_crossfade(class CHtmlSysWin *win, long ms);

    /* get/detach the player object */
    virtual class CTadsAudioPlayer *get_player() const = 0;
    virtual void detach_player() = 0;

protected:
    /* file information */
    CStringBuf fname_;
    unsigned long seek_pos_;
    unsigned long data_size_;

    /* volume control */
    CTadsAudioVolumeControl *volctl_;
};


#endif /* TADSSND_H */
