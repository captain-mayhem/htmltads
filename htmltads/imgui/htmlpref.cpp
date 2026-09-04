#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/htmlpref.cpp,v 1.4 1999/07/11 00:46:46 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlpref.cpp - preferences dialog
Function
  
Notes
  
Modified
  10/26/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdio.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef HTMLGUI_H
#include "htmlgui.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif
#ifndef TADSCBTN_H
#include "tadscbtn.h"
#endif
#ifndef TADSFOLDERDLG_H
#include "tadsfolderdlg.h"
#endif
#ifndef W32MAIN_H
#include "guimain.h"
#endif

#include <imgui/imgui.h>


/*
 *   Display content scale, for the hard-coded ImGui pixel sizes in this
 *   file's dialogs (window sizes, fixed button widths, SetNextItemWidth,
 *   SameLine offsets).  ImGui's ScaleAllSizes() only covers style-driven
 *   spacing; these literals need multiplying by the same factor
 *   CHtmlSysFont_win32 bakes into text.  See CTadsFont::get_dpi_scale() and
 *   migration.md 3.5a.  Negative SetNextItemWidth values that are pure
 *   "stretch to edge" sentinels (-1) are left alone; a negative value that
 *   is a pixel inset from the right edge (e.g. -90) is scaled.
 */
static inline float uisc() { return CTadsFont::get_dpi_scale(); }


/* ------------------------------------------------------------------------ */
/*
 *   Font point sizes offered in the preferences dialog.  Note that the last
 *   element is always zero to indicate the end of the list.
 */
const int CHtmlPreferences::font_pt_sizes[] =
{
    8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72, 0
};


/* ------------------------------------------------------------------------ */
/*
 *   Generic font-popup dialog - this class has some simple routines for
 *   handling property page dialogs with popups. 
 */
class CHtmlDialogFontPp: public CTadsDialogPropPage
{
public:
    CHtmlDialogFontPp(CHtmlPreferences *prefs) { prefs_ = prefs; }
    
protected:
    /* 
     *   Read the value of one of the font popups; returns true if this
     *   changes the font setting.  If 'store' is false, just checks the
     *   value without storing it.  
     */
    int read_font_popup(int id, HTML_pref_id_t prefid, int store,
                        const char *blank_val);

    /* read a font size popup and check for a change */
    int read_fontsz_popup(int id, HTML_pref_id_t prefid, int store);

    /* the main preferences object */
    CHtmlPreferences *prefs_;
};


/*
 *   read the contents of one of the font popups into a CStringBuf 
 */
int CHtmlDialogFontPp::read_font_popup(int id, HTML_pref_id_t prefid,
                                       int store, const char *blank_val)
{
    HWND ctl;
    char buf[128];
    size_t len;
    int idx;
    int changed;
    const textchar_t *oldval;

    /* get the control handle */
    ctl = GetDlgItem(handle_, id);

    /* get the selection number */
    idx = SendMessage(ctl, CB_GETCURSEL, 0, 0);

    /* if it's not a valid selection, don't make any changes */
    if (idx == CB_ERR)
        return FALSE;

    /* get the selected string */
    len = SendMessage(ctl, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)buf);

    /* get the old value */
    oldval = prefs_->get_val_str(prefid);

    /* if the old value is blank, substitute the blank_val value */
    if (oldval == 0 || strlen(oldval) == 0)
        oldval = blank_val;

    /* note whether or not this is a change */
    changed = (oldval == 0 || len != get_strlen(oldval)
               || memcmp(buf, oldval, len) != 0);

    /* store it in the specified preference property */
    if (store)
    {
        /* if the new value matches the blank_val value, make it blank */
        if (blank_val != 0 && stricmp(buf, blank_val) == 0)
        {
            buf[0] = '\0';
            len = 0;
        }

        /* store the new value in the preferences */
        prefs_->set_val_str(prefid, buf, len);
    }

    /* return change indication */
    return changed;
}

/*
 *   read the contents of one of the font popups into a CStringBuf 
 */
int CHtmlDialogFontPp::read_fontsz_popup(int id, HTML_pref_id_t prefid,
                                         int store)
{
    HWND ctl;
    int idx;
    int newptsiz;
    int oldptsiz;
    int changed;

    /* get the control handle */
    ctl = GetDlgItem(handle_, id);

    /* get the selection number */
    idx = SendMessage(ctl, CB_GETCURSEL, 0, 0);

    /* if it's not a valid selection, don't make any changes */
    if (idx == CB_ERR)
        return FALSE;

    /* get the point size (it's the extra item data for the list entry) */
    newptsiz = (int)SendMessage(ctl, CB_GETITEMDATA, (WPARAM)idx, 0);

    /* note whether this is a change */
    oldptsiz = prefs_->get_val_longint(prefid);
    changed = (newptsiz != oldptsiz);

    /* store it in the specified preference property */
    if (store && changed)
        prefs_->set_val_longint(prefid, newptsiz);

    /* return change indication */
    return changed;
}

/* ------------------------------------------------------------------------ */
/*
 *   Font dialog 
 */


class CHtmlDialogFonts: public CHtmlDialogFontPp
{
public:
    CHtmlDialogFonts(CHtmlPreferences *prefs, unsigned int charset_id,
                     int standalone)
        : CHtmlDialogFontPp(prefs)
    {
        /* remember the desired character set ID */
        charset_id_ = charset_id;

        /* remember whether or not we're a stand-alone dialog */
        standalone_ = standalone;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* callbacks to select specific parameterized font types */
    static int font_select_serif(void *, ENUMLOGFONTEX *, NEWTEXTMETRIC *);
    static int font_select_sans(void *, ENUMLOGFONTEX *, NEWTEXTMETRIC *);
    static int font_select_script(void *, ENUMLOGFONTEX *, NEWTEXTMETRIC *);
    static int font_select_typewriter(void *, ENUMLOGFONTEX *,
                                      NEWTEXTMETRIC *);

    /* initialize a point-size popup list */
    void init_ptsiz_popup(int id, int ptsiz);

    /* check for changes */
    int has_changes(int save);

    /* input text color */
    HTML_color_t input_text_color_;

    /* special popup entry for "Main text font" */
    static const char *main_font_entry;

    /* the Windows character set ID for the desired character set */
    unsigned int charset_id_;

    /* 
     *   flag: our property sheet is running as a stand-alone dialog, rather
     *   than as a sub-dialog of the main options dialog 
     */
    int standalone_;
};

/* main font entry text (for special input font name selection) */
const char *CHtmlDialogFonts::main_font_entry = "(Main Game Font)";

/*
 *   initialize 
 */
void CHtmlDialogFonts::init()
{
    COLORREF *cust_colors = prefs_->get_cust_colors();
    const char *cur_inp_font;
    HWND pop;

    /* 
     *   if we're running as a stand-alone dialog (rather than as a
     *   sub-dialog of the main options dialog), center the property sheet
     *   (which is our parent window) on the screen 
     */
    if (standalone_)
        center_dlg_win(GetParent(handle_));

    /* turn off the context-help style in the parent window */
    SetWindowLong(GetParent(handle_), GWL_EXSTYLE,
                  GetWindowLong(GetParent(handle_), GWL_EXSTYLE)
                  & ~WS_EX_CONTEXTHELP);

    /* set up the font combos with lists of available fonts */
    init_font_popup(IDC_MAINFONTPOPUP, TRUE, TRUE,
                    prefs_->get_prop_font(), TRUE, charset_id_);
    init_font_popup(IDC_MONOFONTPOPUP, FALSE, TRUE,
                    prefs_->get_mono_font(), TRUE, charset_id_);
    init_font_popup(IDC_FONTSERIF, prefs_->get_font_serif(),
                    &font_select_serif, 0, TRUE, charset_id_);
    init_font_popup(IDC_FONTSANS, prefs_->get_font_sans(),
                    &font_select_sans, 0, TRUE, charset_id_);
    init_font_popup(IDC_FONTSCRIPT, prefs_->get_font_script(),
                    &font_select_script, 0, TRUE, charset_id_);
    init_font_popup(IDC_FONTTYPEWRITER, prefs_->get_font_typewriter(),
                    &font_select_typewriter, 0, TRUE, charset_id_);

    /* load the point-size combo boxes */
    init_ptsiz_popup(IDC_POP_MAINFONTSIZE, prefs_->get_prop_fontsz());
    init_ptsiz_popup(IDC_POP_MONOFONTSIZE, prefs_->get_mono_fontsz());
    init_ptsiz_popup(IDC_POP_SERIFFONTSIZE, prefs_->get_serif_fontsz());
    init_ptsiz_popup(IDC_POP_SANSFONTSIZE, prefs_->get_sans_fontsz());
    init_ptsiz_popup(IDC_POP_SCRIPTFONTSIZE, prefs_->get_script_fontsz());
    init_ptsiz_popup(IDC_POP_TYPEWRITERFONTSIZE,
                     prefs_->get_typewriter_fontsz());
    init_ptsiz_popup(IDC_POP_INPUTFONTSIZE, prefs_->get_inpfont_size());

    /* get the current input font from the preferences */
    cur_inp_font = prefs_->get_inpfont_name();

    /* if the name is empty, use the "Main Text Font" entry by default */
    if (cur_inp_font == 0 || cur_inp_font[0] == '\0')
        cur_inp_font = main_font_entry;

    /* get the font selector control */
    pop = GetDlgItem(handle_, IDC_POP_INPUTFONT);

    /* clear out any existing list contents */
    SendMessage(pop, CB_RESETCONTENT, 0, 0);

    /* add an element to the font popup for "Main text font" */
    SendMessage(pop, CB_ADDSTRING, (WPARAM)0, (LPARAM)main_font_entry);

    /* set up the font combo with a list of the available fonts */
    init_font_popup(IDC_POP_INPUTFONT, TRUE, TRUE, cur_inp_font, FALSE,
                    charset_id_);

    /* add the color button */
    input_text_color_ = prefs_->get_inpfont_color();
    controls_.add(new CColorBtnPropPage(handle_, IDC_BTN_INPUTCOLOR,
                                        &input_text_color_,
                                        cust_colors, this));

    /* initialize the input font bold/italic style settings */
    CheckDlgButton(handle_, IDC_CK_INPUTBOLD,
                   (prefs_->get_inpfont_bold()
                    ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_INPUTITALIC,
                   (prefs_->get_inpfont_italic()
                    ? BST_CHECKED : BST_UNCHECKED));
}

/*
 *   initialize a font-size popup 
 */
void CHtmlDialogFonts::init_ptsiz_popup(int id, int ptsiz)
{
    HWND pop;
    const int *p;
    
    /* get the popup control */
    pop = GetDlgItem(handle_, id);

    /* clear out any existing list contents */
    SendMessage(pop, CB_RESETCONTENT, 0, 0);

    /* add the point size strings */
    for (p = CHtmlPreferences::font_pt_sizes ; *p != 0 ; ++p)
    {
        char buf[20];
        int idx;

        /* build the name for this item */
        sprintf(buf, "%d pt", *p);

        /* add this item to the combo */
        idx = SendMessage(pop, CB_ADDSTRING, (WPARAM)0, (LPARAM)buf);

        /* set this item's lparam to the point size */
        SendMessage(pop, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)*p);

        /* if this is the current point size, make it the selection */
        if (*p == ptsiz)
            SendMessage(pop, CB_SETCURSEL, (WPARAM)idx, 0);
    }
}

/*
 *   select serifed fonts 
 */
int CHtmlDialogFonts::font_select_serif(void *, ENUMLOGFONTEX *elf,
                                       NEWTEXTMETRIC *tm)
{
    /* return ROMAN-style fonts */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_ROMAN);
}

/*
 *   select sans-serif fonts 
 */
int CHtmlDialogFonts::font_select_sans(void *, ENUMLOGFONTEX *elf,
                                       NEWTEXTMETRIC *tm)
{
    /* return SWISS-style fonts */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_SWISS);
}

/*
 *   select script fonts 
 */
int CHtmlDialogFonts::font_select_script(void *, ENUMLOGFONTEX *elf,
                                         NEWTEXTMETRIC *tm)
{
    /* 
     *   return SCRIPT and ROMAN styles - a lot of script-type fonts seem
     *   to claim they're ROMAN fonts 
     */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && ((elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_SCRIPT
                || (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_ROMAN));
}

/*
 *   select typewriter fonts 
 */
int CHtmlDialogFonts::font_select_typewriter(void *, ENUMLOGFONTEX *elf,
                                             NEWTEXTMETRIC *tm)
{
    /* return MODERN-style fonts */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_MODERN);
}

/*
 *   receive a notification message 
 */
int CHtmlDialogFonts::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* there are changes, so reformat the main window */
            prefs_->schedule_reformat(FALSE);
        }
        
        /* handled */
        return TRUE;
    }

    return FALSE;
}

/*
 *   determine if I have any changes to reflect 
 */
int CHtmlDialogFonts::has_changes(int save)
{
    int ch;
    int new_bold, new_ital;

    /* presume no changes */
    ch = FALSE;

    /* check our popup controls for changes */
    ch |=  read_font_popup(IDC_MAINFONTPOPUP, HTML_PREF_PROP_FONT, save, 0);
    ch |=  read_font_popup(IDC_MONOFONTPOPUP, HTML_PREF_MONO_FONT, save, 0);
    ch |= read_font_popup(IDC_FONTSERIF, HTML_PREF_FONT_SERIF, save, 0);
    ch |= read_font_popup(IDC_FONTSANS, HTML_PREF_FONT_SANS, save, 0);
    ch |= read_font_popup(IDC_FONTSCRIPT, HTML_PREF_FONT_SCRIPT, save, 0);
    ch |= read_font_popup(IDC_FONTTYPEWRITER,
                          HTML_PREF_FONT_TYPEWRITER, save, 0);
    ch |= read_font_popup(IDC_POP_INPUTFONT, HTML_PREF_INPFONT_NAME, save,
                          main_font_entry);
    ch |= read_fontsz_popup(IDC_POP_MAINFONTSIZE,
                            HTML_PREF_PROP_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_MONOFONTSIZE,
                            HTML_PREF_MONO_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_SERIFFONTSIZE,
                            HTML_PREF_SERIF_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_SANSFONTSIZE,
                            HTML_PREF_SANS_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_SCRIPTFONTSIZE,
                            HTML_PREF_SCRIPT_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_TYPEWRITERFONTSIZE,
                            HTML_PREF_TYPEWRITER_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_INPUTFONTSIZE,
                            HTML_PREF_INPUT_FONTSZ, save);

    /* check for changes in the color setting */
    ch |= (prefs_->get_inpfont_color() != input_text_color_);

    /* note the new style settings */
    new_bold = (IsDlgButtonChecked(handle_, IDC_CK_INPUTBOLD)
                == BST_CHECKED);
    new_ital = (IsDlgButtonChecked(handle_, IDC_CK_INPUTITALIC)
                == BST_CHECKED);

    /* check for style changes */
    ch |= ((new_bold != prefs_->get_inpfont_bold())
           || (new_ital != prefs_->get_inpfont_italic()));

    /* if saving, store the new input text color and style settings */
    if (save && ch)
    {
        prefs_->set_inpfont_bold(new_bold);
        prefs_->set_inpfont_italic(new_ital);
        prefs_->set_inpfont_color(input_text_color_);
    }

    /* return the change indication */
    return ch;
}

/*
 *   process a command 
 */
