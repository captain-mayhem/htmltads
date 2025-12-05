#ifndef HTMLSYS_EMS_H
#define HTMLSYS_EMS_H

#include <htmlsys.h>

class CHtmlSysWin_emscripten : public CHtmlSysWin{
public:
	CHtmlSysWin_emscripten(class CHtmlFormatter *formatter);
    ~CHtmlSysWin_emscripten();

	/*
	*   CHtmlSysWin interface implementation 
	*/

	   /* get the window group object for the group I'm associated with */
   virtual class CHtmlSysWinGroup *get_win_group();

   /* 
    *   Receive notification that the contents of the window are being
    *   cleared.  The portable code calls this to let the system layer know
    *   that it's clearing the window.
    *   
    *   The system code isn't required to do anything here; this is purely a
    *   notification to let the system code take care of any necessary
    *   internal bookkeeping.  For example, a window might want to clear its
    *   internal MORE mode counter here, if it uses one, to reflect the fact
    *   that we're starting over with fresh content.  
    */
   virtual void notify_clear_contents();

   /*
    *   Close the window.  Returns true if the window successfully
    *   closed, false if not; the window may not be closed, for example,
    *   if the user cancelled a "save changes" dialog.  If 'force' is true,
    *   the window should be closed without asking the user any questions
    *   (about saving work, for example).  This should always return true
    *   if 'force' is true.  
    */
   virtual int close_window(int force);

   /* get the current width of the HTML display area of the window */
   virtual long get_disp_width();

   /* get the current height of the HTML display area of the window */
   virtual long get_disp_height();

   /* get the number of pixels per inch */
   virtual long get_pix_per_inch();

   /*
    *   Get the bounds of a string rendered in the given font.  Returns a
    *   point, with x set to the width of the *string* as rendered, and y
    *   set to the height of the *font*.  The returned point's y value
    *   should give the total height of the text, including ascenders and
    *   descenders.  If 'ascent' is not null, it should be filled in with
    *   the portion of the height above the text baseline (i.e., the
    *   ascender size for the font).
    *   
    *   The width returned should indicate the *positioning* width of the
    *   text, which might not necessarily be the same as the bounding box of
    *   the glyphs contained in the text.  The difference is that the
    *   positioning width tells us where the next piece of text will go if
    *   we're drawing a string piecewise.  For example, suppose we have a
    *   string "ABCDEF", and we call draw_text() to render it at position
    *   (0,0).  Now suppose we have to redraw a portion of the string, say
    *   the "CDE" part, to adjust the display for a visual change that
    *   doesn't affect character positioning - the user selected the text
    *   with the mouse, say.  In this case, the portable code would call
    *   measure_text() on the part *up to* the part we're redrawing - "AB" -
    *   to figure out where to position the part we *are* redrawing - "CDE".
    *   It would add the width returned from measure_text("AB") to the
    *   original position (0,0) to figure out where to draw "CDE".  So, it's
    *   important that the width returned from measure_text("AB") properly
    *   reflects the character positioning distance.  On most modern systems
    *   with proportional typefaces, the positioning width of a character
    *   often differs from its rendering width, because a glyph can overhang
    *   its positioning area and overlap the previous and next glyph areas.
    *   System APIs usually therefore offer separate measurement functions
    *   for the bounding box (the overall area occupied by all pixels in a
    *   string) and the positioning box (the area used to calculate where
    *   adjacent glyphs should be drawn).
    *   
    *   Note that the heights returned - both the returned y value giving
    *   the overall box height, as well as the ascent height - must be the
    *   DESIGN HEIGHTS for the font, NOT the actual height of the specific
    *   set of characters in 'str'.  The returned height is thus the
    *   *maximum* of the individual heights of *all* of the characters in
    *   the font.  This DOESN'T mean that a port has to actually run through
    *   all of the possible characters in a font and measure each one to
    *   find the largest value - instead, you should be able to call a
    *   simple OS API that gets this information directly.  Virtually every
    *   GUI font engine stores the design metrics as part of its internal
    *   data about a font, and makes them available to apps via a function
    *   that retrieves "font metrics" or something of the like.
    *   
    *   (The 'ascent height' is the distance between the TOP of the box
    *   containing any character in the font and the BASELINE.  The baseline
    *   is defined by the font's designer, but in general it's the alignment
    *   point for the bottoms of the capital letters and of the minuscules
    *   that don't have tails (tails: g, j, p, q, y - the bits that drop
    *   below the baseline).  The reason the ascent height is important is
    *   that it tells us where the baseline is relative to the overall
    *   rectangle containing a character in the font, and the baseline is
    *   important because it's the reference point for aligning adjacent
    *   text from different fonts.)  
    */
   virtual CHtmlPoint measure_text(class CHtmlSysFont *font,
                                   const textchar_t *str, size_t len,
                                   int *ascent);

