#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadssnd.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadssnd.cpp - TADS sound object
Function

Notes
  Threading and cross-thread signalling here now use the C++ standard library
  rather than Win32 primitives; see tadssnd.h and
  htmltads/imgui/migration.md section 3.7.
Modified
  01/10/98 MJRoberts  - Creation
*/

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSSND_H
#include "tadssnd.h"
#endif
#ifndef HTMLSND_H
#include "htmlsnd.h"
#endif
#ifndef HTMLGUI_H
#include "htmlgui.h"
#endif

CTadsSound::CTadsSound()
{
    /* we don't have any data yet */
    seek_pos_ = 0;
    data_size_ = 0;

    /* we don't have any volume or fader controls yet */
    volctl_ = 0;
}

CTadsSound::~CTadsSound()
{
    /* release our volume and fader controls */
    if (volctl_ != 0)
        volctl_->Release();
}

/*
 *   load data from a sound loader file
 */
int CTadsSound::load_from_res(CHtmlSound *res)
{
    /* get the file identification information from the resource */
    fname_.set(res->get_fname());
    seek_pos_ = res->get_seekpos();
    data_size_ = res->get_filesize();

    /* success */
    return 0;
}

/* add a crossfade */
void CTadsSound::add_crossfade(class CHtmlSysWin *, long ms)
{
    /* if we don't have a player, ignore it */
    CTadsAudioPlayer *player = get_player();
    if (player == 0)
        return;

    /* add an end-of-track fader with crossfade */
    CTadsAudioFader *f = new CTadsAudioFaderOutEnd(
        get_player(), volctl_, ms, TRUE);

    /* star the fade */
    f->start_fade();

    /* done with our reference to the fader */
    f->Release();
}