int CHtmlDialogFonts::do_command(WPARAM cmd, HWND ctl)
{
    /* see if it's a notification that one of the popups changed selection */
    if (HIWORD(cmd) == CBN_SELCHANGE)
    {
        /* see if anything changed (but don't save changes at this point) */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
    }

    /* check for other commands */
    switch(cmd)
    {
    case IDC_CK_INPUTBOLD:
    case IDC_CK_INPUTITALIC:
        /* check for changes */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    case IDC_BTN_INPUTCOLOR:
        /* inherit default handling to activate the button */
        CHtmlDialogFontPp::do_command(cmd, ctl);

        /* check for changes now that we've run the color selector */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    default:
        /* inherit the default behavior */
        return CHtmlDialogFontPp::do_command(cmd, ctl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Color dialog
 */

class CHtmlDialogColor: public CTadsDialogPropPage
{
public:
    CHtmlDialogColor(CHtmlPreferences *prefs)
    {
        /* remember the preferences object */
        prefs_ = prefs;

        /* we haven't warned about a link status change yet */
        warned_link_change_ = FALSE;
    }
    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* add a resource string to a popup control's list */
    void add_popup_string(HWND pop, int str_id);

    /* adjust enabling status of the link-related controls */
    void adjust_link_controls();

    /* our preferences object */
    CHtmlPreferences *prefs_;

    /* current color settings */
    HTML_color_t bg_color_;
    HTML_color_t text_color_;
    HTML_color_t link_color_;
    HTML_color_t vlink_color_;
    HTML_color_t hlink_color_;
    HTML_color_t alink_color_;
    HTML_color_t stat_text_color_;
    HTML_color_t stat_bg_color_;

    /* 
     *   flag: we've warned once about a change in the link status, so don't
     *   warn any more while the dialog is open 
     */
    int warned_link_change_;
};


/* 
 *   initialize 
 */
void CHtmlDialogColor::init()
{
    int use_win_colors;
    int override_colors;
    int hover_hilite;
    COLORREF *cust_colors = prefs_->get_cust_colors();
    HWND link_pop;
    int idx;
    
    /* get initial colors from preferences */
    bg_color_ = prefs_->get_bg_color();
    text_color_ = prefs_->get_text_color();
    link_color_ = prefs_->get_link_color();
    vlink_color_ = prefs_->get_vlink_color();
    hlink_color_ = prefs_->get_hlink_color();
    alink_color_ = prefs_->get_alink_color();
    stat_text_color_ = prefs_->get_color_status_text();
    stat_bg_color_ = prefs_->get_color_status_bg();

    /* set up the color buttons */
    controls_.add(new CColorBtnPropPage(handle_, IDC_BKCOLOR,
                                        &bg_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_TXTCOLOR,
                                        &text_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_LINKCOLOR,
                                        &link_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_HLINKCOLOR,
                                        &hlink_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_ALINKCOLOR,
                                        &alink_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_SBKCOLOR,
                                        &stat_bg_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_STXTCOLOR,
                                        &stat_text_color_,
                                        cust_colors, this));

    /* initialize enabling state of text/background color buttons */
    use_win_colors = prefs_->get_use_win_colors();
    override_colors = prefs_->get_override_colors();
    CheckDlgButton(handle_, IDC_CK_USEWINCLR,
                   (use_win_colors ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_OVERRIDECLR,
                   (override_colors ? BST_CHECKED : BST_UNCHECKED));

    /* enable or disable the text and background buttons accordingly */
    EnableWindow(GetDlgItem(handle_, IDC_TXTCOLOR), !use_win_colors);
    EnableWindow(GetDlgItem(handle_, IDC_BKCOLOR), !use_win_colors);

    /* enable or disable the statusline color buttons */
    EnableWindow(GetDlgItem(handle_, IDC_STXTCOLOR), !override_colors);
    EnableWindow(GetDlgItem(handle_, IDC_SBKCOLOR), !override_colors);

    /* initialize underline checkbox */
    CheckDlgButton(handle_, IDC_CK_UNDERLINE,
                   (prefs_->get_underline_links()
                    ? BST_CHECKED : BST_UNCHECKED));

    /* initialize hover hilite checkbox */
    hover_hilite = prefs_->get_hover_hilite();
    CheckDlgButton(handle_, IDC_CK_HOVER,
                   (hover_hilite ? BST_CHECKED : BST_UNCHECKED));

    /* enable or disable the hover color accordingly */
    EnableWindow(GetDlgItem(handle_, IDC_HLINKCOLOR), hover_hilite);

    /* add the element names to the "show links" popup */
    link_pop = GetDlgItem(handle_, IDC_POP_SHOW_LINKS);
    add_popup_string(link_pop, IDS_LINKS_ALWAYS);
    add_popup_string(link_pop, IDS_LINKS_CTRL);
    add_popup_string(link_pop, IDS_LINKS_NEVER);

    /* select the appropriate item in the popup for the current settings */
    idx = (prefs_->get_links_on() ? 0 : prefs_->get_links_ctrl() ? 1 : 2);
    SendMessage(link_pop, CB_SETCURSEL, idx, 0);

    /* adjust the link-related controls for the initial state */
    adjust_link_controls();
}

/*
 *   Add a resource string to a popup's list 
 */
void CHtmlDialogColor::add_popup_string(HWND pop, int str_id)
{
    char buf[256];

    /* load the string */
    LoadString(CTadsApp::get_app()->get_instance(), str_id, buf, sizeof(buf));

    /* add it to the popup list */
    SendMessage(pop, CB_ADDSTRING, 0, (LPARAM)buf);
}

/*
 *   process notifications 
 */
int CHtmlDialogColor::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* we have changes, so reformat the main window */
            prefs_->schedule_reformat(FALSE);
        }

        /* handled */
        return TRUE;
    }
    return FALSE;
}

/*
 *   check for changes 
 */
int CHtmlDialogColor::has_changes(int save)
{
    int new_use_win_clr;
    int new_override_clr;
    int new_underline;
    int new_hover_hilite;
    int ch;
    int idx;
    int new_links_on;
    int new_links_ctrl;
    int link_ch;

    /* note new checkbox settings */
    new_use_win_clr = (IsDlgButtonChecked(handle_, IDC_CK_USEWINCLR)
                       == BST_CHECKED);
    new_override_clr = (IsDlgButtonChecked(handle_, IDC_CK_OVERRIDECLR)
                        == BST_CHECKED);
    new_underline = (IsDlgButtonChecked(handle_, IDC_CK_UNDERLINE)
                     == BST_CHECKED);
    new_hover_hilite = (IsDlgButtonChecked(handle_, IDC_CK_HOVER)
                        == BST_CHECKED);

    /* note the 'show links' popup selection */
    idx = SendMessage(GetDlgItem(handle_, IDC_POP_SHOW_LINKS),
                      CB_GETCURSEL, 0, 0);
    new_links_on = (idx == 0);
    new_links_ctrl = (idx == 1);

    /* note a change in link status */
    link_ch = (prefs_->get_links_on() != new_links_on
               || prefs_->get_links_ctrl() != new_links_ctrl);

    /* note changes in any of our colors */
    ch = (prefs_->get_use_win_colors() != new_use_win_clr
          || prefs_->get_override_colors() != new_override_clr
          || prefs_->get_bg_color() != bg_color_
          || prefs_->get_text_color() != text_color_
          || prefs_->get_link_color() != link_color_
          || prefs_->get_vlink_color() != vlink_color_
          || prefs_->get_hlink_color() != hlink_color_
          || prefs_->get_alink_color() != alink_color_
          || prefs_->get_underline_links() != new_underline
          || prefs_->get_hover_hilite() != new_hover_hilite
          || prefs_->get_color_status_text() != stat_text_color_
          || prefs_->get_color_status_bg() != stat_bg_color_
          || link_ch);
    
    /* save the changes, if desired */
    if (save && ch)
    {
        /* save the current settings */
        prefs_->set_use_win_colors(new_use_win_clr);
        prefs_->set_override_colors(new_override_clr);
        prefs_->set_bg_color(bg_color_);
        prefs_->set_text_color(text_color_);
        prefs_->set_link_color(link_color_);
        prefs_->set_vlink_color(vlink_color_);
        prefs_->set_hlink_color(hlink_color_);
        prefs_->set_alink_color(alink_color_);
        prefs_->set_underline_links(new_underline);
        prefs_->set_hover_hilite(new_hover_hilite);
        prefs_->set_color_status_text(stat_text_color_);
        prefs_->set_color_status_bg(stat_bg_color_);
        prefs_->set_links_on(new_links_on);
        prefs_->set_links_ctrl(new_links_ctrl);

        /* 
         *   if the link status is changing, notify the main window - it
         *   might want to warn about this, since in some states, disabling
         *   links will not have any immediate effect 
         */
        if (link_ch && !warned_link_change_)
        {
            /* notify the main window about it */
            prefs_->notify_link_pref_change();

            /* only notify once per dialog session */
            warned_link_change_ = TRUE;
        }
    }

    /* return the change indication */
    return ch;
}

/*
 *   process a command 
 */
int CHtmlDialogColor::do_command(WPARAM id, HWND ctl)
{
    int checked;

    switch(LOWORD(id))
    {
    case IDC_CK_USEWINCLR:
        /* get the new state of the button */
        checked = (IsDlgButtonChecked(handle_, (int)id) == BST_CHECKED);
            
        /* enable or disable the text and background buttons accordingly */
        EnableWindow(GetDlgItem(handle_, IDC_TXTCOLOR), !checked);
        EnableWindow(GetDlgItem(handle_, IDC_BKCOLOR), !checked);

        /* note the state change */
        set_changed(has_changes(FALSE));
        return TRUE;

    case IDC_CK_HOVER:
    case IDC_POP_SHOW_LINKS:
        /* note the state change */
        set_changed(has_changes(FALSE));

        /* adjust control enabling for the current mode */
        adjust_link_controls();

        /* handled */
        return TRUE;
        
    case IDC_CK_OVERRIDECLR:
        /* get the new state of the button */
        checked = (IsDlgButtonChecked(handle_, (int)id) == BST_CHECKED);

        /* enable or disable the statusline color buttons accordingly */
        EnableWindow(GetDlgItem(handle_, IDC_STXTCOLOR), !checked);
        EnableWindow(GetDlgItem(handle_, IDC_SBKCOLOR), !checked);

        /* note the state change */
        set_changed(has_changes(FALSE));
        return TRUE;

    case IDC_CK_UNDERLINE:
        /* note the state change */
        set_changed(has_changes(FALSE));
        return TRUE;

    case IDC_BKCOLOR:
    case IDC_TXTCOLOR:
    case IDC_LINKCOLOR:
    case IDC_HLINKCOLOR:
    case IDC_ALINKCOLOR:
    case IDC_SBKCOLOR:
    case IDC_STXTCOLOR:
        /* color button - inherit default handling to activate the button */
        CTadsDialogPropPage::do_command(id, ctl);

        /* check for changes now that we've run the color selector */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
        
    default:
        /* pass to superclass for processing */
        return CTadsDialog::do_command(id, ctl);
    }
}

/*
 *   Adjust the link controls for the current link enabling states 
 */
void CHtmlDialogColor::adjust_link_controls()
{
    int hover;
    int links_on;

    /* check to see if 'highlight when hovering' is checked */
    hover = (IsDlgButtonChecked(handle_, (int)IDC_CK_HOVER) == BST_CHECKED);

    /* 
     *   check to see if links are on - 'always' or 'ctrl key down' both
     *   count as on 
     */
    links_on = (SendMessage(GetDlgItem(handle_, IDC_POP_SHOW_LINKS),
                            CB_GETCURSEL, 0, 0) != 2);

    /* 
     *   Enable or disable the color buttons, underline checkbox, and hover
     *   checkbox: enable them if links are on, disable if not.  In addition,
     *   disable the hover color button if hovering isn't enabled.  
     */
    EnableWindow(GetDlgItem(handle_, IDC_CK_UNDERLINE), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_CK_HOVER), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_LINKCOLOR), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_ALINKCOLOR), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_HLINKCOLOR), links_on && hover);
}


/* ------------------------------------------------------------------------ */
/*
 *   MORE dialog 
 */

class CHtmlDialogMore: public CTadsDialogPropPage
{
public:
    CHtmlDialogMore(CHtmlPreferences *prefs) { prefs_ = prefs; }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* get the state of the Alt More Style button */
    int get_alt_more_button();

    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogMore::init()
{
    /* initialize controls with current preference settings */
    CheckRadioButton(handle_, IDC_RB_MORE_NORMAL, IDC_RB_MORE_ALT,
                     prefs_->get_alt_more_style() ?
                     IDC_RB_MORE_ALT : IDC_RB_MORE_NORMAL);
}

/*
 *   process notifications 
 */
int CHtmlDialogMore::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* the style has changed, so reformat the window */
            prefs_->schedule_reformat(TRUE);
        }

        /* handled */
        return TRUE;
    }
    return FALSE;
}

/* 
 *   get the alt-more-mode button state 
 */
int CHtmlDialogMore::get_alt_more_button()
{
    /* check the checkbox state */
    return (IsDlgButtonChecked(handle_, IDC_RB_MORE_ALT) == BST_CHECKED);
}

/*
 *   process commands 
 */
int CHtmlDialogMore::do_command(WPARAM, HWND)
{
    /* see if anything has changed */
    set_changed(has_changes(FALSE));

    /* handled */
    return TRUE;
}

/*
 *   check for changes 
 */
int CHtmlDialogMore::has_changes(int save)
{
    int ch;

    /* check for changes */
    ch = ((prefs_->get_alt_more_style() != 0)
          != (get_alt_more_button() != 0));

    /* save changes if desired */
    if (ch && save)
    {
        /* save the current selection state in the preferences */
        prefs_->set_alt_more_style(get_alt_more_button());
    }

    /* return change indication */
    return ch;
}

/* ------------------------------------------------------------------------ */
/*
 *   Media dialog 
 */