   /*
    *   Get the size of the debugger source window control icon.  This is
    *   a small icon in the left margin of a line of text displayed in a
    *   debugger source window; the icon is meant to show the current
    *   status of the line (current line, breakpoint set on line, etc).
    *   This routine is only needed if the platform implements the TADS
    *   debugger; provide an empty implementation if you're not
    *   implementing the debugger on your platform. 
    */
   virtual CHtmlPoint measure_dbgsrc_icon();

   /*
    *   Determine how much of a given string can fit in a given width,
    *   assuming that the text is all on one line.  This routine is an
    *   optimization that allows an operating system mechanism to be used
    *   directly when we need to figure out how much text we can put on a
    *   line.  If the operating system doesn't provide a native mechanism
    *   for determining this, this routine should scan the string using the
    *   OS's text measuring mechanism.  In such cases, an efficient means
    *   should be used, such as a binary search through the string for the
    *   appropriate length.  If the entire string fits in the given width,
    *   the length of the string should be returned.
    *   
    *   The return value is expressed in byte units, even for multi-byte
    *   character sets.  The system implementation must take care to handle
    *   multi-byte characters properly, if the system supports MBCS's.  
    */
   virtual size_t get_max_chars_in_width(class CHtmlSysFont *font,
                                         const textchar_t *str,
                                         size_t len, long wid);

   /*
    *   Draw text.  The text is to be drawn with its upper left corner
    *   at the given position.  If hilite is true, the text should be
    *   drawn with selection highlighting; otherwise, it should be drawn
    *   normally.  
    */
   virtual void draw_text(int hilite, long x, long y,
                          class CHtmlSysFont *font,
                          const textchar_t *str, size_t len);

   /*
    *   Draw typographical text space.  This is related to draw_text(), but
    *   rather than drawing text, this simply draws a space of the given
    *   pixel width in the given font.  The effect should be as though
    *   draw_text() were called with a string consisting of a number of
    *   spaces equal in total horizontal extent to the given pixel width
    *   (note, however, that the width need not be an integral multiple of
    *   the width of a single ordinary space, so it's not possible in all
    *   cases to obtain the identical effect with draw_text).  This can be
    *   used for purposes such as drawing typographical spaces (spaces of
    *   particular special sizes), or inserting extra space for flush
    *   justification.
    *   
    *   Note that all of the same attributes of regular text drawing should
    *   apply.  Highlighting should work the same as with ordinary text,
    *   and if the font would draw ordinary space characters with any sort
    *   of decoration (such as underlines, strikethroughs, or overbars),
    *   the same decorations should be applied here.
    *   
    *   The simplest and safest implementation of this function is as
    *   follows.  Measure the size of an ordinary space in the given font.
    *   Start at the given x,y coordinates.  With clipping to the full
    *   height of the font and the given width, draw a space character.
    *   Move right by the width of the space.  Repeat until past the
    *   desired drawing width.  This approach will ensure that the space
    *   drawn will have exactly the same appearance as ordinary spaces; the
    *   iteration will ensure enough space is filled, and the clipping will
    *   ensure that there is no drawing beyond the desired space.  
    */
   virtual void draw_text_space(int hilite, long x, long y,
                                class CHtmlSysFont *font, long wid);

