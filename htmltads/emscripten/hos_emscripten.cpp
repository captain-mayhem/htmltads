#include <os.h>
#include <osifcext.h>

#include "html_os.h"

/* ------------------------------------------------------------------------ */
/*
 *   Code page identifier 
 */

/* initialize, given the xxx_CHARSET ID and the code page number */
oshtml_charset_id_t::oshtml_charset_id_t(unsigned int cs, unsigned int cp)
{
    charset = cs;
    codepage = cp;
}

/* initialize to default settings */
oshtml_charset_id_t::oshtml_charset_id_t()
{
    charset = 0;
    codepage = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the next character in a string.
 */
static int utf8_charsize(const char* ch){
	if (ch == nullptr || ch[0] == '\0')
	 return 0;
	if ((ch[0] & 0x80) == 0) return 1;
	else if ((ch[0] & 0xE0) == 0xC0) return 2;
	else if ((ch[0] & 0xF0) == 0xE0) return 3;
	else if ((ch[0] & 0xF8) == 0xF0) return 4;
	else if ((ch[0] & 0xFC) == 0xF8) return 5;
	else if ((ch[0] & 0xFE) == 0xFC) return 6;
	return 0;
 }
 
textchar_t *os_next_char(oshtml_charset_id_t id,
                         const textchar_t *p, size_t len)
{
	textchar_t *nxt = (textchar_t*)p;
	nxt += utf8_charsize(p);
	if (nxt < p +len){
		return nxt;
	}
    return (textchar_t*)p;
}

/*
 *   Get the previous character in a string. 
 */
textchar_t *os_prev_char(oshtml_charset_id_t id,
                         const textchar_t *p, const textchar_t *pstart)
{
	int position = 0;
	int size = 0;
	while ((p[position] & 0xC0) == 0x80 && size < 6 && p >= pstart) {
		position--;
		size++;
	}
    return (textchar_t*)p - position;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get system information 
 */
int os_get_sysinfo(int code, void * /*param*/, long *result)
{
    //CHtmlSys_mainwin *win;
    //CHtmlPreferences *prefs;
    
    /* get the preferences object, in case we need it */
    /*if ((win = CHtmlSys_mainwin::get_main_win()) != 0)
        prefs = win->get_prefs();
    else
        prefs = 0;*/

    /* find out what kind of information they want */
    switch(code)
    {
    case SYSINFO_INTERP_CLASS:
        /* indicate that we're a full HTML interpreter */
        *result = SYSINFO_ICLASS_HTML;
        return TRUE;
        
    case SYSINFO_HTML:
    case SYSINFO_JPEG:
    case SYSINFO_PNG:
    case SYSINFO_MNG:
    case SYSINFO_MIDI:
    case SYSINFO_PNG_TRANS:
    case SYSINFO_MNG_TRANS:
        /* we support HTML, JPEG, PNG, MNG, MIDI, and PNG/MNG transparency */
        *result = TRUE;
        return TRUE;

    case SYSINFO_TEXT_COLORS:
        /* we support full RGB test colors */
        *result = SYSINFO_TXC_RGB;
        return TRUE;

    case SYSINFO_BANNERS:
        /* we support the full banner API */
        *result = TRUE;
        return TRUE;

    case SYSINFO_TEXT_HILITE:
        /* text highlighting is supported (and so much more) */
        *result = TRUE;
        return TRUE;

    case SYSINFO_PNG_ALPHA:
    case SYSINFO_MNG_ALPHA:
        /* ask the image class if it supports alpha blending */
        *result = TRUE;//CTadsImage::is_alpha_supported();
        return TRUE;

    case SYSINFO_WAV:
    case SYSINFO_WAV_MIDI_OVL:
    case SYSINFO_WAV_OVL:
    case SYSINFO_MPEG:
    case SYSINFO_MPEG2:
    case SYSINFO_MPEG3:
    case SYSINFO_OGG:
        /*
         *   We may support WAV or not, depending on DirectX availability.
         *   If WAV isn't supported, then the overlap features aren't
         *   meaningful, but return false for them anyway in that case.  We
         *   support WAV and the various overlapped playback features if and
         *   only if DirectSound is available.
         *   
         *   We use an integrated software player to play MPEG layer II and
         *   III sounds.  However, we essentially decode MPEG sounds to WAV
         *   in memory and play the resulting WAV through DirectX, so we
         *   require DirectX to play MPEG's, just as we do for WAV's.
         *   
         *   Ogg Vorbis is similar to MPEG; we support it if we have
         *   DirectSound capabilities.  
         */
        *result = TRUE;//(win != 0 && win->get_directsound() != 0);
        return TRUE;

    case SYSINFO_PREF_IMAGES:
        *result = TRUE;//(prefs != 0 && prefs->get_graphics_on() ? 1 : 0);
        return TRUE;
        
    case SYSINFO_PREF_SOUNDS:
        *result = TRUE;//(prefs != 0 && prefs->get_sounds_on() ? 1 : 0);
        return TRUE;
        
    case SYSINFO_PREF_MUSIC:
        *result = TRUE;//(prefs != 0 && prefs->get_music_on() ? 1 : 0);
        return TRUE;
        
    case SYSINFO_PREF_LINKS:
        *result = TRUE;/*(prefs != 0
                   ? (prefs->get_links_on()
                      ? 1
                      : prefs->get_links_ctrl() ? 2 : 0)
                   : 0);*/
        return TRUE;

    case SYSINFO_LINKS_HTTP:
    case SYSINFO_LINKS_FTP:
    case SYSINFO_LINKS_NEWS:
    case SYSINFO_LINKS_MAILTO:
    case SYSINFO_LINKS_TELNET:
        *result = TRUE;
        return TRUE;

    case SYSINFO_AUDIO_FADE:
    case SYSINFO_AUDIO_CROSSFADE:
        /* we can do all types of fades and cross-fades */
        *result = SYSINFO_AUDIOFADE_MPEG
                  | SYSINFO_AUDIOFADE_OGG
                  | SYSINFO_AUDIOFADE_WAV
                  | SYSINFO_AUDIOFADE_MIDI;
        return TRUE;

    default:
        /* we don't recognize other codes */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Extended OS interface: command events.
 */

/* command event status */
static unsigned int S_cmd_evt_stat[OS_CMD_LAST];

/* enable/disable a command event */
void os_enable_cmd_event(int id, unsigned int status)
{
    /* if the ID is off our map, ignore it */
    if (id < 1 || id > OS_CMD_LAST)
        return;

    /* adjust the ID to an array index */
    --id;

    /* if the EVT select bit is present, set the EVT bit */
    if ((status & OSCS_EVT_SEL) != 0)
    {
        /* clear out the old status and set the new status */
        S_cmd_evt_stat[id] &= ~OSCS_EVT_ON;
        S_cmd_evt_stat[id] |= status;
    }

    /* if the GETS select bit is present, set the GETS bit */
    if ((status & OSCS_GTS_SEL) != 0)
    {
        /* clear out the old status and set the new status */
        S_cmd_evt_stat[id] &= ~OSCS_GTS_ON;
        S_cmd_evt_stat[id] |= status;
    }
}

/* get the current status of a command event */
int oss_is_cmd_event_enabled(int id, unsigned int typ)
{
    /* 
     *   if the ID is off our map, then it's enabled by default for os_gets()
     *   and disabled for os_get_event() 
     */
    if (id < 1 || id > OS_CMD_LAST)
        return (typ & (OSCS_GTS_ON));

    /* adjust the ID to an array index */
    --id;

    /* if this ID has never been set before, apply the default settings */
    if (S_cmd_evt_stat[id] == 0)
    {
        /* 
         *   it's never been set - the default is ON for os_gets(), OFF for
         *   os_get_event() 
         */
        S_cmd_evt_stat[id] = OSCS_EVT_SEL | OSCS_GTS_SEL | OSCS_GTS_ON;
    }

    /* return the status for the selected ON bit */
    return S_cmd_evt_stat[id] & typ;
}