class CHtmlDialogMedia: public CTadsDialogPropPage
{
public:
    CHtmlDialogMedia(CHtmlPreferences *prefs) { prefs_ = prefs; }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogMedia::init()
{
    /* initialize controls with current preference settings */
    CheckDlgButton(handle_, IDC_CK_GRAPHICS,
                   prefs_->get_graphics_on() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_CK_SOUND_FX,
                   prefs_->get_sounds_on() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_CK_MUSIC,
                   prefs_->get_music_on() ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   process notifications 
 */
int CHtmlDialogMedia::do_notify(NMHDR *nm, int)
{
    int old_gr;

    switch(nm->code)
    {
    case PSN_APPLY:
        /* remember whether or not graphics are currently on */
        old_gr = prefs_->get_graphics_on();

        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* notify the game window that it might need to cancel sounds */
            prefs_->notify_sound_pref_change();

            /* if the graphics enabling changed, reformat the game window */
            if (old_gr != prefs_->get_graphics_on())
                prefs_->schedule_reformat(FALSE);
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   process commands 
 */
int CHtmlDialogMedia::do_command(WPARAM id, HWND ctl)
{
    switch (LOWORD(id))
    {
    case IDC_CK_GRAPHICS:
    case IDC_CK_SOUND_FX:
    case IDC_CK_MUSIC:
        /* see if anything has changed */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
    }

    /* not handled */
    return CTadsDialogPropPage::do_command(id, ctl);
}

/*
 *   check for changes 
 */
int CHtmlDialogMedia::has_changes(int save)
{
    int graphics_on;
    int sound_on;
    int music_on;
    int ch;

    /* get the current button settings */
    graphics_on = (IsDlgButtonChecked(handle_, IDC_CK_GRAPHICS)
                   == BST_CHECKED);
    sound_on = (IsDlgButtonChecked(handle_, IDC_CK_SOUND_FX)
                == BST_CHECKED);
    music_on = (IsDlgButtonChecked(handle_, IDC_CK_MUSIC)
                == BST_CHECKED);

    /* check for changed */
    ch = (prefs_->get_graphics_on() != graphics_on
          || prefs_->get_sounds_on() != sound_on
          || prefs_->get_music_on() != music_on);

    /* save the changes if desired */
    if (ch && save)
    {
        prefs_->set_graphics_on(graphics_on);
        prefs_->set_sounds_on(sound_on);
        prefs_->set_music_on(music_on);
    }

    /* return the change indication */
    return ch;
}

/* ------------------------------------------------------------------------ */
/*
 *   Appearance dialog.  
 */

/*
 *   Run the preferences dialog 
 */
void CHtmlPreferences::run_appearance_dlg(HWND hwndOwner,
                                          CHtmlWinWithPrefs *win,
                                          int standalone)
{
    PROPSHEETPAGE psp[20];
    PROPSHEETHEADER psh;
    CHtmlDialogFonts *fonts_dlg;
    CHtmlDialogColor *color_dlg;
    CHtmlDialogMore *more_dlg;
    CHtmlDialogMedia *media_dlg;
    int i;
    unsigned int charset_id;
    char title[128 + 50];

    /* remember the owning window */
    win_ = win;

    /* get the character set ID of the owner window */
    charset_id = win->get_default_charset();

    /* create our dialog objects */
    fonts_dlg = new CHtmlDialogFonts(this, charset_id, standalone);
    color_dlg = new CHtmlDialogColor(this);
    more_dlg = new CHtmlDialogMore(this);
    media_dlg = new CHtmlDialogMedia(this);

    /* 
     *   set up the page descriptors 
     */

    /* no pages yet */
    i = 0;

    /* set up the Fonts page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_FONT);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_FONT);
    fonts_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Colors page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_COLORS);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_COLORS);
    color_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the "More" Style page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_MORE);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_MORE);
    more_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Media page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_MEDIA);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_MEDIA);
    media_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE;
    psh.hwndParent = hwndOwner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    sprintf(title, "Customize \"%s\" Theme", get_active_profile_name());
    psh.pszCaption = (LPSTR)title;
    psh.nPages = i;
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* run the property sheet */
    PropertySheet(&psh);

    /* delete the dialogs */
    delete fonts_dlg;
    delete color_dlg;
    delete more_dlg;
    delete media_dlg;
}


/* ------------------------------------------------------------------------ */
/*
 *   New Profile dialog 
 */
class CTadsDialogNewProfile: public CTadsDialog
{
public:
    CTadsDialogNewProfile(char *buf, size_t buflen, HWND pop)
    {
        /* remember our profile name buffer */
        buf_ = buf;
        buflen_ = buflen;

        /* remember the listbox containing the existing names */
        pop_ = pop;
    }

    /* run the dialog modally */
    static int run_dialog(HWND parent, char *buf, size_t buflen, HWND pop)
    {
        CTadsDialogNewProfile *dlg;
        int id;

        /* create the dialog */
        dlg = new CTadsDialogNewProfile(buf, buflen, pop);

        /* run the dialog modally */
        id = dlg->run_modal(DLG_NEW_PROFILE, parent,
                            CTadsApp::get_app()->get_instance());

        /* delete the dialog */
        delete dlg;

        /* return true if we were dismissed with OK */
        return (id == IDOK);
    }

    /* process a command */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        int i;
        int cnt;

        switch (LOWORD(id))
        {
        case IDOK:
            /* make sure the name isn't too long */
            if (strlen(buf_) > 128)
            {
                /* it's too long */
                MessageBox(handle_, "This profile name is too long.  Please "
                           "choose a shorter name.", "TADS",
                           MB_OK | MB_ICONEXCLAMATION);

                /* handled - do not dismiss */
                return TRUE;
            }

            /* 
             *   make sure the name doesn't contain a backslash (we use the
             *   name as a registry key, and registry keys can't contain
             *   backslash characters) 
             */
            if (strchr(buf_, '\\') != 0)
            {
                /* explain the problem */
                MessageBox(handle_, "The character '\\' is not allowed "
                           "in a profile name.", "TADS",
                           MB_OK | MB_ICONEXCLAMATION);

                /* handled - do not dismiss */
                return TRUE;
            }

            /* scan the name list to make sure this name doesn't collide */
            cnt = SendMessage(pop_, CB_GETCOUNT, 0, 0);
            for (i = 0 ; i < cnt ; ++i)
            {
                char buf[128];

                /* get this item's name */
                SendMessage(pop_, CB_GETLBTEXT, i, (LPARAM)buf);

                /* make sure this new name is different */
                if (stricmp(buf, buf_) == 0)
                {
                    /* tell them about it */
                    MessageBox(handle_, "A profile with this name already "
                               "exists.  You must give each profile a unique "
                               "name.", "TADS", MB_OK | MB_ICONEXCLAMATION);

                    /* handled - do not dismiss the dialog */
                    return TRUE;
                }
            }

            /* inherit default handling as well, to dismiss the dialog */
            break;

        case IDC_FLD_PROFILE:
            /* see what kind of notification we're receiving */
            switch(HIWORD(id))
            {
            case EN_CHANGE:
                /* get the new field value */
                GetDlgItemText(handle_, IDC_FLD_PROFILE, buf_, buflen_);

                /* enable the "OK" button only if the field is non-empty */
                EnableWindow(GetDlgItem(handle_, IDOK),
                             (buf_[0] != '\0'));

                /* handled */
                return TRUE;
            }

            /* not handled */
            break;
        }

        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

private:
    /* 
     *   profile name buffer - we'll fill this in with the name supplied by
     *   the user if successful, and we'll use the initial contents to
     *   initialize the text field 
     */
    char *buf_;
    size_t buflen_;

    /* handle to popup containing the list of existing profiles */
    HWND pop_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Appearance dialog 
 */
class CHtmlDialogAppearance: public CTadsDialogPropPage
{
public:
    CHtmlDialogAppearance(CHtmlPreferences *prefs)
    {
        /* remember the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* update for a profile change */
    void on_profile_change();

    /* save any changes to the description field */
    void save_desc();

    /* our preferences object */
    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogAppearance::init()
{
    HWND pop;
    HKEY key;
    DWORD disposition;
    const textchar_t *active;
    char base_key[256];
    int key_idx;
    int pop_idx;

    /* inherit default initialization */
    CTadsDialogPropPage::init();

    /* center the parent dialog on the screen */
    center_dlg_win(GetParent(handle_));

    /* get the active profile */
    active = prefs_->get_active_profile_name();

    /* get the popup handle */
    pop = GetDlgItem(handle_, IDC_POP_THEME);

    /* add each profile to the popup */
    sprintf(base_key, "%s\\Profiles", w32_pref_key_name);
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, base_key,
                                  &disposition, TRUE);
    for (key_idx = 0 ; ; ++key_idx)
    {
        char subkey[128];
        DWORD len;
        FILETIME ft;
        
        /* get the next profile name from the registry */
        len = sizeof(subkey);
        if (RegEnumKeyEx(key, key_idx, subkey, &len, 0, 0, 0, &ft)
            != ERROR_SUCCESS)
            break;
        
        /* add the profile name to the popup */
        pop_idx = SendMessage(pop, CB_ADDSTRING, 0, (LPARAM)subkey);

        /* select it if it's currently active */
        if (stricmp(active, subkey) == 0)
            SendMessage(pop, CB_SETCURSEL, pop_idx, 0);
    }

    /* done with the key */
    CTadsRegistry::close_key(key);

    /* initialize controls for the current profile */
    on_profile_change();
}

/*
 *   save the profile description text
 */
void CHtmlDialogAppearance::save_desc()
{
    char buf[128];

    /* get the descriptive text from the field */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_DESC), buf, sizeof(buf));

    /* save the description in the preferences */
    prefs_->set_profile_desc(buf);
}

/*
 *   update the UI for a change in the active profile
 */
void CHtmlDialogAppearance::on_profile_change()
{
    const char *active;
    int is_std;

    /* note the active profile */
    active = prefs_->get_active_profile_name();

    /* note if it's one of the standard (pre-defined) profiles */
    is_std = CHtmlPreferences::is_standard_profile(active);
    
    /* disable the 'delete' button for standard profiles */
    EnableWindow(GetDlgItem(handle_, IDC_BTN_DELETE), !is_std);

    /* enable the 'restore to defaults' button only for standard profiles */
    EnableWindow(GetDlgItem(handle_, IDC_BTN_DEFAULT), is_std);

    /* if it's a standard profile, use the standard description for it */
    if (is_std)
        prefs_->set_std_profile_desc(active);

    /* set the profile desciption text */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_DESC),
                  prefs_->get_profile_desc());

    /* make the profile description read-only for standard profiles */
    SendMessage(GetDlgItem(handle_, IDC_FLD_DESC), EM_SETREADONLY, is_std, 0);
}

/*
 *   process notifications 
 */
int CHtmlDialogAppearance::do_notify(NMHDR *nm, int)
{
    switch (nm->code)
    {
    case PSN_KILLACTIVE:
        /* make sure we've saved any changes to the profile's description */
        save_desc();
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   process commands 
 */
int CHtmlDialogAppearance::do_command(WPARAM id, HWND ctl)
{
    HWND pop;
    int sel;
    char buf[128];

    /* note the theme popup handle, as we need it for most operations */
    pop = GetDlgItem(handle_, IDC_POP_THEME);

    /* check which control the command came from */
    switch(LOWORD(id))
    {
    case IDC_POP_THEME:
        /* it's from the theme selector popup - check what happened */
        switch(HIWORD(id))
        {
        case CBN_SELCHANGE:
            /* selection change - get the new selection */
            sel = SendMessage(pop, CB_GETCURSEL, 0, 0);
            
            /* get the name of the profile selected */
            SendMessage(pop, CB_GETLBTEXT, sel, (LPARAM)buf);

            /* save any changes to the description */
            save_desc();

            /* save the outgoing profile */
            prefs_->save();

            /* activate this profile */
            prefs_->get_game_win()->set_game_specific_profile(buf);

            /* update for the profile change */
            on_profile_change();

            /* handled */
            return TRUE;
        }

        /* not handled */
        break;

    case IDC_BTN_NEW:
        /* run the New Profile dialog */
        buf[0] = '\0';
        if (CTadsDialogNewProfile::run_dialog(
            GetParent(handle_), buf, sizeof(buf),
            GetDlgItem(handle_, IDC_POP_THEME)))
        {
            /* insert the profile name into the popup and select it */
            sel = SendMessage(pop, CB_ADDSTRING, 0, (LPARAM)buf);
            SendMessage(pop, CB_SETCURSEL, sel, 0);

            /* save changes to the description */
            save_desc();

            /* save our current settings */
            prefs_->save();

            /* 
             *   save a copy of the current settings under the new profile
             *   name, to create the new profile, then switch to the new
             *   profile 
             */
            prefs_->save_as(buf);
            prefs_->get_game_win()->set_game_specific_profile(buf);

            /* the new profile initially has no descriptive text */
            prefs_->set_profile_desc("");
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DESC), "");

            /* update the buttons for the selection change */
            on_profile_change();
        }

        /* handled */
        return TRUE;

    case IDC_BTN_DELETE:
        /* make sure they mean it */
        if (MessageBox(handle_, "Are you sure you want to delete this "
                       "theme and discard all of its settings?",
                       "TADS", MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            const char *active;
            int cnt;
            
            /* get the current selection from the popup */
            sel = SendMessage(pop, CB_GETCURSEL, 0, 0);

            /* delete the profile from the popup list */
            SendMessage(pop, CB_DELETESTRING, sel, 0);

            /* get the registry key name for the current profile */
            active = prefs_->get_active_profile_name();
            prefs_->get_settings_key_for(buf, sizeof(buf), active);

            /* delete the key */
            RegDeleteKey(HKEY_CURRENT_USER, buf);

            /* 
             *   select the next listbox item, or the previous if that was
             *   the last one 
             */
            cnt = SendMessage(pop, CB_GETCOUNT, 0, 0);
            if (sel >= cnt)
                sel = cnt - 1;
            SendMessage(pop, CB_SETCURSEL, sel, 0);

            /* get the new selected profile */
            sel = SendMessage(pop, CB_GETCURSEL, 0, 0);
            SendMessage(pop, CB_GETLBTEXT, sel, (LPARAM)buf);

            /* activate this profile */
            prefs_->get_game_win()->set_game_specific_profile(buf);

            /* update the buttons for the selection change */
            on_profile_change();
        }

        /* handled */
        return TRUE;

    case IDC_BTN_CUSTOMIZE:
        /* run the visual settings dialog */
        prefs_->run_appearance_dlg(GetParent(handle_),
                                   prefs_->get_game_win(), FALSE);

        /* handled */
        return TRUE;

    case IDC_BTN_DEFAULT:
        /* make sure they really want to proceed */
        if (MessageBox(GetParent(handle_),
                       "Resetting will discard any customizations you've "
                       "made to this theme's fonts, colors, and other "
                       "visual settings.  Are you sure you want to "
                       "discard your changes and reset the theme to its "
                       "default settings?", "TADS",
                       MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            /* reset the theme */
            prefs_->set_theme_defaults(prefs_->get_active_profile_name());

            /* save the changes */
            prefs_->save();

            /* notify the game window that everything has changed */
            prefs_->schedule_reformat(FALSE);
            prefs_->notify_sound_pref_change();
        }

        /* handled */
        return TRUE;
    }

    /* no special handling - inherit default handling */
    return CTadsDialogPropPage::do_command(id, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Preferences object
 */


/* custom color value name */
const textchar_t CHtmlPreferences::custclr_val_name[] =
    "Custom Colors";


/*
 *   construct 
 */
CHtmlPreferences::CHtmlPreferences()
{
    /* set an initial reference */
    refcnt_ = 1;
    
    /* create the property list */
    proplist_ = new CHtmlPropList();

    /* initialize the property names (for serialization purposes) */
    proplist_->set_prop_name(HTML_PREF_PROP_FONT, "Proportional Font");
    proplist_->set_prop_name(HTML_PREF_MONO_FONT, "Monospaced Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SERIF, "TADS-Serif Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SANS, "TADS-Sans Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SCRIPT, "TADS-Script Font");
    proplist_->set_prop_name(HTML_PREF_FONT_TYPEWRITER,
                             "TADS-Typewriter Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SCALE, "Font Scale");
    proplist_->set_prop_name(HTML_PREF_USE_WIN_COLORS, "Use Windows Colors");
    proplist_->set_prop_name(HTML_PREF_OVERRIDE_COLORS, "Override Colors");
    proplist_->set_prop_name(HTML_PREF_TEXT_COLOR, "Text Color");
    proplist_->set_prop_name(HTML_PREF_BG_COLOR, "Background Color");
    proplist_->set_prop_name(HTML_PREF_LINK_COLOR, "Link Color");
    proplist_->set_prop_name(HTML_PREF_HLINK_COLOR, "Hovering Link Color");
    proplist_->set_prop_name(HTML_PREF_ALINK_COLOR, "Active Link Color");
    proplist_->set_prop_name(HTML_PREF_VLINK_COLOR, "Visited Link Color");
    proplist_->set_prop_name(HTML_PREF_UNDERLINE_LINKS, "Underline Links");
    proplist_->set_prop_name(HTML_PREF_EMACS_CTRL_V, "Emacs Ctrl-V");
    proplist_->set_prop_name(HTML_PREF_EMACS_ALT_V, "Emacs Alt-V");
    proplist_->set_prop_name(HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL,
                            "Arrows Keys Always Scroll");
    proplist_->set_prop_name(HTML_PREF_ALT_MORE_STYLE, "Alt More Style");
    proplist_->set_prop_name(HTML_PREF_WINPOS, "Window Position");
    proplist_->set_prop_name(HTML_PREF_SUBWINPOS, "Child Window Position");
    proplist_->set_prop_name(HTML_PREF_DBGWINPOS, "Debug Window Position");
    proplist_->set_prop_name(HTML_PREF_DBGMAINPOS,
                             "Debug Main Window Position");
    proplist_->set_prop_name(HTML_PREF_MUTE_SOUND, "Mute All Sounds");
    proplist_->set_prop_name(HTML_PREF_MUSIC_ON, "Music");
    proplist_->set_prop_name(HTML_PREF_SOUNDS_ON, "Sound Effects");
    proplist_->set_prop_name(HTML_PREF_GRAPHICS_ON, "Graphics");
    proplist_->set_prop_name(HTML_PREF_LINKS_ON, "Show Links");
    proplist_->set_prop_name(HTML_PREF_LINKS_CTRL, "Show Links on Ctrl");
    proplist_->set_prop_name(HTML_PREF_DIRECTX_HIDEWARN,
                             "Hide DirectX Startup Warning");
    proplist_->set_prop_name(HTML_PREF_DIRECTX_ERROR_CODE,
                             "DirectX Initialization Result");
    proplist_->set_prop_name(HTML_PREF_FILE_SAFETY_LEVEL,
                             "File Safety Level");
    proplist_->set_prop_name(HTML_PREF_FILE_SAFETY_READ,
                             "File Safety Level Read");
    proplist_->set_prop_name(HTML_PREF_FILE_SAFETY_WRITE,
                             "File Safety Level Write");
    proplist_->set_prop_name(HTML_PREF_NET_CLIENT_SAFETY,
                             "Network Client Safety Level");
    proplist_->set_prop_name(HTML_PREF_NET_SERVER_SAFETY,
                             "Network Server Safety Level");
    proplist_->set_prop_name(HTML_PREF_COLOR_STATUS_BG,
                             "Status line Background Color");
    proplist_->set_prop_name(HTML_PREF_COLOR_STATUS_TEXT,
                             "Status line Text Color");
    proplist_->set_prop_name(HTML_PREF_INPFONT_DEFAULT,
                             "Input Font Use Default");
    proplist_->set_prop_name(HTML_PREF_INPFONT_NAME,
                             "Input Font Name");
    proplist_->set_prop_name(HTML_PREF_INPFONT_COLOR,
                             "Input Font Color");
    proplist_->set_prop_name(HTML_PREF_INPFONT_BOLD,
                             "Input Font Bold");
    proplist_->set_prop_name(HTML_PREF_INPFONT_ITALIC,
                             "Input Font Italic");
    proplist_->set_prop_name(HTML_PREF_MEM_TEXT_LIMIT,
                             "Text Memory Limit");
    proplist_->set_prop_name(HTML_PREF_CLOSE_ACTION,
                             "Window-Close Action");
    proplist_->set_prop_name(HTML_PREF_POSTQUIT_ACTION,
                             "Post-Quit Action");
    proplist_->set_prop_name(HTML_PREF_INIT_ASK_GAME,
                             "Ask for Game at Startup");
    proplist_->set_prop_name(HTML_PREF_INIT_OPEN_FOLDER,
                             "Initial Open Folder");
    proplist_->set_prop_name(HTML_PREF_RECENT_1, "Recent Game 1");
    proplist_->set_prop_name(HTML_PREF_RECENT_2, "Recent Game 2");
    proplist_->set_prop_name(HTML_PREF_RECENT_3, "Recent Game 3");
    proplist_->set_prop_name(HTML_PREF_RECENT_4, "Recent Game 4");
    proplist_->set_prop_name(HTML_PREF_RECENT_ORDER, "Recent Game Order");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_1, "Recent Debug Game 1");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_2, "Recent Debug Game 2");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_3, "Recent Debug Game 3");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_4, "Recent Debug Game 4");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_ORDER,
                             "Recent Debug Game Order");
    proplist_->set_prop_name(HTML_PREF_TOOLBAR_VIS, "Show Toolbar");
    proplist_->set_prop_name(HTML_PREF_SHOW_TIMER, "Show Timer");
    proplist_->set_prop_name(HTML_PREF_SHOW_TIMER_SECONDS,
                             "Show Seconds in Timer");
    proplist_->set_prop_name(HTML_PREF_PROP_FONTSZ, "Proportional Font Size");
    proplist_->set_prop_name(HTML_PREF_MONO_FONTSZ, "Monospaced Font Size");
    proplist_->set_prop_name(HTML_PREF_SERIF_FONTSZ, "TADS-Serif Font Size");
    proplist_->set_prop_name(HTML_PREF_SANS_FONTSZ, "TADS-Sans Font Size");
    proplist_->set_prop_name(HTML_PREF_SCRIPT_FONTSZ,
                             "TADS-Script Font Size");
    proplist_->set_prop_name(HTML_PREF_TYPEWRITER_FONTSZ,
                             "TADS-Typewriter Font Size");
    proplist_->set_prop_name(HTML_PREF_INPUT_FONTSZ, "Input Font Size");
    proplist_->set_prop_name(HTML_PREF_GC_DATABASE, "Game Chest Database");
    proplist_->set_prop_name(HTML_PREF_GC_BKG, "Game Chest Background Image");
    proplist_->set_prop_name(HTML_PREF_PROFILE_DESC, "Profile Description");
    proplist_->set_prop_name(HTML_PREF_DEFAULT_PROFILE, "Default Profile");

    /* 
     *   Mark certain properties as global, so that they're the same in all
     *   profiles.  The settings that are local are the fonts, colors, MORE
     *   style, music/sound on/off, graphics on/off, and link visibility;
     *   everything else is global.  
     */
    proplist_->get_prop(HTML_PREF_FONT_SCALE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_EMACS_CTRL_V)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_EMACS_ALT_V)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_WINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_SUBWINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DBGWINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DBGMAINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_MUTE_SOUND)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DIRECTX_HIDEWARN)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DIRECTX_ERROR_CODE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_FILE_SAFETY_LEVEL)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_FILE_SAFETY_READ)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_FILE_SAFETY_WRITE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_NET_CLIENT_SAFETY)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_NET_SERVER_SAFETY)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_MEM_TEXT_LIMIT)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_CLOSE_ACTION)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_POSTQUIT_ACTION)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_INIT_ASK_GAME)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_INIT_OPEN_FOLDER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_1)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_2)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_3)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_4)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_ORDER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_1)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_2)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_3)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_4)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_ORDER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_TOOLBAR_VIS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER_SECONDS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_GC_DATABASE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_GC_BKG)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DEFAULT_PROFILE)->set_global(TRUE);

    /* 
     *   Mark certain properties as unsynchronized (i.e., last saved change
     *   wins).  These properties are not resynchronized on UI activation,
     *   but are merely stored when we save our changes, ignoring any changes
     *   made by other instances.  
     */
    proplist_->get_prop(HTML_PREF_FONT_SCALE)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_MUTE_SOUND)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_WINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_SUBWINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_DBGWINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_DBGMAINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_TOOLBAR_VIS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER_SECONDS)
        ->set_synchronized(FALSE);

    /* if the standard profiles don't exist, initialize them to defaults */
    init_standard_profiles();

    /* register or look up our global "preferences updated" message ID */
    prefs_updated_msg_ = RegisterWindowMessage("htmltads.prefs_updated_msg");
}