   /* draw a bullet */
   virtual void draw_bullet(int hilite, long x, long y,
                            class CHtmlSysFont *font,
                            HTML_SysWin_Bullet_t style);

   /*
    *   Draw a horizontal rule covering the given rectangle.  If shade is
    *   true, draw with 3D-style shading, otherwise draw as a solid bar.
    */
   virtual void draw_hrule(const CHtmlRect *pos, int shade);

   /*
    *   Draw a table or cell border.  The border should be drawn inside
    *   the given rectangle with the given width.  If cell is true, this
    *   is an individual cell border, otherwise it's the overall table's
    *   border.  
    */
   virtual void draw_table_border(const CHtmlRect *pos, int width,
                                  int cell);

   /*
    *   Draw a table or cell's background.  This should simply fill in
    *   the given area with the given color. 
    */
   virtual void draw_table_bkg(const CHtmlRect *pos,
                               HTML_color_t bgcolor);

   /*
    *   Draw a debugger source line icon.  This is used only for the TADS
    *   debugger; if this platform doesn't implement the debugger, this
    *   routine can have an empty implementation.
    *   
    *   'stat' gives a status code; this is a combination of the
    *   HTMLTDB_STAT_xxx codes (see tadshtml.h).  
    */
   virtual void draw_dbgsrc_icon(const CHtmlRect *pos,
                                 unsigned int stat);


   /*
    *   Do formatting -- run the formatter until it runs out of content
    *   to format.  This routine can be implemented simply to call the
    *   formatter's formatting routine until the formatter reports that
    *   it's out of tags to format; however, we provide this method in
    *   the window class so that the window can optimize redrawing.  This
    *   routine should return true if it did any display updating, false
    *   if not.  If show_status is true, it indicates that the operation
    *   may be lengthy, so a status line message, wait cursor, and/or
    *   other indication of the busy status should be displayed;
    *   otherwise the operation is expected to finish quickly enough that
    *   no such display is desired.  show_status should be set to true
    *   when reformatting after operations that will reformat the entire
    *   window, such as resizing the window.  If update_win is true, we
    *   should update the window as soon as possible (i.e., as soon as
    *   we've formatted enough text to redraw the window); otherwise, we
    *   should leave redrawing to the normal draw message handler.  If
    *   freeze_display is true, the formatter should be told not to
    *   update the window until it's done; this is used when the whole
    *   window needs to be reformatted, such as after a font preference
    *   change, to avoid making the user watch the window's entire
    *   contents scroll by as the reformatting is done.
    */
   virtual int do_formatting(int show_status, int update_win,
                             int freeze_display);


   /* -------------------------------------------------------------------- */
   /*
    *   Palette management.  These routines are needed only if we're
    *   using an index-based palette to display colors, and then only if
    *   applications must manage the palette.  For systems with high
    *   color resolution (generally greater than 8-bit color) or
    *   automatic color table management, these routines can have empty
    *   implementations.  
    */
   
   /*
    *   Recalculate the palette.  The formatter calls this routine when
    *   it adds a new image to the list of initial images, which are used
    *   to determine the system palette.  If the system is not using a
    *   palette, this routine doesn't need to do anything.
    */
   virtual void recalc_palette() ;

   /*
    *   Determine if we need to use a palette.  If this system is
    *   currently using an index-based palette to select display colors,
    *   this should return true, otherwise it should return false.  Some
    *   systems use an index-based palette when the display hardware is
    *   showing 8 bits of color resolution or less (notably Windows).  
    */
   virtual int get_use_palette();

   /* -------------------------------------------------------------------- */
   /*
    *   Font management.  The window always owns all of the font
    *   objects, which means that it is responsible for tracking them and
    *   deleting them when the window is destroyed.  Fonts that are given
    *   to a formatter must remain valid as long as the window is in
    *   existence.  Note that this doesn't mean that the font class can't
    *   internally release system resources, if it's necessary on a given
    *   system to minimize the number of system font resources in use
    *   simultaneously -- the portable code can never access the actual
    *   system resources directly, so the implementation of this
    *   interface can manage the system resources internally as
    *   appropriate.  
    */
   