/* cancel the sound with fade-out */
void CTadsSound::cancel_sound(CHtmlSysWin *, int sync,
                              long fade_out_ms, int fade_in_bg)
{
    /* grab local copies of the player and volume control pointers */
    CTadsAudioPlayer *player = get_player();
    CTadsAudioVolumeControl *volctl = volctl_;

    /* make sure we have a player */
    if (player == 0)
        return;

    /* add references to the objects we're working with */
    player->AddRef();
    volctl->AddRef();

    /*
     *   if we're in 'sync' mode, or there's no fade interval, simply stop
     *   the sound abruptly
     */
    if (sync || fade_out_ms == 0)
    {
        /* no fade - just stop the playback abruptly */
        player->stop(sync);
    }
    else
    {
        /*
         *   We're doing an immediate fade-out.
         *
         *   If we're going to do the fade in the background, detach the
         *   player from the resource, since we want the resource to be free
         *   to play another copy immediately.
         */
        if (fade_in_bg)
        {
            /* tell the player a background fade is starting */
            player->note_background_fade();

            /* detach the player reference from the resource */
            detach_player();

            /* detach the volume control reference from the resource */
            volctl_ = 0;
            volctl->Release();

            /*
             *   immediately invoke the 'done' callback, to let the resource
             *   queue move on to the next track while the fade-out is
             *   playing
             */
            player->call_done_callback();
        }

        /*
         *   create an immediate fade-out fader, start the fade, and release
         *   the reference (once we start the fader thread, the thread owns
         *   the fader, so we can let it go)
         */
        CTadsAudioFader *fader = new CTadsAudioFaderOutCxl(
            player, volctl, fade_out_ms);
        fader->start_fade();
        fader->Release();
    }

    /* release our references */
    player->Release();
    volctl->Release();
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic audio player class
 */
CTadsAudioPlayer::CTadsAudioPlayer(
    void (*done_func)(void *, int), void *done_func_ctx)
{
    /* remember the 'done' callback */
    done_func_ = done_func;
    done_func_ctx_ = done_func_ctx;

    /* no start time yet */
    start_time_ = 0;
}

CTadsAudioPlayer::~CTadsAudioPlayer()
{
    /*
     *   Make sure the worker thread is finished.  If we're being deleted
     *   from within the worker thread itself, we obviously can't join it -
     *   detach instead so its resources are still reclaimed.
     */
    join_worker();
}

/*
 *   enqueue the 'done' message for the main thread
 */
void CTadsAudioPlayer::send_done_message(int repeat_count)
{
    /* add a reference on behalf of the queued notification */
    AddRef();

    /* hand it to the main-thread drain queue */
    tads_audio_post_done_callback(this, repeat_count);
}

/*
 *   handle the 'done' message (called on the main thread)
 */
void CTadsAudioPlayer::on_sound_done(int repeat_count)
{
    /* invoke our 'done' callback */
    if (done_func_ != 0)
    {
        /* call the callback */
        (*done_func_)(done_func_ctx_, repeat_count);

        /* forget the callback, to ensure we only call it once */
        done_func_ = 0;
        done_func_ctx_ = 0;
    }

    /* the callback has been invoked */
    cb_evt_.set();

    /* release the reference that we added on behalf of the queued message */
    Release();
}

/*
 *   Synchronize on termination
 */
void CTadsAudioPlayer::sync_on_stop()
{
    /*
     *   Wait for each of our stop conditions in turn: playback has stopped
     *   (the 'stop' event), the worker thread has terminated, and the 'done'
     *   callback has been invoked.  Each wait has a 5-second cap so a wedged
     *   worker can't hang us here forever.
     */
    stop_evt_.wait_for(5000);
    join_worker();
    cb_evt_.wait_for(5000);
}

/*
 *   Wait until playback starts, or until it's cancelled.  Returns true if
 *   playback actually started (and wasn't also cancelled).
 */
bool CTadsAudioPlayer::wait_for_playback_start()
{
    for (;;)
    {
        /* if playback was cancelled, give up */
        if (stop_evt_.is_set())
            return false;

        /*
         *   wait a short while for the start event; if it fires, we've
         *   started (proceed only if we weren't also cancelled)
         */
        if (start_evt_.wait_for(20))
            return !stop_evt_.is_set();
    }
}

/*
 *   Wait up to 'ms' for the stop event.  Returns true if it fired.
 */
bool CTadsAudioPlayer::wait_stop(long ms)
{
    return stop_evt_.wait_for(ms);
}

/* ------------------------------------------------------------------------ */
/*
 *   Volume control object
 */
CTadsAudioVolumeControl::CTadsAudioVolumeControl(
    CTadsAudioPlayer *player, int init_vol, int fading_in, int muted)
{
    /* start with one reference on behalf of the caller */
    refcnt_.store(1);

    /* remember the player */
    player_ = player;
    player_->AddRef();

    /* remember the initial volume */
    base_vol_ = init_vol;

    /*
     *   if we're going to fade in initially, set the starting fade-in level
     *   to zero (silence); otherwise set it to 100%
     */
    fade_in_level_ = (fading_in ? 0 : 10000);

    /* we haven't started a fade-out yet, so set the fade-out level to 100% */
    fade_out_level_ = 10000;

    /* assume we're not muted */
    muted_ = muted;

    /* set the initial level in the player */
    update_level();
}

CTadsAudioVolumeControl::~CTadsAudioVolumeControl()
{
    /* release our player */
    if (player_ != 0)
        player_->Release();
}

void CTadsAudioVolumeControl::set_base_volume(int level)
{
    /*
     *   ensure we're in the 0..100 range (we could get in trouble with our
     *   log10 calculation later on if we didn't limit the range)
     */
    if (level < 0)
        level = 0;
    if (level > 100)
        level = 100;

    /* remember the new base volume */
    base_vol_ = level;

    /* update the current player volume */
    update_level();
}

void CTadsAudioVolumeControl::mute(int muted)
{
    /* note the new muting status */
    muted_ = muted;

    /* set the new volume level accordingly */
    update_level();
}

void CTadsAudioVolumeControl::set_fade_in(int level)
{
    /* if the new level is higher than before, set it */
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (level > fade_in_level_)
            fade_in_level_ = level;
    }

    /* set the updated volume */
    update_level();
}

void CTadsAudioVolumeControl::set_fade_out(int level)
{
    /* if the new level is lower than before, set it */
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (level < fade_out_level_)
            fade_out_level_ = level;
    }

    /* set the updated volume */
    update_level();
}

/*
 *   update the volume level in the player to match the current mute and fade
 *   levels
 */
void CTadsAudioVolumeControl::update_level()
{
    /*
     *   Figure the fade level: this gives us a percentage of the base
     *   volume.  The fade level is the lesser of the current fade-in and
     *   fade-out levels.
     */
    int fade;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        fade = (fade_in_level_ < fade_out_level_
                ? fade_in_level_ : fade_out_level_);
    }

    /*
     *   Figure the current base volume on a perceptual (logarithmic) scale.
     *   Human loudness perception is roughly logarithmic, so map the linear
     *   0..100 base volume through a base-10 log (over the range 1..10) into
     *   the 0..10000 range the player expects.
     */
    int vol = (int)(log10(1.0 + ((double)base_vol_/100.0)*9.0)*10000.0);

    /* combine the volume and the fade to get the final level */
    vol *= fade;
    vol /= 10000;

    /* now set the volume - 0 if muted, otherwise what we just figured */
    player_->set_audio_volume(muted_ ? 0 : vol);
}

/* ------------------------------------------------------------------------ */
/*
 *   Audio fader
 */

CTadsAudioFader::CTadsAudioFader(CTadsAudioPlayer *player,
                                 CTadsAudioVolumeControl *volctl, long ms)
{
    /* add a reference on behalf of our caller */
    refcnt_.store(1);

    /* remember our interval */
    ms_ = ms;

    /* remember (and reference) the player */
    player_ = player;
    player->AddRef();

    /* remember (and reference) the volume control */
    volctl_ = volctl;
    volctl->AddRef();
}