/* 
 *   delete 
 */
CHtmlPreferences::~CHtmlPreferences()
{
    /* delete the property list */
    delete proplist_;
}

/*
 *   Establish the theme settings for a standard profile.  
 */
void CHtmlPreferences::set_theme_defaults(const textchar_t *theme)
{
    /* set the standard description for the theme */
    set_std_profile_desc(theme);

    /* establish the base font settings */
    set_prop_font("Times New Roman");
    set_prop_fontsz(12);
    set_mono_font("Courier New");
    set_mono_fontsz(11);
    set_font_serif("Times New Roman");
    set_serif_fontsz(12);
    set_font_sans("Arial");
    set_sans_fontsz(11);
    set_font_script("Comic Sans MS");
    set_script_fontsz(11);
    set_font_typewriter("Courier New");
    set_typewriter_fontsz(11);

    /* use game font for input font by default, but make it bold */
    set_inpfont_name("");
    set_inpfont_size(get_prop_fontsz());
    set_inpfont_color(COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOWTEXT)));
    set_inpfont_bold(TRUE);
    set_inpfont_italic(FALSE);

    /* use system colors and the standard web browser color scheme */
    set_use_win_colors(TRUE);
    set_override_colors(FALSE);
    set_text_color(COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOWTEXT)));
    set_bg_color(COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOW)));

    /* 
     *   use black text on light gray ("silver," in html parlance) background
     *   for default status line color scheme 
     */
    set_color_status_text(0x000000);
    set_color_status_bg(0xc0c0c0);

    /* use the "alternative" more style */
    set_alt_more_style(TRUE);

    /* turn on music, sound effects, and graphics */
    set_music_on(TRUE);
    set_sounds_on(TRUE);
    set_graphics_on(TRUE);

    /* enable hyperlinks, and use the standard web browser look */
    set_links_on(TRUE);
    set_links_ctrl(FALSE);
    set_link_color(HTML_make_color(0x00, 0x00, 0xff));
    set_vlink_color(HTML_make_color(0x00, 0x80, 0x80));
    set_alink_color(HTML_make_color(0xff, 0x00, 0xff));
    set_hlink_color(HTML_make_color(0xff, 0x00, 0x00));
    set_underline_links(TRUE);
    set_hover_hilite(FALSE);

    /* apply overrides for the Web Style theme */
    if (stricmp(theme, "Web Style") == 0)
    {
        /* make some changes to the base fonts for this theme */
        set_prop_font("Verdana");
        set_prop_fontsz(11);
        set_font_sans("Verdana");
        set_sans_fontsz(11);
        set_mono_fontsz(12);
        set_typewriter_fontsz(12);
    }

    /* apply overrides for the Plain Text theme */
    if (stricmp(theme, "Plain Text") == 0)
    {
        /* make some changes to the base fonts */
        set_prop_font(get_mono_font());
        set_prop_fontsz(get_mono_fontsz());
        set_inpfont_size(get_mono_fontsz());
        set_serif_fontsz(11);
        set_script_fontsz(11);
        set_sans_fontsz(10);
        set_inpfont_name("");
        set_inpfont_color(HTML_make_color(0xff, 0xff, 0xff));
        set_inpfont_bold(FALSE);
        set_inpfont_italic(FALSE);

        /* use a special white-on-navy color scheme */
        set_use_win_colors(FALSE);
        set_text_color(HTML_make_color(0xc0, 0xc0, 0xc0));
        set_bg_color(HTML_make_color(0x00, 0x00, 0x80)); 
        set_color_status_text(HTML_make_color(0x00, 0x00, 0x80));
        set_color_status_bg(HTML_make_color(0xc0, 0xc0, 0xc0));
        set_link_color(HTML_make_color(0xc0, 0xff, 0xff));
        set_vlink_color(HTML_make_color(0xc0, 0xff, 0xff));
        set_alink_color(HTML_make_color(0xff, 0x00, 0xff));
        set_hlink_color(HTML_make_color(0xff, 0x00, 0x80));
    }
}


/*
 *   Initialize a standard profile 
 */
void CHtmlPreferences::init_standard_profiles()
{
    CHtmlRect rc;
    char fname[OSFNMAX];

    /* set the basic global defaults */
    set_font_scale(2);
    set_emacs_ctrl_v(FALSE);
    set_emacs_alt_v(FALSE);
    set_arrow_keys_always_scroll(FALSE);
    set_mute_sound(FALSE);
    set_directx_hidewarn(FALSE);
    set_directx_error_code(HTMLW32_DIRECTX_OK);
    set_mem_text_limit(65536);
    set_close_action(HTML_PREF_CLOSE_PROMPT);
    set_postquit_action(HTML_PREF_POSTQUIT_KEEP);
    set_init_ask_game(FALSE);
    set_init_open_folder("");
    set_recent_game('0', "");
    set_recent_game('1', "");
    set_recent_game('2', "");
    set_recent_game('3', "");
    set_recent_game_order("0123");
    set_recent_dbg_game('0', "");
    set_recent_dbg_game('1', "");
    set_recent_dbg_game('2', "");
    set_recent_dbg_game('3', "");
    set_recent_dbg_game_order("0123");
    set_toolbar_vis(TRUE);
    set_show_timer(TRUE);
    set_show_timer_seconds(TRUE);
    set_default_profile("Multimedia");

    /* set up default initial window positions */
    rc.set(50, 50, 500, 600);
    set_win_pos(&rc);
    set_dbgwin_pos(&rc);
    rc.set(25, 25, 400, 50);
    set_dbgmain_pos(&rc);
    rc.set(75, 75, 300, 400);
    set_subwin_pos(&rc);

    /* 
     *   By default, use the safety settings in the preferences (so don't use
     *   a temporary setting).  The run-time will set a temporary setting if
     *   the user specified a command line setting.  Set all of the safety
     *   levels initially to -1, so that we'll be able to tell which ones we
     *   restored.  
     */
    temp_file_safety_level_set_ = FALSE;
    set_file_safety_level(-1, -1);
    set_val_longint(HTML_PREF_FILE_SAFETY_LEVEL, -1);

    /* use 'localhost' network safety by default */
    set_net_safety_level(1, 1);
    temp_net_safety_level_set_ = FALSE;

    /* by default, the Game Chest database file goes in My Documents\TADS */
    if (CTadsApp::get_my_docs_path(fname, sizeof(fname)))
    {
        /* build the full filename */
        PathAppend(fname, "TADS\\GameChest.txt");
    }
    else
    {
        /* can't get the My Documents path - use the current directory */
        strcpy(fname, "GameChest.txt");
    }
    set_gc_database(fname);

    /* set the Game Chest background image to the built-in default */
    set_gc_bkg("exe:gamechest/bkg.png");

    /* set the default settings for the "Multimedia" profile first */
    set_theme_defaults("Multimedia");

    /*
     *   Load any old-style, pre-profile preference settings.  This will
     *   ensure that if the user has just upgraded from a pre-profile
     *   version, we'll carry their settings forward into the new standard
     *   profiles.  Loading a null profile loads the pre-profile settings.
     *   Note that it's important that we do this *after* initializing the
     *   defaults: if there are any old settings, this will load them over
     *   the defaults, otherwise it'll just leave the defaults as they are.  
     */
    restore_as(0, FALSE);

    /* 
     *   The current preferences in memory are now set up the way we want
     *   them by default for the Multimedia profile: we've set all of the
     *   basic defaults, and we've loaded any existing pre-profile
     *   preferences over those, so we either have the Multimedia defaults or
     *   the user's old pre-profile settings.  Save these as the new
     *   Multimedia profile, if that profile doesn't already exist.  
     */
    if (!profile_exists("Multimedia"))
        save_as("Multimedia");

    /* if the Web Style profile doesn't exist, create it */
    if (!profile_exists("Web Style"))
    {
        /* re-initialize with the Web Style settings */
        set_theme_defaults("Web Style");

        /* save the settings */
        save_as("Web Style");
    }

    /* if the Plain Text theme doesn't exist, create it */
    if (!profile_exists("Plain Text"))
    {
        /* load the defaults for the Plain Text theme */
        set_theme_defaults("Plain Text");
        
        /* save the settings */
        save_as("Plain Text");
    }
}