   /* get the default font */
   virtual class CHtmlSysFont *get_default_font();

   /* 
    *   Get a font suitable for the given characteristics.  Note that this
    *   routine is responsible for checking the font descriptor for the
    *   following parameterized names, and providing the appropriate actual
    *   system font when one of the parameterized names is used (the names
    *   are case-insensitive):
    *   
    *   TADS-serif - a serifed font, usually proportional
    *.  TADS-sans - a san serif font, usually proportional
    *.  TADS-script - a script or cursive font, usually proportional
    *.  TADS-typewriter - a typewriter-style font, usually fixed-width
    *.  TADS-input - the font to use for player commands
    *   
    *   When possible, the player should be able to select the parameterized
    *   fonts in a "preferences" dialog or similar mechanism.  The purpose
    *   of the parameterized fonts is to allow the game to specify the style
    *   to use, while letting the player specify the exact details of the
    *   presentation.
    *   
    *   Note that, on some systems, parameterized fonts might let the user
    *   specify attributes in addition to the font face.  For example, on
    *   the Windows interpreter, "TADS-Input" lets the user specify the text
    *   color and set boldface and italics.  When these additional
    *   attributes are part of a parameterized font name, they should be
    *   applied ONLY when the 'face_set_explicitly' flag in the font_desc is
    *   set to TRUE - this ensures that the attributes can be overridden by
    *   nesting other attribute tags (<font color=red> or <i>, for example)
    *   within the <font> tag that explicitly selects the font face.
    *   
    *   Note on memory management: the portable code never deletes a font
    *   object obtained from this routine.  Instead, this routine is
    *   expected to keep a list of font objects previously allocated, and to
    *   reuse an existing object if there's one in the list that matches the
    *   descriptor.  This ensures that the lack of deletion won't leak
    *   memory: a given font object is never deleted, but only one font
    *   object will ever be created for a given descriptor.  
    */
   virtual class CHtmlSysFont
       *get_font(const class CHtmlFontDesc *font_desc);

   /* 
    *   Get the font for drawing bullets, given a text font.  This can
    *   simply return the same font if there isn't a separate bullet font
    *   or bullets are drawn independently of the font.  The current font
    *   is provided so that a suitable size can be selected in the bullet
    *   font.  
    */
   virtual class CHtmlSysFont
       *get_bullet_font(class CHtmlSysFont *current_font);

   /* -------------------------------------------------------------------- */
   /*
    *   Timers
    */

   /*
    *   Register a timer callback routine.  This routine should be called
    *   with the given context data as its argument periodically.  If
    *   possible, the routine should be called about once per second, but
    *   the precise interval isn't important; all that's important is to
    *   call the function every so often so that the function can check
    *   for pending work.  Since the frequency of the callback invocation
    *   is not tightly specified, the callback must check the current
    *   time if it's time-sensitive.
    *   
    *   The callback should not be invoked via an interrupt or in a
    *   separate thread; instead, it should be invoked in the course of
    *   the window's normal event message dispatching mechanism.
    *   
    *   Note that any number of timer functions may be simultaneously
    *   registered.  The order in which such functions are called is
    *   unspecified.  
    */
   virtual void register_timer_func(void (*timer_func)(void *),
                                    void *func_ctx);

   /*
    *   Remove an timer callback function previously registered.  
    */
   virtual void unregister_timer_func(void (*timer_func)(void *),
                                      void *func_ctx);

   /*
    *   Create a timer object that calls a given callback function when the
    *   timer event occurs.  This doesn't register a timer, but merely
    *   creates a timer object that can be used in set_timer().
    *   
    *   Returns a CHtmlSysTimer object that can be used to unregister the
    *   timer.  
    */
   virtual class CHtmlSysTimer *create_timer(void (*timer_func)(void *),
                                             void *func_ctx);


   /*
    *   Set a timer, previously created with create_timer(), to the given
    *   interval in milliseconds.  If 'repeat' is true, we'll invoke the
    *   callback associated with the timer repeatedly at the given interval;
    *   otherwise, we'll invoke the callback just once and then make the
    *   timer inactive. 
    */
   virtual void set_timer(class CHtmlSysTimer *timer, long interval_ms,
                          int repeat);

