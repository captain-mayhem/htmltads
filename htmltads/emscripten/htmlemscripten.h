#ifndef HTMLSYS_EMS_H
#define HTMLSYS_EMS_H

#include <htmlsys.h>

class CHtmlSysWin_emscripten : public CHtmlSysWin{
public:
	CHtmlSysWin_emscripten(class CHtmlFormatter *formatter);
    ~CHtmlSysWin_emscripten();
};

/* ------------------------------------------------------------------------ */
/*
 *   Subclass of HTML System Window that allows text input.  This class
 *   operates essentially the same as the normal HTML window, but extends
 *   it to allow reading input.  
 */
class CHtmlSysWin_emscripten_Input: public CHtmlSysWin_emscripten
{
public:
    CHtmlSysWin_emscripten_Input(class CHtmlFormatterInput *formatter);
};

class CHtmlSys_mainwin: public CHtmlSysFrame, public CHtmlSysWinGroup
{
public:
    CHtmlSys_mainwin(class CHtmlFormatterInput *formatter,
                     class CHtmlParser *parser, int in_debugger);
    ~CHtmlSys_mainwin();
	
	/* 
	 *   display a debug message, bringing the debug log window to the
	 *   foreground if 'foreground' is true 
	 */
	void dbg_print(const char *msg, int foreground);
	
	/* -------------------------------------------------------------------- */
	/*
	 *   CHtmlSysFrame implementation 
	 */

	/* create a banner */
	virtual class CHtmlSysWin
		*create_banner_window(class CHtmlSysWin *parent,
							  HTML_BannerWin_Type_t typ,
							  class CHtmlFormatter *formatter,
							  int where, class CHtmlSysWin *other,
							  HTML_BannerWin_Pos_t pos, unsigned long style);

	/* create an about box subwindow for the game's "About" box */
	virtual class CHtmlSysWin
		*create_aboutbox_window(class CHtmlFormatter *formatter);

	/* remove a banner subwindow */
	virtual void remove_banner_window(class CHtmlSysWin *subwin);

	/* orphan a banner window */
	virtual void orphan_banner_window(CHtmlFormatterBannerExt *fmt);

	/* look up an embedded resource in the executable */
	virtual int get_exe_resource(const textchar_t *resname, size_t resnamelen,
								 textchar_t *fname_buf, size_t fname_buf_len,
								 unsigned long *seek_pos,
								 unsigned long *siz);

	/*
	 *   Read a keyboard command.  Returns false if the application is
	 *   quitting.
	 */
	int get_input_timeout(textchar_t *buf, size_t bufsiz,
						  unsigned long timeout, int use_timeout);

	/*
	 *   Cancel input that was started with get_input_timeout() and
	 *   subsequently interrupted by a timeout. 
	 */
	void get_input_cancel(int reset);

	/* get input from the keyboard */
	int get_input(textchar_t *buf, size_t bufsiz)
	{
		/* cancel any previously interrupted command input */
		get_input_cancel(TRUE);
		
		/* 
		 *   Call our input-line-with-timeout method, but tell it not to use
		 *   the timeout.  Return success (TRUE) if we get a LINE event,
		 *   failure (FALSE) otherwise.
		 */
		return (get_input_timeout(buf, bufsiz, 0, FALSE) == OS_EVT_LINE);
	}

	/*
	 *   Read an event 
	 */
	int get_input_event(unsigned long timeout_in_milliseconds,
						int use_timeout, os_event_info_t *info);

	/* set non-stop mode */
	void set_nonstop_mode(int flag);

	/* check for a break key sequence */
	int check_break_key();

	/* flush buffered text to the parser, and optionally to the display */
	void flush_txtbuf(int fmt, int immediate_redraw);

	/* clear the screen and start a new page */
	void start_new_page();

	/* display output */
	void display_output(const textchar_t *buf, size_t len);
	void display_outputz(const textchar_t *buf)
		{ display_output(buf, get_strlen(buf)); }

	/* 
	 *   display output with quotes - this is useful for writing attribute
	 *   values that might contain characters that are markup-significant 
	 */
	void display_output_quoted(const textchar_t *buf, size_t len);
	void display_outputz_quoted(const textchar_t *buf)
		{ display_output_quoted(buf, get_strlen(buf)); }

	/* 
	 *   Wait for a keystroke from the main panel.  If pause_only is true,
	 *   it means that we don't care about the keystroke but are simply
	 *   waiting for a user-controlled pause; we'll display a status
	 *   message in this case to let the user know we're waiting.  
	 */
	textchar_t wait_for_keystroke(int pause_only);

	/* pause before exiting */
	void pause_for_exit();

	/* show the MORE prompt */
	void pause_for_more();

	/* display a message on the debug console */
	void dbg_print(const char *msg) { dbg_print(msg, TRUE); }

	/* ----------------------------------------------------------------- */
    /*
     *   CHtmlSysWinGroup implementation 
     */

    /* get the default character set */
    oshtml_charset_id_t get_default_win_charset() const
        { return default_charset_; }

    /* translate an HTML 4 code point value */
    virtual size_t xlat_html4_entity(textchar_t *result, size_t result_size,
                                     unsigned int charval,
                                     oshtml_charset_id_t *charset,
                                     int *changed_charset);
	
	/* get my parser */
	CHtmlParser *get_parser() { return parser_; }
	
private:
	/* main HTML panel window */
	CHtmlSysWin_emscripten_Input *main_panel_;

	/* our parser */
	class CHtmlParser *parser_;

	/* text buffer for sending text to the parser */
	class CHtmlTextBuffer *txtbuf_;

	/* default character set */
    oshtml_charset_id_t default_charset_;

	/* 
	*   default "invalid" character in default character set - this is
	*   the character value that we'll display when we have an unmappable
	*   character (this is normally rendered as an open rectangle, to
	*   indicate a missing character) 
	*/
	textchar_t invalid_char_val_;

};

#endif