/*
 *   Set the current profile description to the profile description of one of
 *   the standard pre-defined profiles.  
 */
void CHtmlPreferences::set_std_profile_desc(const textchar_t *profile)
{
    int id;
    char buf[256];

    /* get the resource string ID for the desired standard profile */
    if (stricmp(profile, "Multimedia") == 0)
        id = IDS_THEMEDESC_MULTIMEDIA;
    else if (stricmp(profile, "Plain Text") == 0)
        id = IDS_THEMEDESC_PLAIN_TEXT;
    else if (stricmp(profile, "Web Style") == 0)
        id = IDS_THEMEDESC_WEB_STYLE;
    else
        return;

    /* load the selected resource string */
    LoadString(CTadsApp::get_app()->get_instance(), id, buf, sizeof(buf));

    /* save it as the current profile description */
    set_profile_desc(buf);
}



/*
 *   Run the profiles dialog.  This is just like the preferences dialog, but
 *   only shows the "Appearance" tab for managing profiles.  
 */
void CHtmlPreferences::run_profiles_dlg(HWND hwndOwner,
                                        CHtmlWinWithPrefs *win)
{
    PROPSHEETPAGE psp[5];
    PROPSHEETHEADER psh;
    CHtmlDialogAppearance *appearance_dlg;
    int i;

    /* remember the owning window */
    win_ = win;

    /* create our dialog objects */
    appearance_dlg = new CHtmlDialogAppearance(this);

    /* 
     *   set up the page descriptors 
     */

    /* no pages yet */
    i = 0;

    /* set up the Appearance page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_APPEARANCE);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_APPEARANCE);
    appearance_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE;
    psh.hwndParent = hwndOwner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Manage Themes";
    psh.nPages = i;
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* run the property sheet */
    PropertySheet(&psh);

    /* delete the dialogs */
    delete appearance_dlg;
}


/* ------------------------------------------------------------------------ */
/*
 *   ImGui-native "Options" dialog (guit3).  Replaces the Win32 property
 *   sheet run_preferences_dlg() shows (that function, and the
 *   CHtmlDialog{Appearance,Keys,Safety,NetSafety,Mem,Quit,Start,GameChest}
 *   property-page classes above, are left in place unused rather than
 *   removed - same reasoning as the never-removed native menu elsewhere in
 *   the port: they're harmless dead code once nothing calls them).
 *
 *   Each control below writes straight through to the preferences object
 *   the instant it changes - there's no separate "Apply" staging step like
 *   the original PSN_APPLY-driven pages had, matching how the rest of the
 *   already-ported ImGui chrome (menu bar, toolbar) works.
 *
 *   A few sub-dialogs reachable from here are deliberately left native for
 *   now: "Customize Theme..." (the Fonts/Colors/More/Media property sheet,
 *   run_appearance_dlg()) and the folder/file browse buttons (Starting tab,
 *   Game Chest tab).  These all block via their own self-pumping Windows
 *   modal loop (PropertySheet()/GetOpenFileName()/CTadsDialogFolderSel2) -
 *   unlike the app's own WM_COMMAND-routed main frame, they never depended
 *   on guit3's (nonexistent) main message pump, so they still work
 *   correctly when invoked from an ImGui button handler.  Porting them to
 *   ImGui is future work, not a correctness fix.
 */

/*
 *   Re-enumerate the available themes/profiles from the registry into
 *   opt_profile_names_/opt_profile_count_, and re-sync opt_profile_sel_ to
 *   whichever entry matches the currently active profile (falling back to
 *   the first entry if the active profile no longer exists, e.g. mid-way
 *   through deleting it).
 */
void CHtmlPreferences::opt_refresh_profile_list()
{
    HKEY key;
    DWORD disposition;
    char base_key[256];
    const textchar_t *active;
    int i;

    opt_profile_count_ = 0;

    sprintf(base_key, "%s\\Profiles", w32_pref_key_name);
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, base_key,
                                  &disposition, TRUE);
    for (i = 0 ;
         opt_profile_count_ < (int)(sizeof(opt_profile_names_)
                                     / sizeof(opt_profile_names_[0])) ;
         ++i)
    {
        char subkey[128];
        DWORD len;
        FILETIME ft;

        len = sizeof(subkey);
        if (RegEnumKeyEx(key, i, subkey, &len, 0, 0, 0, &ft) != ERROR_SUCCESS)
            break;

        strcpy(opt_profile_names_[opt_profile_count_], subkey);
        ++opt_profile_count_;
    }
    CTadsRegistry::close_key(key);

    active = get_active_profile_name();
    opt_profile_sel_ = -1;
    for (i = 0 ; i < opt_profile_count_ ; ++i)
    {
        if (stricmp(opt_profile_names_[i], active) == 0)
        {
            opt_profile_sel_ = i;
            break;
        }
    }
    if (opt_profile_sel_ < 0 && opt_profile_count_ > 0)
        opt_profile_sel_ = 0;
}

/*
 *   Refresh opt_desc_ (and the standard-profile description, if the newly
 *   active profile is one of the pre-defined ones) for the currently active
 *   profile.  Mirrors CHtmlDialogAppearance::on_profile_change().
 */
void CHtmlPreferences::opt_on_profile_change()
{
    const char *active = get_active_profile_name();

    if (is_standard_profile(active))
        set_std_profile_desc(active);

    strncpy(opt_desc_, get_profile_desc(), sizeof(opt_desc_) - 1);
    opt_desc_[sizeof(opt_desc_) - 1] = '\0';
}

/*
 *   Open the Options dialog: snapshot all the current preference values
 *   into the dialog's working state and mark it pending-open.
 */
void CHtmlPreferences::open_options_dialog(HWND owner, CHtmlWinWithPrefs *win)
{
    /* remember the owning window, as the native dialog did */
    win_ = win;
    opt_owner_hwnd_ = owner;

    opt_refresh_profile_list();
    opt_on_profile_change();

    opt_emacs_ctrl_v_ = get_emacs_ctrl_v();
    opt_arrow_scroll_ = get_arrow_keys_always_scroll();
    opt_emacs_alt_v_ = get_emacs_alt_v();

    get_file_safety_level(&opt_safety_read_, &opt_safety_write_);
    get_net_safety_level(&opt_net_client_, &opt_net_server_);

    opt_mem_idx_ = (int)(get_mem_text_limit() / (unsigned long)32768);
    if (opt_mem_idx_ > 4)
        opt_mem_idx_ = 4;

    opt_ask_game_ = get_init_ask_game();
    strncpy(opt_init_folder_, get_init_open_folder(),
            sizeof(opt_init_folder_) - 1);
    opt_init_folder_[sizeof(opt_init_folder_) - 1] = '\0';

    opt_close_action_ = get_close_action();
    opt_postquit_action_ = get_postquit_action();

    strncpy(opt_gc_file_, get_gc_database(), sizeof(opt_gc_file_) - 1);
    opt_gc_file_[sizeof(opt_gc_file_) - 1] = '\0';
    strncpy(opt_gc_bkg_, get_gc_bkg(), sizeof(opt_gc_bkg_) - 1);
    opt_gc_bkg_[sizeof(opt_gc_bkg_) - 1] = '\0';

    opt_new_profile_name_[0] = '\0';
    opt_new_profile_err_[0] = '\0';
    opt_new_profile_popup_pending_ = false;

    opt_dlg_open_ = true;
    opt_dlg_want_open_ = true;
}

/*
 *   Draw the Options dialog for the current ImGui frame.  Safe to call
 *   unconditionally every frame; it's a no-op once the dialog is closed.
 */