   /*
    *   Cancel a timer. 
    */
   virtual void cancel_timer(class CHtmlSysTimer *timer);
   
   /*
    *   Delete a timer previously created with register_timer_func_ms().  If
    *   the timer is currently active, it is cancelled.  
    */
   virtual void delete_timer(class CHtmlSysTimer *timer);


   /* -------------------------------------------------------------------- */
   /*
    *   General HTML operations 
    */

   /*
    *   Adjust the horizontal scrollbar.  The formatter calls this whenever
    *   a new line is formatted that exceeds the maximum line width so far.
    *   This routine should adjust the horizontal scrollbar accordingly -
    *   that is, it should update the range (min/max) of the OS-level
    *   scrollbar control to match the new document width.
    *   
    *   Note that the routine should by default NOT perform any horizonal
    *   scrolling here - it should simply update the min/max RANGE of the
    *   scrollbar and leave the scrolling position unchanged.  However, if
    *   the window is a banner window with the style flag
    *   OS_BANNER_STYLE_AUTO_HSCROLL set (via create_banner_window() or
    *   set_banner_info()), then this routine should immediately scroll the
    *   window horizontally to bring the right edge of the document into
    *   view; the window should be invalidated and/or redrawn as needed, and
    *   the scrollbar's thumb position should be updated accordingly.  
    */
   virtual void fmt_adjust_hscroll();

   /*
    *   Adjust the vertical scrollbar.  The formatter calls this whenever a
    *   new line is added or the document otherwise grows vertically.  This
    *   routine should adjust the min/max range of the vertical scrollbar to
    *   match the new document height.
    *   
    *   Note that this routine should normally NOT perform any vertical
    *   scrolling - it should simply update the RANGE of the min/max
    *   scrollbar and leave the current scrolling position unchanged.
    *   However, if the window is a banner window with the style flag
    *   OS_BANNER_STYLE_AUTO_VSCROLL set (via create_banner_window() or
    *   set_banner_info()), then this routine should immediately scroll the
    *   window vertically to bring the bottom edge of the document into
    *   view; the window should be invalidated and/or redrawn as needed, and
    *   the scrollbar's thumb position should be updated accordingly.
    *   
    *   MORE MODE: We leave it up to the port code to determine how to
    *   handle MORE mode, if such a thing exists locally.  In a regular
    *   "main" window, or in a banner window with the style flag
    *   OS_BANNER_STYLE_MOREMODE set, the port should implement MORE mode
    *   according to local conventions.  One possible implementation is to
    *   do nothing special: since we presumably are showing the text in a
    *   standard GUI window with a scrollbar, any text that spills beyond
    *   the vertical extent of the window will simply be scrolled off the
    *   bottom, and we can leave it up to the user to manually use the
    *   scrollbar to see the text below the bottom of the window.  
    *   
    *   In most cases, though, it's best to simulate the behavior of the
    *   Unix "more" program, since (a) this is much more convenient for
    *   users, and (b) it's what most users are accustomed to in a
    *   terminal-style interface like ours.  Here's roughly how this works.
    *   Whenever fmt_adjust_vscroll() notifies the window that the document
    *   has grown vertically beyond the current vertical extent of the
    *   window, the window sets an internal flag saying "more mode is
    *   pending."  Whenever the program makes an input call (get_input(),
    *   get_input_timeout(), etc), the window checks the "more mode pending"
    *   flag, and if set, clears that flag and sets another one saying "more
    *   mode is active."  In the input routines, as long as the "more mode"
    *   flag is set, we treat keyboard input specially: the SPACE key
    *   scrolls down by a page, the RETURN key scrolls down by a "line"
    *   (which is an essentially arbitrary unit, since we can have a mixture
    *   of text heights in the window - the design height of the default or
    *   current font is usually a good choice), the up/down arrow keys
    *   scroll by a line one way or the other; assign other keys according
    *   to local conventions or your preferences, but generally ignore keys
    *   to which you haven't assigned a special more-mode meaning.  When in
    *   more mode, if at any time the vertical scroll position becomes such
    *   that the bottom of the document is in view, cancel more mode and
    *   switch to ordinary input mode.  In ordinary input mode, you'd
    *   normally use a different scrolling behavior: if the input caret
    *   isn't visible, and the user presses any key that affects the input,
    *   immediately jump directly to a scrolling position that brings the
    *   caret into view.  This allows the user to scroll up to look at old
    *   text in the transcript, but then immediately jump back to the
    *   command line just by starting to type some input.
    *   
    *   We leave all of this up to the port code, though, because we want to
    *   give each port maximum flexibility in applying local customs.  Some
    *   ports might not even want a MORE mode at all, and some might use
    *   different key mappings or different overall behaviors.  Some ports
    *   might provide a visual indication that MORE mode is active
    *   (something like the "[More]" prompt that the Unix "more" command and
    *   the TADS text-only interpreters display), while some might be
    *   content to rely on the scrollbars as a sufficient visual cue.  The
    *   Windows interpreter uses something like the traditional "[More]"
    *   prompt, but it has a couple of user-configurable options for how to
    *   display it: it can be shown in the status bar along the bottom of
    *   the window frame, or it can be displayed as an overlay within the
    *   document area of the window itself, superimposed over the document's
    *   own contents at the bottom of the window.  (The status line approach
    *   is a lot easier to implement.)  
    */
   virtual void fmt_adjust_vscroll();