CTadsAudioFader::~CTadsAudioFader()
{
    /* release our volume control */
    volctl_->Release();

    /* release our player, if we still have a reference */
    if (player_ != 0)
        player_->Release();
}

/*
 *   start the fade - call this after instantiation to launch the background
 *   thread that'll carry out the fade
 */
void CTadsAudioFader::start_fade()
{
    /* add a self-reference on behalf of the thread */
    AddRef();

    /* kick off our fader thread; it owns the reference we just added */
    try
    {
        std::thread(&CTadsAudioFader::thread_main, this).detach();
    }
    catch (...)
    {
        /* the thread didn't start, so it doesn't need that reference */
        Release();
    }
}

/*
 *   fader thread main
 */
void CTadsAudioFader::do_thread_main()
{
    /* wait until the start time for the fade */
    int ok = wait_until_ready();

    /* if that succeeded, do the fade */
    if (ok)
    {
        /*
         *   figure the number of steps: we want to adjust the volume every 50
         *   ms, so divide the total time by 50 ms to get the number of steps
         */
        int steps = ms_ / 50;
        double stepsize = 1.0 / (steps != 0 ? steps : 1);

        /* do the fade */
        for (double pt = 0.0 ; steps != 0 ; --steps, pt += stepsize)
        {
            /* adjust the volume level */
            set_volctl_volume(get_step_vol(pt));

            /* wait for 50ms, but stop if playback ended */
            if (player_->wait_stop(50))
                break;
        }

        /* set the volume to the final level */
        set_volctl_volume(get_step_vol(1.0));
    }

    /* do any final action now that the fade is finished */
    after_fade(ok);

    /* we're done with our player reference */
    player_->Release();
    player_ = 0;
}

/*
 *   Wait to start the fade until the underlying track starts playing
 */
int CTadsAudioFader::wait_until_ready()
{
    /* wait until the track starts playing, or until it's canceled */
    return player_->wait_for_playback_start();
}


/* ------------------------------------------------------------------------ */
/*
 *   Fade-out fader
 */

/*
 *   When the fade ends, stop playback.  There's no point in continuing
 *   playback after the end of a fade-out, since we'd just be burning CPU
 *   decoding a track that we're playing back at zero volume.
 */
void CTadsAudioFaderOut::after_fade(int ok)
{
    /* stop playback in our player */
    player_->stop(FALSE);
}



/* ------------------------------------------------------------------------ */
/*
 *   End-of-song fader
 */

/*
 *   Wait until the track is within our fade interval of ending
 */
int CTadsAudioFaderOutEnd::wait_until_ready()
{
    /*
     *   Compute the track length.  Note that we wait until now to do this so
     *   that we do the computation in the background - figuring the length
     *   of an audio file can require reading the whole file, so it's better
     *   not to stop the foreground thread while doing that scan.  The scan
     *   should take *much* less time than actually playing back the file, so
     *   this background thread is the perfect place to do the work - it has
     *   to sit around waiting until the end of the track anyway, so we might
     *   as well do some useful work here while we're waiting.  The
     *   pathological case is that the track is so short that starting up
     *   this thread and doing the scan takes as long as playing the whole
     *   track, in which case we'd be late on starting the fade; but even a
     *   really long MP3 file should only take a few hundreds of milliseconds
     *   to scan, so a file that's so short as to trigger this condition
     *   would be too short for a perceptibly meaningful fade-out anyway.
     */
    long track_len = player_->get_track_len_ms();

    /* if it wasn't possible to compute the track length, abort the fade */
    if (track_len == 0)
        return FALSE;

    /* wait until the track starts playing */
    if (!CTadsAudioFaderOut::wait_until_ready())
        return FALSE;

    /* compute the end time: start time + track time - fade time */
    unsigned long t1 = player_->get_start_time() + track_len - ms_;

    /* assume we will proceed with the fade */
    int ok = TRUE;

    /* figure the interval from now to the end of our wait */
    long interval = (long)(t1 - CTadsAudioPlayer::now_ms());
    if (interval < 0)
        interval = 0;

    /*
     *   Wait until we reach the fade time.  However, stop if the underlying
     *   player signals termination, since there's no point in doing a fade
     *   on a player that's not playing.
     */
    if (player_->wait_stop(interval))
    {
        /* the termination event was signaled, so cancel the fade */
        ok = FALSE;
    }

    /*
     *   if all is well, and we're doing a cross-fade, call the 'done'
     *   callback now to indicate that the next track is eligible to start
     */
    if (ok && crossfade_)
        player_->call_done_callback();

    /* return our OK-to-proceed status */
    return ok;
}