void CHtmlPreferences::render_options_dialog()
{
    if (!opt_dlg_open_)
        return;

    if (opt_dlg_want_open_)
    {
        ImGui::OpenPopup("Options");
        opt_dlg_want_open_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(480 * uisc(), 420 * uisc()), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Options", &opt_dlg_open_,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        if (ImGui::BeginTabBar("OptionsTabs"))
        {
            if (ImGui::BeginTabItem("Appearance"))
            {
                opt_render_appearance_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Keyboard"))
            {
                opt_render_keys_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("File Safety"))
            {
                opt_render_filesafety_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Network Safety"))
            {
                opt_render_netsafety_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Memory"))
            {
                opt_render_mem_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Starting"))
            {
                opt_render_start_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Quitting"))
            {
                opt_render_quit_tab();
                ImGui::EndTabItem();
            }
            if (CHtmlSys_mainwin::is_game_chest_present()
                && ImGui::BeginTabItem("Game Chest"))
            {
                opt_render_gamechest_tab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(80 * uisc(), 0)))
        {
            opt_dlg_open_ = false;
            ImGui::CloseCurrentPopup();
        }

        /*
         *   Draw the folder picker on top, if the Starting tab's Browse
         *   button opened one. This has to happen here, nested inside our
         *   own still-open BeginPopupModal, rather than from a top-level
         *   call in CHtmlSys_mainwin::do_render() - see the "why render()
         *   has to be called from here" comment in tadsfolderdlg.h.
         */
        CTadsFolderDialog::render();

        ImGui::EndPopup();
    }
}

/*
 *   Appearance tab: theme/profile picker, description, and buttons to
 *   create/delete themes or jump to the (still-native) Customize Theme and
 *   Reset to Defaults flows.  Mirrors CHtmlDialogAppearance.
 */
void CHtmlPreferences::opt_render_appearance_tab()
{
    ImGui::TextWrapped(
        "A theme is a set of font, color, and other visual settings.  "
        "Each game remembers its theme, so you can use different themes "
        "for different games.");
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Theme:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200 * uisc());
    if (ImGui::BeginCombo("##ThemePicker",
                          opt_profile_sel_ >= 0
                          ? opt_profile_names_[opt_profile_sel_] : ""))
    {
        for (int i = 0 ; i < opt_profile_count_ ; ++i)
        {
            bool sel = (i == opt_profile_sel_);
            if (ImGui::Selectable(opt_profile_names_[i], sel))
            {
                /* save the outgoing profile's description, then switch */
                set_profile_desc(opt_desc_);
                save();

                opt_profile_sel_ = i;
                win_->set_game_specific_profile(opt_profile_names_[i]);
                opt_on_profile_change();
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("New..."))
    {
        opt_new_profile_name_[0] = '\0';
        opt_new_profile_err_[0] = '\0';
        opt_new_profile_popup_pending_ = true;
    }

    bool is_std = opt_profile_sel_ >= 0
        && is_standard_profile(opt_profile_names_[opt_profile_sel_]);

    ImGui::SameLine();
    ImGui::BeginDisabled(is_std);
    if (ImGui::Button("Delete"))
        ImGui::OpenPopup("Delete Theme?");
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal("Delete Theme?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("Are you sure you want to delete this theme "
                            "and discard all of its settings?");
        if (ImGui::Button("Yes", ImVec2(80 * uisc(), 0)))
        {
            char keybuf[256];
            const char *active = get_active_profile_name();
            get_settings_key_for(keybuf, sizeof(keybuf), active);
            RegDeleteKey(HKEY_CURRENT_USER, keybuf);

            opt_refresh_profile_list();
            if (opt_profile_sel_ >= 0)
            {
                win_->set_game_specific_profile(
                    opt_profile_names_[opt_profile_sel_]);
                opt_on_profile_change();
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80 * uisc(), 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Text("Description:");
    ImGui::BeginDisabled(is_std);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##ThemeDesc", opt_desc_, sizeof(opt_desc_)))
        set_profile_desc(opt_desc_);
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (ImGui::Button("Customize Theme...", ImVec2(140 * uisc(), 0)))
        open_customize_theme_dialog(opt_owner_hwnd_, win_);
    ImGui::SameLine();
    ImGui::TextWrapped("This lets you customize the fonts, colors, and "
                        "other visual settings of the selected theme.");

    ImGui::Spacing();
    ImGui::BeginDisabled(!is_std);
    if (ImGui::Button("Reset to Defaults", ImVec2(140 * uisc(), 0)))
        ImGui::OpenPopup("Reset Theme?");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextWrapped("If a standard, pre-defined theme is selected, you "
                        "can reset the theme to its factory defaults.  Use "
                        "this if you've made changes you don't want to "
                        "keep.");

    if (ImGui::BeginPopupModal("Reset Theme?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "Resetting will discard any customizations you've made to "
            "this theme's fonts, colors, and other visual settings.  Are "
            "you sure you want to discard your changes and reset the "
            "theme to its default settings?");
        if (ImGui::Button("Yes", ImVec2(80 * uisc(), 0)))
        {
            set_theme_defaults(get_active_profile_name());
            save();
            schedule_reformat(FALSE);
            notify_sound_pref_change();
            opt_on_profile_change();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80 * uisc(), 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    /* "New Theme" name-entry popup, replacing CTadsDialogNewProfile */
    if (opt_new_profile_popup_pending_)
    {
        ImGui::OpenPopup("New Theme");
        opt_new_profile_popup_pending_ = false;
    }
    if (ImGui::BeginPopupModal("New Theme", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Theme name:");
        ImGui::SetNextItemWidth(220 * uisc());
        bool enter = ImGui::InputText(
            "##NewThemeName", opt_new_profile_name_,
            sizeof(opt_new_profile_name_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (opt_new_profile_err_[0] != '\0')
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                               opt_new_profile_err_);
        }

        bool ok_clicked = ImGui::Button("OK", ImVec2(80 * uisc(), 0));
        ImGui::SameLine();
        bool cancel_clicked = ImGui::Button("Cancel", ImVec2(80 * uisc(), 0));

        if (cancel_clicked)
        {
            ImGui::CloseCurrentPopup();
        }
        else if ((enter || ok_clicked) && opt_new_profile_name_[0] != '\0')
        {
            opt_new_profile_err_[0] = '\0';

            if (strlen(opt_new_profile_name_) > 128)
            {
                strcpy(opt_new_profile_err_,
                       "This theme name is too long.  Please choose a "
                       "shorter name.");
            }
            else if (strchr(opt_new_profile_name_, '\\') != 0)
            {
                strcpy(opt_new_profile_err_,
                       "The character '\\' is not allowed in a theme "
                       "name.");
            }
            else
            {
                int dup = FALSE;
                for (int i = 0 ; i < opt_profile_count_ ; ++i)
                {
                    if (stricmp(opt_profile_names_[i],
                                opt_new_profile_name_) == 0)
                    {
                        dup = TRUE;
                        break;
                    }
                }

                if (dup)
                {
                    strcpy(opt_new_profile_err_,
                           "A theme with this name already exists.  You "
                           "must give each theme a unique name.");
                }
                else
                {
                    set_profile_desc(opt_desc_);
                    save();
                    save_as(opt_new_profile_name_);
                    win_->set_game_specific_profile(opt_new_profile_name_);
                    set_profile_desc("");

                    opt_refresh_profile_list();
                    opt_on_profile_change();

                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }
}

/*
 *   Keyboard tab.  Mirrors CHtmlDialogKeys.
 */
void CHtmlPreferences::opt_render_keys_tab()
{
    ImGui::Text("Ctrl+V");
    if (ImGui::RadioButton("Page Down (Paste is Ctrl+Y)",
                           opt_emacs_ctrl_v_ != 0))
    {
        opt_emacs_ctrl_v_ = TRUE;
        set_emacs_ctrl_v(opt_emacs_ctrl_v_);
    }
    if (ImGui::RadioButton("Paste", opt_emacs_ctrl_v_ == 0))
    {
        opt_emacs_ctrl_v_ = FALSE;
        set_emacs_ctrl_v(opt_emacs_ctrl_v_);
    }

    ImGui::Spacing();
    ImGui::Text("Alt+V");
    if (ImGui::RadioButton("Page Up", opt_emacs_alt_v_ != 0))
    {
        opt_emacs_alt_v_ = TRUE;
        set_emacs_alt_v(opt_emacs_alt_v_);
    }
    if (ImGui::RadioButton("Standard Windows Menu Shortcut",
                           opt_emacs_alt_v_ == 0))
    {
        opt_emacs_alt_v_ = FALSE;
        set_emacs_alt_v(opt_emacs_alt_v_);
    }

    ImGui::Spacing();
    ImGui::Text("Up/Down Arrow Keys");
    if (ImGui::RadioButton("Scroll window contents", opt_arrow_scroll_ != 0))
    {
        opt_arrow_scroll_ = TRUE;
        set_arrow_keys_always_scroll(opt_arrow_scroll_);
    }
    if (ImGui::RadioButton("Command history", opt_arrow_scroll_ == 0))
    {
        opt_arrow_scroll_ = FALSE;
        set_arrow_keys_always_scroll(opt_arrow_scroll_);
    }
}

/*
 *   File Safety tab.  Mirrors CHtmlDialogSafety; the read/write button
 *   arrays there (rbuttons_/wbuttons_) collapse to exactly three selectable
 *   levels per side, which is what's reproduced here directly.
 */
void CHtmlPreferences::opt_render_filesafety_tab()
{
    struct level_opt_t { const char *label; int level; };

    ImGui::TextUnformatted("File Read Access:");
    static const level_opt_t rlevels[] = {
        { "No read access", 4 },
        { "Read from game folder only", 2 },
        { "Read from any file", 0 },
    };
    for (int i = 0 ; i < 3 ; ++i)
    {
        if (ImGui::RadioButton(rlevels[i].label,
                               opt_safety_read_ == rlevels[i].level))
        {
            opt_safety_read_ = rlevels[i].level;
            set_file_safety_level(opt_safety_read_, opt_safety_write_);
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("File Write Access:");
    static const level_opt_t wlevels[] = {
        { "No write access", 4 },
        { "Write to game folder only", 1 },
        { "Write to any file", 0 },
    };
    for (int i = 0 ; i < 3 ; ++i)
    {
        if (ImGui::RadioButton(wlevels[i].label,
                               opt_safety_write_ == wlevels[i].level))
        {
            opt_safety_write_ = wlevels[i].level;
            set_file_safety_level(opt_safety_read_, opt_safety_write_);
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Note: The File Safety Setting applies only to explicit file "
        "operations by the game.  It does not affect saving or restoring "
        "games or logging a transcript to a file.");
}

/*
 *   Network Safety tab.  Mirrors CHtmlDialogNetSafety.
 */
void CHtmlPreferences::opt_render_netsafety_tab()
{
    struct level_opt_t { const char *label; int level; };

    ImGui::TextWrapped(
        "Client Safety (access FROM game to network services):");
    static const level_opt_t cli[] = {
        { "Maximum safety: No network access", 2 },
        { "Local: Access to services on this computer only", 1 },
        { "Minimum safety: All network access allowed", 0 },
    };
    for (int i = 0 ; i < 3 ; ++i)
    {
        if (ImGui::RadioButton(cli[i].label, opt_net_client_ == cli[i].level))
        {
            opt_net_client_ = cli[i].level;
            set_net_safety_level(opt_net_client_, opt_net_server_);
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Server Safety (access TO game from network clients):");
    static const level_opt_t srv[] = {
        { "Maximum safety: No server capabilities", 2 },
        { "Local: Access from this computer only", 1 },
        { "Minimum safety: Access from any computer", 0 },
    };
    for (int i = 0 ; i < 3 ; ++i)
    {
        if (ImGui::RadioButton(srv[i].label, opt_net_server_ == srv[i].level))
        {
            opt_net_server_ = srv[i].level;
            set_net_safety_level(opt_net_client_, opt_net_server_);
        }
    }
}

/*
 *   Memory tab.  Mirrors CHtmlDialogMem.
 */
void CHtmlPreferences::opt_render_mem_tab()
{
    static const char *settings[] =
    {
        "No Limit", "32 KBytes", "64 KBytes", "96 KBytes", "128 KBytes",
    };
    const int settings_count = sizeof(settings) / sizeof(settings[0]);

    ImGui::TextWrapped(
        "Use this setting to limit the amount of previously-displayed "
        "text that the system will keep in memory.  Use a smaller size "
        "if your system runs low on memory during long game sessions.");
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Text Memory Limit");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150 * uisc());
    if (ImGui::BeginCombo("##MemLimit", settings[opt_mem_idx_]))
    {
        for (int i = 0 ; i < settings_count ; ++i)
        {
            bool sel = (i == opt_mem_idx_);
            if (ImGui::Selectable(settings[i], sel))
            {
                opt_mem_idx_ = i;
                set_mem_text_limit((unsigned long)i * (unsigned long)32768);
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

/*
 *   Starting tab.  Mirrors CHtmlDialogStart; the folder browse button now
 *   shows the ImGui-native CTadsFolderDialog instead of the native
 *   CTadsDialogFolderSel2 modal.
 */
void CHtmlPreferences::opt_render_start_tab()
{
    bool ask = (opt_ask_game_ != 0);
    if (ImGui::Checkbox("Ask for a game to open on starting HTML TADS",
                        &ask))
    {
        opt_ask_game_ = ask ? TRUE : FALSE;
        set_init_ask_game(opt_ask_game_);
    }

    ImGui::Spacing();
    ImGui::Text("Initial game folder:");
    ImGui::SetNextItemWidth(300 * uisc());
    if (ImGui::InputText("##InitFolder", opt_init_folder_,
                         sizeof(opt_init_folder_)))
        set_init_open_folder(opt_init_folder_);
    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        textchar_t fname[OSFNMAX];
        strcpy(fname, opt_init_folder_);
        if (fname[0] == '\0')
            GetCurrentDirectory(sizeof(fname), fname);

        CTadsFolderDialog::open("Initial \"Open\" Folder:",
            "Select Initial Folder", fname,
            [this](const char *folder)
            {
                if (folder != 0)
                {
                    strncpy(opt_init_folder_, folder,
                            sizeof(opt_init_folder_) - 1);
                    opt_init_folder_[sizeof(opt_init_folder_) - 1] = '\0';
                    set_init_open_folder(opt_init_folder_);

                    /*
                     *   make the new folder active for the next "open"
                     *   dialog too
                     */
                    CTadsApp::get_app()->set_openfile_path(opt_init_folder_);
                }
            });
    }
}

/*
 *   Quitting tab.  Mirrors CHtmlDialogQuit.
 */
void CHtmlPreferences::opt_render_quit_tab()
{
    struct close_opt_t { const char *label; int val; };

    ImGui::TextUnformatted("Action on closing game window:");
    static const close_opt_t close_opts[] = {
        { "Send QUIT command to game", HTML_PREF_CLOSE_CMD },
        { "Prompt before closing window and exiting", HTML_PREF_CLOSE_PROMPT },
        { "Close window and exit without prompting", HTML_PREF_CLOSE_NOW },
    };
    for (int i = 0 ; i < 3 ; ++i)
    {
        if (ImGui::RadioButton(close_opts[i].label,
                               opt_close_action_ == close_opts[i].val))
        {
            opt_close_action_ = close_opts[i].val;
            set_close_action(opt_close_action_);
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("After quitting the game:");
    static const close_opt_t quit_opts[] = {
        { "Wait for a keystroke, then exit", HTML_PREF_POSTQUIT_EXIT },
        { "Keep running", HTML_PREF_POSTQUIT_KEEP },
    };
    for (int i = 0 ; i < 2 ; ++i)
    {
        if (ImGui::RadioButton(quit_opts[i].label,
                               opt_postquit_action_ == quit_opts[i].val))
        {
            opt_postquit_action_ = quit_opts[i].val;
            set_postquit_action(opt_postquit_action_);
        }
    }
}

/*
 *   Game Chest tab.  Mirrors CHtmlDialogGameChest; the browse buttons now
 *   show the ImGui-native CTadsFileDialog instead of the native
 *   GetOpenFileName() common dialog.
 */
void CHtmlPreferences::opt_render_gamechest_tab()
{
    ImGui::TextWrapped(
        "Game Chest database file: this is where your list of game links "
        "is stored.  Game Chest puts this file in the \"TADS\" folder in "
        "\"My Documents\" by default, but you can choose a custom "
        "location.");
    ImGui::SetNextItemWidth(-90 * uisc());
    if (ImGui::InputText("##GcFile", opt_gc_file_, sizeof(opt_gc_file_)))
        set_gc_database(opt_gc_file_);
    ImGui::SameLine();
    if (ImGui::Button("Browse...##GcFileBrowse"))
    {
        CTadsFileDialog::open(TADSFILEDLG_OPEN,
            "Select the Game Chest database file",
            "Game Chest Database\0GameChest.txt\0", opt_gc_file_, FALSE,
            [this](const char *fname)
            {
                if (fname != 0)
                {
                    strncpy(opt_gc_file_, fname, sizeof(opt_gc_file_) - 1);
                    opt_gc_file_[sizeof(opt_gc_file_) - 1] = '\0';
                    set_gc_database(opt_gc_file_);
                }
            });
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Default##GcFileDefault"))
    {
        char fname[OSFNMAX];
        if (CTadsApp::get_my_docs_path(fname, sizeof(fname)))
        {
            char dir[OSFNMAX];
            strcpy(dir, fname);
            os_build_full_path(fname, sizeof(fname), dir,
                               "TADS\\GameChest.txt");
        }
        else
        {
            strcpy(fname, "GameChest.txt");
        }

        strncpy(opt_gc_file_, fname, sizeof(opt_gc_file_) - 1);
        opt_gc_file_[sizeof(opt_gc_file_) - 1] = '\0';
        set_gc_database(opt_gc_file_);
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Game Chest background picture.  You can choose a custom image "
        "(JPEG or PNG) for the background, or leave this blank if you "
        "don't want a background picture at all.");
    ImGui::SetNextItemWidth(-90 * uisc());
    if (ImGui::InputText("##GcBkg", opt_gc_bkg_, sizeof(opt_gc_bkg_)))
        set_gc_bkg(opt_gc_bkg_);
    ImGui::SameLine();
    if (ImGui::Button("Browse...##GcBkgBrowse"))
    {
        CTadsFileDialog::open(TADSFILEDLG_OPEN,
            "Select an image to use as the Game Chest background",
            "Images\0*.jpg;*.jpeg;*.jpe;*.png\0", opt_gc_bkg_, TRUE,
            [this](const char *fname)
            {
                if (fname != 0)
                {
                    strncpy(opt_gc_bkg_, fname, sizeof(opt_gc_bkg_) - 1);
                    opt_gc_bkg_[sizeof(opt_gc_bkg_) - 1] = '\0';
                    set_gc_bkg(opt_gc_bkg_);
                }
            });
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Default##GcBkgDefault"))
    {
        strcpy(opt_gc_bkg_, "exe:gamechest/bkg.png");
        set_gc_bkg(opt_gc_bkg_);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   ImGui-native "Customize Theme" dialog (guit3).  Replaces the native
 *   Fonts/Colors/More/Media property sheet run_appearance_dlg() shows (that
 *   function, and the CHtmlDialog{Fonts,Color,More,Media} property-page
 *   classes above, are left in place unused rather than removed, same as
 *   the rest of the never-removed native code elsewhere in the port).
 *
 *   Same immediate-write, no-Apply-step convention as the Options dialog:
 *   every control writes straight through to the preferences object the
 *   instant it changes, firing the same schedule_reformat()/notify_*_pref_
 *   change() side effects the native PSN_APPLY handlers used to fire only
 *   on Apply/OK.
 */

/*
 *   Font-family selector callbacks - identical filtering rules to
 *   CHtmlDialogFonts::font_select_serif/sans/script/typewriter above.
 */
int CHtmlPreferences::cust_font_select_serif(ENUMLOGFONTEX *elf,
                                             NEWTEXTMETRIC *tm)
{
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_ROMAN);
}

int CHtmlPreferences::cust_font_select_sans(ENUMLOGFONTEX *elf,
                                            NEWTEXTMETRIC *tm)
{
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_SWISS);
}

int CHtmlPreferences::cust_font_select_script(ENUMLOGFONTEX *elf,
                                              NEWTEXTMETRIC *tm)
{
    return (tm->tmCharSet != SYMBOL_CHARSET
            && ((elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_SCRIPT
                || (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_ROMAN));
}

int CHtmlPreferences::cust_font_select_typewriter(ENUMLOGFONTEX *elf,
                                                  NEWTEXTMETRIC *tm)
{
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_MODERN);
}

/*
 *   Context for cust_font_enum_cb() - unlike CTadsDialog::init_font_popup(),
 *   which enumerates straight into a combo box's list, this enumerates into
 *   a plain fixed-size name array for use by an ImGui combo.
 */
struct cust_font_enum_info_t
{
    char (*list)[CHtmlPreferences::CUST_FONT_NAME_LEN];
    int count;
    int max;
    int include_proportional;
    int include_fixed;
    int (*selector_func)(ENUMLOGFONTEX *, NEWTEXTMETRIC *);
};

static int CALLBACK cust_font_enum_cb(ENUMLOGFONTEX *elf, NEWTEXTMETRIC *tm,
                                      DWORD, LPARAM lpar)
{
    cust_font_enum_info_t *info = (cust_font_enum_info_t *)lpar;
    int proportional, fixed, include_font;

    include_font = FALSE;
    if (info->selector_func != 0)
    {
        include_font = (*info->selector_func)(elf, tm);
    }
    else
    {
        proportional = ((tm->tmPitchAndFamily & TMPF_FIXED_PITCH) != 0);
        fixed = !proportional;
        if (((proportional && info->include_proportional)
             || (fixed && info->include_fixed))
            && tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) != FF_DECORATIVE)
            include_font = TRUE;
    }

    if (include_font && info->count < info->max)
    {
        const char *name = (const char *)elf->elfLogFont.lfFaceName;
        int i;

        /* EnumFontFamiliesEx reports each face once per style/script, so
         * skip names we've already added */
        for (i = 0 ; i < info->count ; ++i)
        {
            if (strcmp(info->list[i], name) == 0)
                return TRUE;
        }

        strncpy(info->list[info->count], name,
                CHtmlPreferences::CUST_FONT_NAME_LEN - 1);
        info->list[info->count][CHtmlPreferences::CUST_FONT_NAME_LEN - 1]
            = '\0';
        ++info->count;
    }

    return TRUE;
}

/*
 *   Re-gather the cust_fonts_*_ name lists for the given character set.
 *   Mirrors what CTadsDialog::init_font_popup() did per-combo, but fills
 *   plain arrays instead of populating combo box controls, since there are
 *   no HWNDs to enumerate into here.
 */
void CHtmlPreferences::cust_refresh_font_lists(unsigned int charset_id)
{
    struct { char (*list)[CUST_FONT_NAME_LEN]; int *count;
             int prop; int fixed;
             int (*sel)(ENUMLOGFONTEX *, NEWTEXTMETRIC *); } jobs[] =
    {
        { cust_fonts_all_, &cust_fonts_all_count_, TRUE, TRUE, 0 },
        { cust_fonts_fixed_, &cust_fonts_fixed_count_, FALSE, TRUE, 0 },
        { cust_fonts_serif_, &cust_fonts_serif_count_, 0, 0,
          &cust_font_select_serif },
        { cust_fonts_sans_, &cust_fonts_sans_count_, 0, 0,
          &cust_font_select_sans },
        { cust_fonts_script_, &cust_fonts_script_count_, 0, 0,
          &cust_font_select_script },
        { cust_fonts_typewriter_, &cust_fonts_typewriter_count_, 0, 0,
          &cust_font_select_typewriter },
    };

    HDC dc = GetDC(0);
    for (size_t i = 0 ; i < sizeof(jobs)/sizeof(jobs[0]) ; ++i)
    {
        cust_font_enum_info_t info;
        LOGFONT lf;

        info.list = jobs[i].list;
        info.count = 0;
        info.max = CUST_MAX_FONTS;
        info.include_proportional = jobs[i].prop;
        info.include_fixed = jobs[i].fixed;
        info.selector_func = jobs[i].sel;

        lf.lfFaceName[0] = '\0';
        lf.lfPitchAndFamily = 0;
        lf.lfCharSet = (BYTE)charset_id;

        EnumFontFamiliesEx(dc, &lf, (FONTENUMPROC)cust_font_enum_cb,
                           (LPARAM)&info, 0);

        *jobs[i].count = info.count;
    }
    ReleaseDC(0, dc);
}

/*
 *   Open the Customize Theme dialog: snapshot all the current preference
 *   values into the dialog's working state, gather the font name lists,
 *   and mark it pending-open.
 */
void CHtmlPreferences::open_customize_theme_dialog(HWND owner,
                                                    CHtmlWinWithPrefs *win)
{
    win_ = win;
    cust_owner_hwnd_ = owner;

    cust_refresh_font_lists(win->get_default_charset());

    strncpy(cust_font_prop_, get_prop_font(), CUST_FONT_NAME_LEN - 1);
    cust_font_prop_[CUST_FONT_NAME_LEN - 1] = '\0';
    cust_fontsz_prop_ = get_prop_fontsz();

    strncpy(cust_font_mono_, get_mono_font(), CUST_FONT_NAME_LEN - 1);
    cust_font_mono_[CUST_FONT_NAME_LEN - 1] = '\0';
    cust_fontsz_mono_ = get_mono_fontsz();

    strncpy(cust_font_serif_, get_font_serif(), CUST_FONT_NAME_LEN - 1);
    cust_font_serif_[CUST_FONT_NAME_LEN - 1] = '\0';
    cust_fontsz_serif_ = get_serif_fontsz();

    strncpy(cust_font_sans_, get_font_sans(), CUST_FONT_NAME_LEN - 1);
    cust_font_sans_[CUST_FONT_NAME_LEN - 1] = '\0';
    cust_fontsz_sans_ = get_sans_fontsz();

    strncpy(cust_font_script_, get_font_script(), CUST_FONT_NAME_LEN - 1);
    cust_font_script_[CUST_FONT_NAME_LEN - 1] = '\0';
    cust_fontsz_script_ = get_script_fontsz();

    strncpy(cust_font_typewriter_, get_font_typewriter(),
            CUST_FONT_NAME_LEN - 1);
    cust_font_typewriter_[CUST_FONT_NAME_LEN - 1] = '\0';
    cust_fontsz_typewriter_ = get_typewriter_fontsz();

    /* the input font defaults to "follow the main game font" when blank */
    {
        const char *inp = get_inpfont_name();
        if (inp == 0 || inp[0] == '\0')
            strcpy(cust_font_input_, "(Main Game Font)");
        else
        {
            strncpy(cust_font_input_, inp, CUST_FONT_NAME_LEN - 1);
            cust_font_input_[CUST_FONT_NAME_LEN - 1] = '\0';
        }
    }
    cust_fontsz_input_ = get_inpfont_size();
    cust_input_color_ = get_inpfont_color();
    cust_input_bold_ = (get_inpfont_bold() != 0);
    cust_input_italic_ = (get_inpfont_italic() != 0);

    cust_bg_color_ = get_bg_color();
    cust_text_color_ = get_text_color();
    cust_link_color_ = get_link_color();
    cust_vlink_color_ = get_vlink_color();
    cust_hlink_color_ = get_hlink_color();
    cust_alink_color_ = get_alink_color();
    cust_stat_text_color_ = get_color_status_text();
    cust_stat_bg_color_ = get_color_status_bg();
    cust_use_win_colors_ = (get_use_win_colors() != 0);
    cust_override_colors_ = (get_override_colors() != 0);
    cust_underline_links_ = (get_underline_links() != 0);
    cust_hover_hilite_ = (get_hover_hilite() != 0);
    cust_show_links_sel_ =
        (get_links_on() ? 0 : get_links_ctrl() ? 1 : 2);
    cust_warned_link_change_ = false;

    cust_alt_more_style_ = (get_alt_more_style() ? 1 : 0);

    cust_graphics_on_ = (get_graphics_on() != 0);
    cust_sounds_on_ = (get_sounds_on() != 0);
    cust_music_on_ = (get_music_on() != 0);

    cust_dlg_open_ = true;
    cust_dlg_want_open_ = true;
}

/*
 *   Draw the Customize Theme dialog for the current ImGui frame.  Safe to
 *   call unconditionally every frame; it's a no-op once the dialog is
 *   closed.
 */
void CHtmlPreferences::render_customize_theme_dialog()
{
    if (!cust_dlg_open_)
        return;

    if (cust_dlg_want_open_)
    {
        ImGui::OpenPopup("CustomizeTheme");
        cust_dlg_want_open_ = false;
    }

    char title[128 + 50];
    sprintf(title, "Customize \"%s\" Theme###CustomizeTheme",
            get_active_profile_name());

    ImGui::SetNextWindowSize(ImVec2(520 * uisc(), 480 * uisc()), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(title, &cust_dlg_open_,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        if (ImGui::BeginTabBar("CustomizeThemeTabs"))
        {
            if (ImGui::BeginTabItem("Font"))
            {
                cust_render_font_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Colors"))
            {
                cust_render_colors_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("More"))
            {
                cust_render_more_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Media"))
            {
                cust_render_media_tab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(80 * uisc(), 0)))
        {
            cust_dlg_open_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

/*
 *   Font-name combo helper shared by the Font tab's rows.  Returns true if
 *   the selection changed (and updates 'cur' in place).
 */
bool CHtmlPreferences::cust_font_combo(const char *imgui_id,
                                       char (*list)[CUST_FONT_NAME_LEN],
                                       int count, char *cur)
{
    bool changed = false;

    ImGui::SetNextItemWidth(200 * uisc());
    if (ImGui::BeginCombo(imgui_id, cur))
    {
        for (int i = 0 ; i < count ; ++i)
        {
            bool sel = (strcmp(list[i], cur) == 0);
            if (ImGui::Selectable(list[i], sel))
            {
                strncpy(cur, list[i], CUST_FONT_NAME_LEN - 1);
                cur[CUST_FONT_NAME_LEN - 1] = '\0';
                changed = true;
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return changed;
}

/*
 *   Font point-size combo helper shared by the Font tab's rows.
 */
bool CHtmlPreferences::cust_fontsz_combo(const char *imgui_id, int *cur)
{
    bool changed = false;
    char preview[20];

    sprintf(preview, "%d pt", *cur);
    ImGui::SetNextItemWidth(70 * uisc());
    if (ImGui::BeginCombo(imgui_id, preview))
    {
        for (const int *p = font_pt_sizes ; *p != 0 ; ++p)
        {
            bool sel = (*p == *cur);
            char buf[20];

            sprintf(buf, "%d pt", *p);
            if (ImGui::Selectable(buf, sel))
            {
                *cur = *p;
                changed = true;
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return changed;
}

/*
 *   Font tab: main/fixed-width/serif/sans/script/typewriter font pickers,
 *   plus the command (input) font and its color/bold/italic style.
 *   Mirrors CHtmlDialogFonts.
 */
void CHtmlPreferences::cust_render_font_tab()
{
    struct { const char *label; const char *combo_id;
             char (*list)[CUST_FONT_NAME_LEN]; int count;
             char *cur; const char *sz_id; int *sz; } rows[] =
    {
        { "Main Game Font", "##MainFont", cust_fonts_all_,
          cust_fonts_all_count_, cust_font_prop_,
          "##MainFontSz", &cust_fontsz_prop_ },
        { "Fixed-Width Font", "##MonoFont", cust_fonts_fixed_,
          cust_fonts_fixed_count_, cust_font_mono_,
          "##MonoFontSz", &cust_fontsz_mono_ },
        { "Serif Font", "##SerifFont", cust_fonts_serif_,
          cust_fonts_serif_count_, cust_font_serif_,
          "##SerifFontSz", &cust_fontsz_serif_ },
        { "Sans-Serif Font", "##SansFont", cust_fonts_sans_,
          cust_fonts_sans_count_, cust_font_sans_,
          "##SansFontSz", &cust_fontsz_sans_ },
        { "Script Font", "##ScriptFont", cust_fonts_script_,
          cust_fonts_script_count_, cust_font_script_,
          "##ScriptFontSz", &cust_fontsz_script_ },
        { "Typewriter Font", "##TypewriterFont", cust_fonts_typewriter_,
          cust_fonts_typewriter_count_, cust_font_typewriter_,
          "##TypewriterFontSz", &cust_fontsz_typewriter_ },
    };

    typedef void (CHtmlPreferences::*setter_t)(const char *);
    static const setter_t name_setters[] =
    {
        &CHtmlPreferences::set_prop_font, &CHtmlPreferences::set_mono_font,
        &CHtmlPreferences::set_font_serif, &CHtmlPreferences::set_font_sans,
        &CHtmlPreferences::set_font_script,
        &CHtmlPreferences::set_font_typewriter,
    };
    typedef void (CHtmlPreferences::*szsetter_t)(int);
    static const szsetter_t sz_setters[] =
    {
        &CHtmlPreferences::set_prop_fontsz, &CHtmlPreferences::set_mono_fontsz,
        &CHtmlPreferences::set_serif_fontsz, &CHtmlPreferences::set_sans_fontsz,
        &CHtmlPreferences::set_script_fontsz,
        &CHtmlPreferences::set_typewriter_fontsz,
    };

    for (size_t i = 0 ; i < sizeof(rows)/sizeof(rows[0]) ; ++i)
    {
        ImGui::TextUnformatted(rows[i].label);
        ImGui::SameLine(160 * uisc());
        if (cust_font_combo(rows[i].combo_id, rows[i].list, rows[i].count,
                            rows[i].cur))
        {
            (this->*name_setters[i])(rows[i].cur);
            schedule_reformat(FALSE);
        }
        ImGui::SameLine();
        if (cust_fontsz_combo(rows[i].sz_id, rows[i].sz))
        {
            (this->*sz_setters[i])(*rows[i].sz);
            schedule_reformat(FALSE);
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Command Font");
    ImGui::TextUnformatted("Font");
    ImGui::SameLine(160 * uisc());
    {
        /* the input font list is "(Main Game Font)" plus the general list */
        static char input_list[CUST_MAX_FONTS + 1][CUST_FONT_NAME_LEN];
        int input_count = 0;
        strcpy(input_list[input_count++], "(Main Game Font)");
        for (int i = 0 ; i < cust_fonts_all_count_
                         && input_count < CUST_MAX_FONTS + 1 ; ++i)
        {
            strncpy(input_list[input_count], cust_fonts_all_[i],
                    CUST_FONT_NAME_LEN - 1);
            input_list[input_count][CUST_FONT_NAME_LEN - 1] = '\0';
            ++input_count;
        }

        if (cust_font_combo("##InputFont", input_list, input_count,
                            cust_font_input_))
        {
            if (strcmp(cust_font_input_, "(Main Game Font)") == 0)
                set_inpfont_name("");
            else
                set_inpfont_name(cust_font_input_);
            schedule_reformat(FALSE);
        }
    }
    ImGui::SameLine();
    if (cust_fontsz_combo("##InputFontSz", &cust_fontsz_input_))
    {
        set_inpfont_size(cust_fontsz_input_);
        schedule_reformat(FALSE);
    }

    ImGui::SameLine();
    if (cust_color_edit("##InputColor", &cust_input_color_))
    {
        set_inpfont_color(cust_input_color_);
        schedule_reformat(FALSE);
    }

    if (ImGui::Checkbox("Bold##InputBold", &cust_input_bold_))
    {
        set_inpfont_bold(cust_input_bold_);
        schedule_reformat(FALSE);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Italic##InputItalic", &cust_input_italic_))
    {
        set_inpfont_italic(cust_input_italic_);
        schedule_reformat(FALSE);
    }
}

/*
 *   Color-swatch helper shared by the Colors tab's rows.  Returns true if
 *   the color changed (and updates '*color' in place).
 */
bool CHtmlPreferences::cust_color_edit(const char *label,
                                       HTML_color_t *color)
{
    ImVec4 c = HTML_color_to_ImVec4(*color);
    if (ImGui::ColorEdit3(label, (float *)&c,
                          ImGuiColorEditFlags_NoAlpha
                          | ImGuiColorEditFlags_NoInputs))
    {
        *color = ImVec4_to_HTML_color(c);
        return true;
    }
    return false;
}

/*
 *   Colors tab: main text/background, status line, and hyperlink colors.
 *   Mirrors CHtmlDialogColor.
 */
void CHtmlPreferences::cust_render_colors_tab()
{
    ImGui::SeparatorText("Main Text");
    if (ImGui::Checkbox("Use Windows colors", &cust_use_win_colors_))
    {
        set_use_win_colors(cust_use_win_colors_);
        schedule_reformat(FALSE);
    }
    ImGui::BeginDisabled(cust_use_win_colors_);
    if (cust_color_edit("Text Color", &cust_text_color_))
    {
        set_text_color(cust_text_color_);
        schedule_reformat(FALSE);
    }
    if (cust_color_edit("Background Color", &cust_bg_color_))
    {
        set_bg_color(cust_bg_color_);
        schedule_reformat(FALSE);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::SeparatorText("Status Line");
    if (ImGui::Checkbox("Let games override these colors",
                        &cust_override_colors_))
    {
        set_override_colors(cust_override_colors_);
        schedule_reformat(FALSE);
    }
    ImGui::BeginDisabled(cust_override_colors_);
    if (cust_color_edit("Status Text Color", &cust_stat_text_color_))
    {
        set_color_status_text(cust_stat_text_color_);
        schedule_reformat(FALSE);
    }
    if (cust_color_edit("Status Background Color", &cust_stat_bg_color_))
    {
        set_color_status_bg(cust_stat_bg_color_);
        schedule_reformat(FALSE);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::SeparatorText("Hyperlinks");
    static const char *show_link_opts[] =
        { "Always", "Only when Ctrl is held down", "Never" };
    ImGui::SetNextItemWidth(260 * uisc());
    if (ImGui::BeginCombo("Show Links", show_link_opts[cust_show_links_sel_]))
    {
        for (int i = 0 ; i < 3 ; ++i)
        {
            bool sel = (cust_show_links_sel_ == i);
            if (ImGui::Selectable(show_link_opts[i], sel))
            {
                int old_sel = cust_show_links_sel_;
                cust_show_links_sel_ = i;

                bool links_on = (i == 0), links_ctrl = (i == 1);
                bool ch = (old_sel != i);
                set_links_on(links_on);
                set_links_ctrl(links_ctrl);

                if (ch && !cust_warned_link_change_)
                {
                    notify_link_pref_change();
                    cust_warned_link_change_ = true;
                }
                schedule_reformat(FALSE);
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    bool links_on = (cust_show_links_sel_ != 2);
    ImGui::BeginDisabled(!links_on);
    if (ImGui::Checkbox("Underline links", &cust_underline_links_))
    {
        set_underline_links(cust_underline_links_);
        schedule_reformat(FALSE);
    }
    if (ImGui::Checkbox("Highlight link when the mouse hovers over it",
                        &cust_hover_hilite_))
    {
        set_hover_hilite(cust_hover_hilite_);
        schedule_reformat(FALSE);
    }
    if (cust_color_edit("Unvisited Link Color", &cust_link_color_))
    {
        set_link_color(cust_link_color_);
        schedule_reformat(FALSE);
    }
    ImGui::BeginDisabled(!cust_hover_hilite_);
    if (cust_color_edit("Hovering Link Color", &cust_hlink_color_))
    {
        set_hlink_color(cust_hlink_color_);
        schedule_reformat(FALSE);
    }
    ImGui::EndDisabled();
    if (cust_color_edit("Clicked Link Color", &cust_alink_color_))
    {
        set_alink_color(cust_alink_color_);
        schedule_reformat(FALSE);
    }
    ImGui::EndDisabled();

    if (cust_color_edit("Visited Link Color", &cust_vlink_color_))
    {
        set_vlink_color(cust_vlink_color_);
        schedule_reformat(FALSE);
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Note: some games override these colors with their own visual "
        "style, regardless of this setting.");
}

/*
 *   More tab: how the game handles the MORE prompt.  Mirrors
 *   CHtmlDialogMore.
 */
void CHtmlPreferences::cust_render_more_tab()
{
    ImGui::TextWrapped(
        "When there's more text than fits on the screen, TADS pauses and "
        "shows a MORE prompt.  Choose how you'd like this to work:");
    ImGui::Spacing();

    if (ImGui::RadioButton("Show a MORE prompt in the game window",
                           cust_alt_more_style_ == 0))
    {
        cust_alt_more_style_ = 0;
        set_alt_more_style(FALSE);
        schedule_reformat(TRUE);
    }
    ImGui::Indent();
    ImGui::TextWrapped(
        "Halts scrolling until you press a key to see more text.");
    ImGui::Unindent();

    ImGui::Spacing();
    if (ImGui::RadioButton("Show \"MORE\" on the status line",
                           cust_alt_more_style_ == 1))
    {
        cust_alt_more_style_ = 1;
        set_alt_more_style(TRUE);
        schedule_reformat(TRUE);
    }
    ImGui::Indent();
    ImGui::TextWrapped(
        "Shows all of the text without stopping to wait for a key press.");
    ImGui::Unindent();
}

/*
 *   Media tab: graphics/sound effects/music enabling.  Mirrors
 *   CHtmlDialogMedia.
 */
void CHtmlPreferences::cust_render_media_tab()
{
    ImGui::TextWrapped(
        "These options control what kinds of multimedia content the game "
        "is allowed to show.");
    ImGui::Spacing();

    if (ImGui::Checkbox("Allow Graphics", &cust_graphics_on_))
    {
        set_graphics_on(cust_graphics_on_);
        notify_sound_pref_change();
        schedule_reformat(FALSE);
    }
    if (ImGui::Checkbox("Allow Sound Effects", &cust_sounds_on_))
    {
        set_sounds_on(cust_sounds_on_);
        notify_sound_pref_change();
    }
    if (ImGui::Checkbox("Allow Background Music", &cust_music_on_))
    {
        set_music_on(cust_music_on_);
        notify_sound_pref_change();
    }
}


/*
 *   Schedule reformatting of the window
 */
void CHtmlPreferences::schedule_reformat(int more_mode)
{
    /* 
     *   if we're only reformatting MORE mode, and the window isn't in
     *   MORE mode, we don't need to reformat 
     */
    if (more_mode && !win_->in_more_mode())
        return;
    
    /* tell the window to schedule reformatting */
    win_->notify_visual_pref_change();
}

/*
 *   Notify the game window of a change in the sound preferences.
 */
void CHtmlPreferences::notify_sound_pref_change()
{
    /* tell the window about it */
    win_->notify_sound_pref_change();
}

/*
 *   Notify the game window of a change in links-enabled preferences 
 */
void CHtmlPreferences::notify_link_pref_change()
{
    /* tell the window about it */
    win_->notify_link_pref_change();
}

/*
 *   Schedule reloading the game chest data 
 */
void CHtmlPreferences::schedule_reload_game_chest()
{
    /* tell the window to schedule the reload */
    win_->notify_game_chest_update();
}

/*
 *   Save the game chest database to the given file 
 */
void CHtmlPreferences::save_game_chest_db_as(const char *fname)
{
    /* tell the window to save the database contents */
    win_->save_game_chest_db_as(fname);
}


/*
 *   Save the preferences 
 */
void CHtmlPreferences::save()
{
    /* save the settings under the active profile name */
    save_as(get_active_profile_name());
}

/*
 *   Save the current preferences under the given profile name, creating the
 *   profile name if it doesn't already exist.  This does NOT change the
 *   active profile - it simply saves the settings under the given profile
 *   name.  
 */
void CHtmlPreferences::save_as(const char *profile)
{
    int id;
    HKEY key;
    HKEY global_key;
    DWORD disposition;
    char key_name[256];

    /* get the key containing the settings */
    get_settings_key_for(key_name, sizeof(key_name), profile);

    /* open the registry key for our preference settings */
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name,
                                  &disposition, TRUE);

    /* open the global key as well */
    global_key = CTadsRegistry::open_key(HKEY_CURRENT_USER, w32_pref_key_name,
                                         &disposition, TRUE);

    /* save the properties */
    for (id = (HTML_pref_id_t)0 ; id < HTML_PREF_LAST ; ++id)
    {
        int glob;

        /* check to see if it's global */
        glob = proplist_->get_prop(id)->is_global();

        /* write it under the appropriate key */
        write_to_registry((HTML_pref_id_t)id, glob ? global_key : key);
    }

    /* save the custom colors */
    CTadsRegistry::set_key_binary(key, custclr_val_name,
                                  cust_colors_, sizeof(cust_colors_));

    /* done with the registry key */
    CTadsRegistry::close_key(global_key);
    CTadsRegistry::close_key(key);

    /* 
     *   Broadcast a notification to all top-level windows in the system to
     *   let them know about the update.  This will allow any other running
     *   instances of this same application to check for changes in the
     *   registry and reload their preference settings to reflect the
     *   updates.  
     */
    PostMessage(HWND_BROADCAST, get_prefs_updated_msg(), 0, 0);
}

/*
 *   Restore the preferences for the currently active profile
 */
void CHtmlPreferences::restore(int synced_only)
{
    /* restore the settings for the active profile */
    restore_as(get_active_profile_name(), synced_only);
}

/*
 *   Restore the preferences for a given profile.  
 */
void CHtmlPreferences::restore_as(const char *profile, int synced_only)
{
    int id;
    HKEY key;
    HKEY global_key;
    DWORD disposition;
    char key_name[256];

    /* get the key containing the settings */
    get_settings_key_for(key_name, sizeof(key_name), profile);

    /* open the registry key for our preference settings */
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name,
                                  &disposition, TRUE);

    /* open the global key as well */
    global_key = CTadsRegistry::open_key(HKEY_CURRENT_USER, w32_pref_key_name,
                                         &disposition, TRUE);

    /* save the properties */
    for (id = 0 ; id < HTML_PREF_LAST ; ++id)
    {
        int glob;
        CHtmlProperty *prop = proplist_->get_prop(id);

        /* check to see if it's global */
        glob = prop->is_global();

        /* 
         *   If it's synchronized, or we're reading all properties, read it
         *   from the appropriate key.  (Don't restore unsynchronized
         *   properties when we're merely synchronizing.  During initial
         *   loading, though, restore everything.)  
         */
        if (!synced_only || prop->is_synchronized())
            read_from_registry((HTML_pref_id_t)id, glob ? global_key : key);
    }

    /* load the custom colors */
    CTadsRegistry::query_key_binary(key, custclr_val_name,
                                    cust_colors_, sizeof(cust_colors_));

    /*
     *   Carry forward the old combined file safety level into the new
     *   read/write levels, or apply defaults.  If there's an old combined
     *   value but not a new split value, copy the old value to the new
     *   value.  If there's not even an old value, apply the default level 2.
     */
    int level = get_val_longint(HTML_PREF_FILE_SAFETY_LEVEL);
    int read = get_val_longint(HTML_PREF_FILE_SAFETY_READ);
    int write = get_val_longint(HTML_PREF_FILE_SAFETY_WRITE);
    if (read == -1)
        set_val_longint(HTML_PREF_FILE_SAFETY_READ, level >= 0 ? level : 2);
    if (write == -1)
        set_val_longint(HTML_PREF_FILE_SAFETY_WRITE, level >= 0 ? level : 1);

    /* done with the registry key */
    CTadsRegistry::close_key(global_key);
    CTadsRegistry::close_key(key);
}

/*
 *   Compare the active profile to its saved version 
 */
int CHtmlPreferences::equals_saved(int synced_only)
{
    return equals_saved(get_active_profile_name(), synced_only);
}

/*
 *   Compare a profile in memory to the corresponding profile in the
 *   registry.  Returns true if the saved values in the registry match the
 *   values in memory, false if not.  
 */
int CHtmlPreferences::equals_saved(const char *profile, int synced_only)
{
    int id;
    HKEY key;
    HKEY global_key;
    DWORD disposition;
    char key_name[256];
    int eq;
    char cust_color_buf[sizeof(cust_colors_)];

    /* get the key containing the settings */
    get_settings_key_for(key_name, sizeof(key_name), profile);

    /* open the registry key for our preference settings */
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name,
                                  &disposition, TRUE);

    /* open the global key as well */
    global_key = CTadsRegistry::open_key(HKEY_CURRENT_USER, w32_pref_key_name,
                                         &disposition, TRUE);

    /* scan the properties */
    for (eq = TRUE, id = 0 ; id < HTML_PREF_LAST ; ++id)
    {
        int glob;
        CHtmlProperty *prop = proplist_->get_prop(id);

        /* check to see if it's global */
        glob = prop->is_global();
        
        /* 
         *   Compare the value to the registry value; if it doesn't match,
         *   then the overall profile doesn't match, so we can stop scanning.
         *   If we're only checking synchronized properties, ignore the
         *   property if it's unsynchronized.  
         */
        if ((!synced_only || prop->is_synchronized())
            && !equals_registry_value((HTML_pref_id_t)id,
                                      glob ? global_key : key))
        {
            /* it doesn't match */
            eq = FALSE;

            /* no need to keep looking, as it can't match now */
            break;
        }
    }

    /* load the custom colors */
    CTadsRegistry::query_key_binary(key, custclr_val_name,
                                    cust_color_buf, sizeof(cust_color_buf));

    /* if these don't match, we don't have a match */
    if (memcmp(cust_color_buf, cust_colors_, sizeof(cust_colors_)) != 0)
        eq = FALSE;

    /* done with the registry key */
    CTadsRegistry::close_key(global_key);
    CTadsRegistry::close_key(key);

    /* return the overall comparison result */
    return eq;
}

/*
 *   Write a property to the registry
 */
void CHtmlPreferences::write_to_registry(HTML_pref_id_t id, HKEY key)
{
    char buf[256];
    size_t vallen;
    CHtmlProperty *prop;
    
    /* get the property value in string format */
    prop = proplist_->get_prop(id);
    vallen = prop->gen_str_rep(buf, sizeof(buf));

    /* write it to the registry */
    CTadsRegistry::set_key_str(key, prop->get_name(), buf, vallen);
}

/*
 *   Read a property from the registry 
 */
void CHtmlPreferences::read_from_registry(HTML_pref_id_t id, HKEY key)
{
    char buf[256];
    size_t vallen;
    CHtmlProperty *prop;

    /* get the property */
    prop = proplist_->get_prop(id);

    /* if the registry value isn't set, keep the default value */
    if (!CTadsRegistry::value_exists(key, prop->get_name()))
        return;
    
    /* read the registry value */
    vallen = CTadsRegistry::query_key_str(key, prop->get_name(),
                                          buf, sizeof(buf));

    /* set the preference property item */
    prop->parse_str_rep(buf, vallen);
}

/*
 *   Compare a key to the registry value.  Returns true if the key is equal
 *   to the registry value, false if not.  
 */
int CHtmlPreferences::equals_registry_value(HTML_pref_id_t id, HKEY key)
{
    char buf[256];
    size_t vallen;
    CHtmlProperty *prop;
    char regbuf[256];
    size_t regvallen;

    /* get the property value in string format */
    prop = proplist_->get_prop(id);
    vallen = prop->gen_str_rep(buf, sizeof(buf));

    /* read the registry value */
    regvallen = CTadsRegistry::query_key_str(key, prop->get_name(),
                                             regbuf, sizeof(regbuf));

    /* is the value in memory is the same as the value in the registry? */
    return (vallen == regvallen && memcmp(buf, regbuf, vallen) == 0);
}

/*
 *   Get the name of the registry key containing our settings.  The key
 *   depends on the active profile.  
 */
void CHtmlPreferences::get_settings_key(char *buf, size_t buflen)
{
    /* get the key for the active profile */
    get_settings_key_for(buf, buflen, get_active_profile_name());
}

/*
 *   Get the settings key for the given profile name 
 */
void CHtmlPreferences::get_settings_key_for(char *buf, size_t buflen,
                                            const char *profile)
{
    /* check to see if a profile name was given */
    if (profile == 0)
    {
        /*  
         *   The profile name is null, which means that we want to use the
         *   old-style non-profiled settings key.  These are stored directly
         *   under the root preferences key.  
         */
        strcpy(buf, w32_pref_key_name);
    }
    else
    {
        /* build the registry key for the given profile name */
        _snprintf(buf, buflen, "%s\\Profiles\\%s",
                  w32_pref_key_name, profile);
        buf[buflen - 1] = '\0';
    }
}


/*
 *   Get the currently active profile name
 */
const textchar_t *CHtmlPreferences::get_active_profile_name() const
{
    /* if we don't have an active profile, use the default */
    if (active_profile_.get() == 0)
        return get_default_profile();

    /* if the named profile doesn't exist, use the default */
    if (!profile_exists(active_profile_.get()))
        return get_default_profile();

    /* return the active profile name string */
    return active_profile_.get();
}

/*
 *   Set the active profile name
 */
void CHtmlPreferences::set_active_profile_name(const char *profile)
{
    /* if it's "Default", use the current default */
    if (stricmp(profile, "Default") == 0)
        profile = get_default_profile();

    /* set the name */
    active_profile_.set(profile);
}

/*
 *   Check to see if the selected profile is valid 
 */
int CHtmlPreferences::profile_exists(const char *profile)
{
    char keyname[256];
    HKEY key;
    DWORD disposition;
    int exists;

    /* the empty profile name is invalid */
    if (profile != 0 && profile[0] == '\0')
        return FALSE;

    /* 
     *   try opening the base key for the profile; don't create it if it
     *   doesn't already exist, since we want to test for its existence 
     */
    get_settings_key_for(keyname, sizeof(keyname), profile);
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, keyname,
                                  &disposition, FALSE);

    /* if the key exists, the profile is valid */
    exists = (key != 0);

    /* if we successfully opened the key, close it */
    if (exists)
        CTadsRegistry::close_key(key);

    /* return the result */
    return exists;
}