   /*
    *   Invalidate the window area given by the document coordinates (i.e.,
    *   before taking into account any scrolling).
    *   
    *   Note that the area->right and area->bottom coordinates can be passed
    *   in as HTMLSYSWIN_MAX_RIGHT and/or HTMLSYSWIN_MAX_BOTTOM, defined
    *   above - these indicate that you should invalidate all the way to the
    *   right or bottom edge of the window.
    */
   virtual void inval_doc_coords(const class CHtmlRect *area);

   /*
    *   Receive notification that the formatter associated with this
    *   window is about to reset the display list, deleting all existing
    *   display list items.  Any display list items that the window is
    *   referencing should be forgotten.  It's particularly likely that
    *   the window is keeping track of a link display item, such as the
    *   link that the mouse is hovering over.  The window should simply
    *   forget about any such display list items at this time.  
    */
   virtual void advise_clearing_disp_list();

   /*
    *   Scroll a document position into view.  This should try to scroll
    *   the window so that the given coordinates appear at the middle of
    *   the window.  If possible, the entire rectangle should be within
    *   the window.  
    */
   virtual void scroll_to_doc_coords(const class CHtmlRect *pos);

   /*
    *   Get the current scrolling position in document coordinates.
    *   Returns a value suitable for passing to scroll_to_doc_coords to
    *   restore this same scrolling position at a later time.  
    */
   virtual void get_scroll_doc_coords(class CHtmlRect *pos);

   /* set the window's title */
   virtual void set_window_title(const textchar_t *title);

   /*
    *   Set the background color, text color, and link colors.  If
    *   use_default for a particular color is true, we'll ignore the
    *   provided color and use the default color instead.  These calls are
    *   used to advise the system window implementation of the color
    *   settings made programmatically, such as through the <BODY> tag.
    *   
    *   The "background" color is the color used to fill the background of
    *   the window, where no text or graphics are explicitly drawn.
    *   
    *   The "text" color is the default color for ordinary text (i.e., text
    *   that isn't hyperlinked or given an explicit font color via <FONT
    *   COLOR=xxx> or the like).
    *   
    *   The "input" color is the color to use for input text.
    *   
    *   The "link" color is the color for hyperlinked text - that is, text
    *   inside an <A HREF> tag.
    *   
    *   The "vlink" color is the "visited" hyperlink text color.  We include
    *   this for completeness, but currently we don't use it, since we don't
    *   really have a sense of visited vs unvisited links in the same way
    *   that a browser would.
    *   
    *   The "alink" color is the color for "active" hyperlink text.  This is
    *   the color for hyperlink text when the link is being clicked by the
    *   mouse; that is, when the user has moved the mouse over the hyperlink
    *   and then clicked and held down the button.  This is the color to use
    *   as long as the button is being held down.
    *   
    *   The "hlink" color is the "hover" hyperlink text color.  This is the
    *   color for when the mouse is hovering over the hyperlink, but the
    *   mouse button isn't being held down.
    *   
    *   Note that the system implementation is free to ignore all of these
    *   color settings, or to use them only if user preferences allow, or
    *   under any other conditions suitable for the local system.  On most
    *   systems, there are four possible color sources for any given text
    *   fragment: user preferences; set_html_xxx_color() settings; explicit
    *   <FONT> and other settings; and OS defaults.  The interpreter then
    *   implements a hierarchy to determine which of these to use.  The
    *   exact hierarchy is up to the interpreter, but it's usually like
    *   this:
    *   
    *   - If the user preferences allow the user to select an "override"
    *   option, and the user selects this option, use the user preference
    *   color setting applicable to a given text fragment.  (The user
    *   preferences might allow the user to select each of these color types
    *   individually - text, background, input, alink, vlink, etc - in which
    *   case the interpreter should pick the right one for the context.  If
    *   the preferences don't distinguish color by context, just use the
    *   single color setting.)
    *   
    *   - If "override" isn't selected (or isn't offered), and there's a
    *   <FONT COLOR=xxx> setting for a given text fragment, use that.
    *   
    *   - Otherwise, if there's been a call to set_html_xxx_color() for the
    *   given context with use_default=FALSE, use that color.
    *   
    *   - If there's been no call to set_html_xxx_color() for the given
    *   context, or there's been a call with use_default=TRUE, AND there's a
    *   user preference setting applicable to the context, use the user
    *   preference setting.
    *   
    *   - Otherwise, use the system defaults.  
    */
   virtual void set_html_bg_color(HTML_color_t color, int use_default);
   virtual void set_html_text_color(HTML_color_t color, int use_default);
   virtual void set_html_input_color(HTML_color_t clr, int use_default);
   virtual void set_html_link_colors(HTML_color_t link_color,
                                     int link_use_default,
                                     HTML_color_t vlink_color,
                                     int vlink_use_default,
                                     HTML_color_t alink_color,
                                     int alink_use_default,
                                     HTML_color_t hlink_color,
                                     int hlink_use_default);

   /*
    *   Map a special parameterized color value to the corresponding RGB
    *   value.  The parameterized color values are the HTML_COLOR_xxx
    *   values defined in tadshtml.h.  The system should use an
    *   appropriate mechanism for determining the colors; the system may
    *   use fixed values, or may choose colors appropriate for the
    *   current display device, or can let the user control these
    *   settings through a preferences mechanism. 
    */
   virtual HTML_color_t map_system_color(HTML_color_t color);

   /* 
    *   Get the link colors.  The link colors are stored in the window
    *   object, so that the system-specific code can save, use, and
    *   customize user preferences as needed.  The colors are normal (link),
    *   activated/clicked (alink), visited (vlink), and hovering (hlink).
    *   These are the colors that are used to draw hyperlinked text in the
    *   respective states.
    *   
    *   Note that these should return the colors appropriate for the current
    *   system-specific preference settings.  For example, if the user
    *   preferences the user to select whether or not links are highlighted
    *   on "hovering" with the mouse cursor, and hover highlighting is
    *   currently turned off, then the hlink color should return the same as
    *   the normal link color.  
    */
   virtual HTML_color_t get_html_link_color() const;
   virtual HTML_color_t get_html_alink_color() const;
   virtual HTML_color_t get_html_vlink_color() const;
   virtual HTML_color_t get_html_hlink_color() const;

   /* determine if textual links should be drawn underlined */
   virtual int get_html_link_underline() const;

   /*
    *   Determine if links are to be shown.  This should be offered as a
    *   user-settable preference if possible.  Returns true if links
    *   should be shown as links, false if not.  If links are not to be
    *   shown as links, we'll draw links as ordinary text.  
    */
   virtual int get_html_show_links() const;

   /*
    *   Determine if graphics can be displayed.  On systems where
    *   graphics can be displayed, this should be offered as a
    *   user-settable preference if possible.  On systems where graphics
    *   can't be displayed at all, this should always return false.  If
    *   this returns false, we'll render graphics using a suitable
    *   textual substitute if possible, such as the ALT attribute of an
    *   IMG tag, or simply leave any graphics out entirely.  If this
    *   returns true, we'll render graphics as normal.  
    */
   virtual int get_html_show_graphics() const;

   /*
    *   Set the background image.  The image is an ordinary HTML resource
    *   cache entry, whose get_image() method will return a CHtmlSysImage
    *   object (or null if there's not a valid object associated with the
    *   cache entry).
    *   
    *   In graphical implementations, the background image is normally drawn
    *   as the background of the document, tiled to fill out the window
    *   space.  The image should scroll with the document.  As the document
    *   scrolls down, the image is normally repeatedly tiled vertically to
    *   fill in all displayed space.  The image is normally drawn out to the
    *   visible edges of the window; no empty margin space is normally shown
    *   between the interior edges of the window's frame and the image.  
    */
   virtual void set_html_bg_image(class CHtmlResCacheObject *image);

   /*
    *   Invalidate a portion of the background image.  This is called when
    *   the background image is animated to trigger redrawing of the
    *   portions of the image changed from one animation frame to the next.
    *   
    *   The coordinates given are relative to the upper left corner of the
    *   background image itself.  These are not in document or window
    *   coordinates - they're in *image* coordinates.  The portable caller
    *   can't make any assumptions about the placement of the background
    *   image relative to the window or document coordinate systems, so we
    *   must rely on the window itself to figure out what needs to be
    *   invalidated within the image.
    *   
    *   It's important to note that if the image is tiled, so that it's
    *   drawn multiple times in the window, then *each visible tile* must be
    *   invalidated here.  If all of the visible tiles are not invalidated,
    *   they would incorrectly remain un-updated - but they'd draw in their
    *   new form eventually when the window is next updated by virtue of
    *   having a portion uncovered.  Therefore, it's crucial that this
    *   routine properly invalidate all visible copies of the image.  
    */
   virtual void inval_html_bg_image(unsigned int x, unsigned int y,
                                    unsigned int wid, unsigned int ht);

   /*
    *   Set the size of this window as a banner.  A call to this routine
    *   should be ignored for a main window.
    *   
    *   Note that normally only one of the dimensions is actually used.  If
    *   this is a horizontal banner, normally only the height is meaningful,
    *   because horizontal banners are constrained to run across the entire
    *   width of the main window.  Similarly, only the width is meaningful
    *   for vertical banners.
    *   
    *   If use_height is false, it means that the height parameter value
    *   should be ignored and the current window height retained; likewise
    *   use_width.  
    */
   virtual void set_banner_size(
       long width, HTML_BannerWin_Units_t width_units, int use_width,
       long height, HTML_BannerWin_Units_t height_units, int use_height);

   /*
    *   Set alignment and style for this existing banner window.  A call to
    *   this routine should be ignored for a main window.
    *   
    *   'style' is a combination of OS_BANNER_STYLE_xxx flags, as defined
    *   in osifc.h (from tads 2).  
    */
   virtual void set_banner_info(HTML_BannerWin_Pos_t pos,
                                unsigned long style);

   /*
    *   Get information on this banner window.  '*pos' is filled in with the
    *   banner's alignment, and '*style' is filed in with a combination of
    *   OS_BANNER_STYLE_xxx flags describing the styles actually provided by
    *   the window.
    *   
    *   Note that the style flags returned might not be the same ones as
    *   originally requested in CHtmlSysFrame::create_banner_window(),
    *   because we might not support some styles that the caller requested,
    *   and we might unconditionally use some styles not requested.  The
    *   style flags returned here describe the *actual* window, not the
    *   requested one.  
    */
   virtual void get_banner_info(HTML_BannerWin_Pos_t *pos,
                                unsigned long *style);

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