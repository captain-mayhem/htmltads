#include "htmlemscripten.h"
#include "htmlprs.h"
#include "htmlfmt.h"
#include "emfont.h"

/*
 *   Kill the process 
 */
void CHtmlSysFrame::kill_process()
{
    exit(0);
}


CHtmlSysWin_emscripten::CHtmlSysWin_emscripten(class CHtmlFormatter *formatter) : CHtmlSysWin(formatter){
    CHtmlRect margins;

    /* set a small inset from the margins */
    margins.set(8, 2, 8, 2);

    /* create the default font */
    CHtmlFontDesc font_desc;
    font_desc.htmlsize = 3;
    font_desc.default_charset = FALSE;
    //font_desc.charset = owner->owner_get_default_charset();
    //default_font_ = (CHtmlSysFont_win32 *)get_font(&font_desc);

    formatter_->set_win(this, &margins);
}

CHtmlSysWin_emscripten::~CHtmlSysWin_emscripten(){

}

class CHtmlSysWinGroup *CHtmlSysWin_emscripten::get_win_group(){
    return nullptr;
}

void CHtmlSysWin_emscripten::notify_clear_contents(){

}

int CHtmlSysWin_emscripten::close_window(int force){
    return 0;
}

long CHtmlSysWin_emscripten::get_disp_width(){
    return 1024;
}

long CHtmlSysWin_emscripten::get_disp_height(){
    return 768;
}

long CHtmlSysWin_emscripten::get_pix_per_inch(){
    return 72;
}

CHtmlPoint CHtmlSysWin_emscripten::measure_text(class CHtmlSysFont *font,
                                   const textchar_t *str, size_t len,
                                   int *ascent){
    return CHtmlPoint(100, 20);
}

CHtmlPoint CHtmlSysWin_emscripten::measure_dbgsrc_icon(){
    return CHtmlPoint(0, 0);
}

size_t CHtmlSysWin_emscripten::get_max_chars_in_width(class CHtmlSysFont *font,
                                         const textchar_t *str,
                                         size_t len, long wid){
                                            return 200;
                                         }

void CHtmlSysWin_emscripten::draw_text(int hilite, long x, long y,
                          class CHtmlSysFont *font,
                          const textchar_t *str, size_t len){
    printf("at %ix%i: %s\n", x, y, str);
                          }

void CHtmlSysWin_emscripten::draw_text_space(int hilite, long x, long y,
                                class CHtmlSysFont *font, long wid){

                                }

void CHtmlSysWin_emscripten::draw_bullet(int hilite, long x, long y,
                            class CHtmlSysFont *font,
                            HTML_SysWin_Bullet_t style){

                            }

void CHtmlSysWin_emscripten::draw_hrule(const CHtmlRect *pos, int shade){

}

void CHtmlSysWin_emscripten::draw_table_border(const CHtmlRect *pos, int width,
                                  int cell){

}

void CHtmlSysWin_emscripten::draw_table_bkg(const CHtmlRect *pos,
                               HTML_color_t bgcolor){

}

void CHtmlSysWin_emscripten::draw_dbgsrc_icon(const CHtmlRect *pos,
                                 unsigned int stat){

}

int CHtmlSysWin_emscripten::do_formatting(int show_status, int update_win,
                             int freeze_display){
    int drawn = FALSE;
    while(formatter_->more_to_do()){
        formatter_->do_formatting();
    }
    return drawn;
}

void CHtmlSysWin_emscripten::recalc_palette(){

}

int CHtmlSysWin_emscripten::get_use_palette(){
    return FALSE;
}

class CHtmlSysFont *CHtmlSysWin_emscripten::get_default_font(){
    CHtmlFontDesc desc;

    /* get a font matching the current defaults */
    desc.htmlsize = 3;
    desc.default_charset = FALSE;
    //desc.charset = owner_->owner_get_default_charset();
    return get_font(&desc);
}

class CHtmlSysFont
       *CHtmlSysWin_emscripten::get_font(const class CHtmlFontDesc *font_desc){
        return new CHtmlSysFont_emscripten();
}

class CHtmlSysFont
       *CHtmlSysWin_emscripten::get_bullet_font(class CHtmlSysFont *current_font){
        return nullptr;
       }

void CHtmlSysWin_emscripten::register_timer_func(void (*timer_func)(void *),
                                    void *func_ctx){

}

void CHtmlSysWin_emscripten::unregister_timer_func(void (*timer_func)(void *),
                                      void *func_ctx){

}

class CHtmlSysTimer *CHtmlSysWin_emscripten::create_timer(void (*timer_func)(void *),
                                             void *func_ctx){
    return nullptr;
}

void CHtmlSysWin_emscripten::set_timer(class CHtmlSysTimer *timer, long interval_ms,
                          int repeat){

                          }

void CHtmlSysWin_emscripten::cancel_timer(class CHtmlSysTimer *timer){

}
   
void CHtmlSysWin_emscripten::delete_timer(class CHtmlSysTimer *timer){

}

void CHtmlSysWin_emscripten::fmt_adjust_hscroll(){

}

void CHtmlSysWin_emscripten::fmt_adjust_vscroll(){

}

void CHtmlSysWin_emscripten::inval_doc_coords(const class CHtmlRect *area){

}

void CHtmlSysWin_emscripten::advise_clearing_disp_list(){

}

void CHtmlSysWin_emscripten::scroll_to_doc_coords(const class CHtmlRect *pos){

}

void CHtmlSysWin_emscripten::get_scroll_doc_coords(class CHtmlRect *pos){

}

void CHtmlSysWin_emscripten::set_window_title(const textchar_t *title){

}

void CHtmlSysWin_emscripten::set_html_bg_color(HTML_color_t color, int use_default){

}

void CHtmlSysWin_emscripten::set_html_text_color(HTML_color_t color, int use_default){

}

void CHtmlSysWin_emscripten::set_html_input_color(HTML_color_t clr, int use_default){

}

void CHtmlSysWin_emscripten::set_html_link_colors(HTML_color_t link_color,
                                     int link_use_default,
                                     HTML_color_t vlink_color,
                                     int vlink_use_default,
                                     HTML_color_t alink_color,
                                     int alink_use_default,
                                     HTML_color_t hlink_color,
                                     int hlink_use_default){

}

HTML_color_t CHtmlSysWin_emscripten::map_system_color(HTML_color_t color){
    return color;
}

HTML_color_t CHtmlSysWin_emscripten::get_html_link_color() const{
    return 0;
}

HTML_color_t CHtmlSysWin_emscripten::get_html_alink_color() const{
    return 0;
}

HTML_color_t CHtmlSysWin_emscripten::get_html_vlink_color() const{
    return 0;
}

HTML_color_t CHtmlSysWin_emscripten::get_html_hlink_color() const{
    return 0;
}

int CHtmlSysWin_emscripten::get_html_link_underline() const{
    return FALSE;
}

int CHtmlSysWin_emscripten::get_html_show_links() const{
    return TRUE;
}

int CHtmlSysWin_emscripten::get_html_show_graphics() const{
    return TRUE;
}

void CHtmlSysWin_emscripten::set_html_bg_image(class CHtmlResCacheObject *image){

}

void CHtmlSysWin_emscripten::inval_html_bg_image(unsigned int x, unsigned int y,
                                    unsigned int wid, unsigned int ht){

                                    }

void CHtmlSysWin_emscripten::set_banner_size(
       long width, HTML_BannerWin_Units_t width_units, int use_width,
       long height, HTML_BannerWin_Units_t height_units, int use_height){

       }

void CHtmlSysWin_emscripten::set_banner_info(HTML_BannerWin_Pos_t pos,
                                unsigned long style){

                                }

void CHtmlSysWin_emscripten::get_banner_info(HTML_BannerWin_Pos_t *pos,
                                unsigned long *style){

                                }

void CHtmlSysWin_emscripten::doPaint(){
    CHtmlRect area;
    area.set(0, 0, 1000000, 1000000);
    int clip_lines = FALSE;
    long clip_ypos_ = 0;
    /* draw everything in the client area */
    if (formatter_ != 0)
        formatter_->draw(&area, clip_lines, &clip_ypos_);
}



CHtmlSysWin_emscripten_Input::CHtmlSysWin_emscripten_Input(class CHtmlFormatterInput *formatter) : CHtmlSysWin_emscripten(formatter){

}



CHtmlSys_mainwin::CHtmlSys_mainwin(class CHtmlFormatterInput *formatter,
                     class CHtmlParser *parser, int in_debugger) : parser_(parser){
	/* create a text buffer */
	txtbuf_ = new CHtmlTextBuffer();
	/* set the global frame object pointer to point to me */
	CHtmlSysFrame::set_frame_obj(this);
	/* create the main HTML display panel window */
	main_panel_ = new CHtmlSysWin_emscripten_Input(formatter);
}

CHtmlSys_mainwin::~CHtmlSys_mainwin(){
	delete main_panel_;
	/* clear out the global frame object pointer */
	CHtmlSysFrame::set_frame_obj(0);
	/* done with our text buffer */
	delete txtbuf_;
}

void CHtmlSys_mainwin::dbg_print(const char *msg, int foreground){
	fputs(msg, stdout);
	fflush(stdout);
}

class CHtmlSysWin* CHtmlSys_mainwin::create_banner_window(class CHtmlSysWin* parent,
	HTML_BannerWin_Type_t typ,
	class CHtmlFormatter* formatter,
	int where, class CHtmlSysWin* other,
	HTML_BannerWin_Pos_t pos, unsigned long style) {
	return nullptr;
}

class CHtmlSysWin
	* CHtmlSys_mainwin::create_aboutbox_window(class CHtmlFormatter* formatter) {
	return nullptr;
}

void CHtmlSys_mainwin::remove_banner_window(class CHtmlSysWin* subwin) {

}

void CHtmlSys_mainwin::orphan_banner_window(CHtmlFormatterBannerExt* fmt) {

}

int CHtmlSys_mainwin::get_exe_resource(const textchar_t* resname, size_t resnamelen,
	textchar_t* fname_buf, size_t fname_buf_len,
	unsigned long* seek_pos,
	unsigned long* siz) {
	return 0;
}

int CHtmlSys_mainwin::get_input_timeout(textchar_t* buf, size_t bufsiz,
	unsigned long timeout, int use_timeout) {

	/* flush output and make sure it's displayed */
	flush_txtbuf(TRUE, FALSE);

	if (fgets(buf, bufsiz, stdin) == nullptr){
		return OS_EVT_EOF;
	}
	int len = strlen(buf);
	if (len > 0){
		buf[len-1] = '\0';
	}
	return OS_EVT_LINE;
}

void CHtmlSys_mainwin::get_input_cancel(int reset) {

}


int CHtmlSys_mainwin::get_input_event(unsigned long timeout_in_milliseconds,
	int use_timeout, os_event_info_t* info) {
	return 0;
}

void CHtmlSys_mainwin::set_nonstop_mode(int flag) {

}

int CHtmlSys_mainwin::check_break_key() {
	return 0;
}


void CHtmlSys_mainwin::flush_txtbuf(int fmt, int immediate_redraw) {
	/* parse what's in the buffer */
	parser_->parse(txtbuf_, this);

	/* we're done with the text in the buffer - clear it out */
	txtbuf_->clear();

	if (fmt){
        main_panel_->do_formatting(FALSE, FALSE, FALSE);
        //main_panel_->doPaint();
	}
}

void CHtmlSys_mainwin::start_new_page() {

}

void CHtmlSys_mainwin::display_output(const textchar_t* buf, size_t len) {
	fputs(buf, stdout);
	fflush(stdout);
	/* add the text to the buffer we're accumulating */
	txtbuf_->append(buf, len);
}

void CHtmlSys_mainwin::display_output_quoted(const textchar_t* buf, size_t len) {

}


textchar_t CHtmlSys_mainwin::wait_for_keystroke(int pause_only) {
    flush_txtbuf(TRUE, FALSE);
	return getchar();
}

#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SYMBOL_CHARSET 2
#define EASTEUROPE_CHARSET 238
#define CP_SYMBOL 42
/*
 *   Map an HTML 4 code point value 
 */
size_t CHtmlSys_mainwin::xlat_html4_entity(textchar_t *result,
                                           size_t result_size,
                                           unsigned int charval,
                                           oshtml_charset_id_t *charset,
                                           int *changed_charset)
{
    oshtml_charset_id_t new_charset;
    size_t len;
    
    /* presume we're going to return a single byte */
    result[1] = '\0';
    len = 1;

    /* check the character to see what range it's in */
    if (charval < 128)
    {
        /* 
         *   it's in the ASCII range, so assume that it's part of the
         *   ASCII subset of every Windows character set -- return the
         *   value unchanged and in the current default character set 
         */
        result[0] = (textchar_t)charval;
        new_charset = default_charset_;
    }
    else if (charval >= 160 && charval <= 255)
    {
        /*
         *   It's in the 160-255 range, which is the ISO Latin-1 range for
         *   HTML 4.  These map trivially to Windows code page 1252
         *   characters, so leave the value unchanged and use the ANSI
         *   character set.  
         */
        result[0] = (textchar_t)charval;
        new_charset = oshtml_charset_id_t(ANSI_CHARSET, 1252);
        len = 1;
    }
    else
    {
        const int ANY_125X_CHARSET = -1;
        static struct
        {
            unsigned int html_val;
            unsigned int win_val;
            int          win_charset;
            int          codepage;
        } mapping[] =
        {
            /*
             *   IMPORTANT: This table must be sorted by html_val, because
             *   we perform a binary search on this key.  
             */

            /* 
             *   Latin-2 characters and additional alphabetic characters.
             *   Most of these characters exist only in the Windows
             *   Eastern European code page, but a few are in the common
             *   Windows extended code page set (code points 128-159) and
             *   a few are only in the ANSI code page.  
             */
            { 258, 0xC3, EASTEUROPE_CHARSET, 1250 },              /* Abreve */
            { 259, 0xE3, EASTEUROPE_CHARSET, 1250 },              /* abreve */
            { 260, 0xA5, EASTEUROPE_CHARSET, 1250 },               /* Aogon */
            { 261, 0xB9, EASTEUROPE_CHARSET, 1250 },               /* aogon */
            { 262, 0xC6, EASTEUROPE_CHARSET, 1250 },              /* Cacute */
            { 263, 0xE6, EASTEUROPE_CHARSET, 1250 },              /* cacute */
            { 268, 0xC8, EASTEUROPE_CHARSET, 1250 },              /* Ccaron */
            { 269, 0xE8, EASTEUROPE_CHARSET, 1250 },              /* ccaron */
            { 270, 0xCF, EASTEUROPE_CHARSET, 1250 },              /* Dcaron */
            { 271, 0xEF, EASTEUROPE_CHARSET, 1250 },              /* dcaron */
            { 272, 0xD0, EASTEUROPE_CHARSET, 1250 },              /* Dstrok */
            { 273, 0xF0, EASTEUROPE_CHARSET, 1250 },              /* dstrok */
            { 280, 0xCA, EASTEUROPE_CHARSET, 1250 },               /* Eogon */
            { 281, 0xEA, EASTEUROPE_CHARSET, 1250 },               /* eogon */
            { 282, 0xCC, EASTEUROPE_CHARSET, 1250 },              /* Ecaron */
            { 283, 0xEC, EASTEUROPE_CHARSET, 1250 },              /* ecaron */
            { 313, 0xC5, EASTEUROPE_CHARSET, 1250 },              /* Lacute */
            { 314, 0xE5, EASTEUROPE_CHARSET, 1250 },              /* lacute */
            { 317, 0xBC, EASTEUROPE_CHARSET, 1250 },              /* Lcaron */
            { 318, 0xBE, EASTEUROPE_CHARSET, 1250 },              /* lcaron */
            { 321, 0xA3, EASTEUROPE_CHARSET, 1250 },              /* Lstrok */
            { 322, 0xB3, EASTEUROPE_CHARSET, 1250 },              /* lstrok */
            { 323, 0xD1, EASTEUROPE_CHARSET, 1250 },              /* Nacute */
            { 324, 0xF1, EASTEUROPE_CHARSET, 1250 },              /* nacute */
            { 327, 0xD2, EASTEUROPE_CHARSET, 1250 },              /* Ncaron */
            { 328, 0xF2, EASTEUROPE_CHARSET, 1250 },              /* ncaron */
            { 336, 0xD5, EASTEUROPE_CHARSET, 1250 },              /* Odblac */
            { 337, 0xF5, EASTEUROPE_CHARSET, 1250 },              /* odblac */
            { 338,  140, ANSI_CHARSET, 1252 },                     /* OElig */
            { 339,  156, ANSI_CHARSET, 1252 },                     /* oelig */
            { 340, 0xC0, EASTEUROPE_CHARSET, 1250 },              /* Racute */
            { 341, 0xE0, EASTEUROPE_CHARSET, 1250 },              /* racute */
            { 344, 0xD8, EASTEUROPE_CHARSET, 1250 },              /* Rcaron */
            { 345, 0xF8, EASTEUROPE_CHARSET, 1250 },              /* rcaron */
            { 346, 0x8C, EASTEUROPE_CHARSET, 1250 },              /* Sacute */
            { 347, 0x9C, EASTEUROPE_CHARSET, 1250 },              /* sacute */
            { 350, 0xAA, EASTEUROPE_CHARSET, 1250 },              /* Scedil */
            { 351, 0xBA, EASTEUROPE_CHARSET, 1250 },              /* scedil */
            { 352,  138, ANSI_CHARSET, 1252 },                    /* Scaron */
            { 353,  154, ANSI_CHARSET, 1252 },                    /* scaron */
            { 354, 0xDE, EASTEUROPE_CHARSET, 1250 },              /* Tcedil */
            { 355, 0xFE, EASTEUROPE_CHARSET, 1250 },              /* tcedil */
            { 356, 0x8D, EASTEUROPE_CHARSET, 1250 },              /* Tcaron */
            { 357, 0x9D, EASTEUROPE_CHARSET, 1250 },              /* tcaron */
            { 366, 0xD9, EASTEUROPE_CHARSET, 1250 },               /* Uring */
            { 367, 0xF9, EASTEUROPE_CHARSET, 1250 },               /* uring */
            { 368, 0xDB, EASTEUROPE_CHARSET, 1250 },              /* Udblac */
            { 369, 0xFB, EASTEUROPE_CHARSET, 1250 },              /* udblac */
            { 376,  159, ANSI_CHARSET, 1252 },                      /* Yuml */
            { 377, 0x8F, EASTEUROPE_CHARSET, 1250 },              /* Zacute */
            { 378, 0x9F, EASTEUROPE_CHARSET, 1250 },              /* zacute */
            { 379, 0xAF, EASTEUROPE_CHARSET, 1250 },                /* Zdot */
            { 380, 0xBF, EASTEUROPE_CHARSET, 1250 },                /* zdot */
            { 381, 0x8E, EASTEUROPE_CHARSET, 1250 },              /* Zcaron */
            { 382, 0x9E, EASTEUROPE_CHARSET, 1250 },              /* zcaron */

            /* Latin-extended B */
            { 402, 166, SYMBOL_CHARSET, CP_SYMBOL },                /* fnof */

            /* Latin-2 accents */
            { 710, 136, ANSI_CHARSET, 1252 },                       /* circ */
            { 711, 0xA1, EASTEUROPE_CHARSET, 1250 },               /* caron */
            { 728, 0xA2, EASTEUROPE_CHARSET, 1250 },               /* breve */
            { 729, 0xFF, EASTEUROPE_CHARSET, 1250 },                 /* dot */
            { 731, 0xB2, EASTEUROPE_CHARSET, 1250 },                /* ogon */
            { 732, 152, ANSI_CHARSET, 1252 },                      /* tilde */
            { 733, 0xBD, EASTEUROPE_CHARSET, 1250 },               /* dblac */

            /* Greek letters */
            { 913, 'A', SYMBOL_CHARSET, CP_SYMBOL },               /* Alpha */
            { 914, 'B', SYMBOL_CHARSET, CP_SYMBOL },                /* Beta */
            { 915, 'G', SYMBOL_CHARSET, CP_SYMBOL },               /* Gamma */
            { 916, 'D', SYMBOL_CHARSET, CP_SYMBOL },               /* Delta */
            { 917, 'E', SYMBOL_CHARSET, CP_SYMBOL },             /* Epsilon */
            { 918, 'Z', SYMBOL_CHARSET, CP_SYMBOL },                /* Zeta */
            { 919, 'H', SYMBOL_CHARSET, CP_SYMBOL },                 /* Eta */
            { 920, 'Q', SYMBOL_CHARSET, CP_SYMBOL },               /* Theta */
            { 921, 'I', SYMBOL_CHARSET, CP_SYMBOL },                /* Iota */
            { 922, 'K', SYMBOL_CHARSET, CP_SYMBOL },               /* Kappa */
            { 923, 'L', SYMBOL_CHARSET, CP_SYMBOL },              /* Lambda */
            { 924, 'M', SYMBOL_CHARSET, CP_SYMBOL },                  /* Mu */
            { 925, 'N', SYMBOL_CHARSET, CP_SYMBOL },                  /* Nu */
            { 926, 'X', SYMBOL_CHARSET, CP_SYMBOL },                  /* Xi */
            { 927, 'O', SYMBOL_CHARSET, CP_SYMBOL },             /* Omicron */
            { 928, 'P', SYMBOL_CHARSET, CP_SYMBOL },                  /* Pi */
            { 929, 'R', SYMBOL_CHARSET, CP_SYMBOL },                 /* Rho */
            { 931, 'S', SYMBOL_CHARSET, CP_SYMBOL },               /* Sigma */
            { 932, 'T', SYMBOL_CHARSET, CP_SYMBOL },                 /* Tau */
            { 933, 'U', SYMBOL_CHARSET, CP_SYMBOL },             /* Upsilon */
            { 934, 'F', SYMBOL_CHARSET, CP_SYMBOL },                 /* Phi */
            { 935, 'C', SYMBOL_CHARSET, CP_SYMBOL },                 /* Chi */
            { 936, 'Y', SYMBOL_CHARSET, CP_SYMBOL },                 /* Psi */
            { 937, 'W', SYMBOL_CHARSET, CP_SYMBOL },               /* Omega */
            { 945, 'a', SYMBOL_CHARSET, CP_SYMBOL },               /* alpha */
            { 946, 'b', SYMBOL_CHARSET, CP_SYMBOL },                /* beta */
            { 947, 'g', SYMBOL_CHARSET, CP_SYMBOL },               /* gamma */
            { 948, 'd', SYMBOL_CHARSET, CP_SYMBOL },               /* delta */
            { 949, 'e', SYMBOL_CHARSET, CP_SYMBOL },             /* epsilon */
            { 950, 'z', SYMBOL_CHARSET, CP_SYMBOL },                /* zeta */
            { 951, 'h', SYMBOL_CHARSET, CP_SYMBOL },                 /* eta */
            { 952, 'q', SYMBOL_CHARSET, CP_SYMBOL },               /* theta */
            { 953, 'i', SYMBOL_CHARSET, CP_SYMBOL },                /* iota */
            { 954, 'k', SYMBOL_CHARSET, CP_SYMBOL },               /* kappa */
            { 955, 'l', SYMBOL_CHARSET, CP_SYMBOL },              /* lambda */
            { 956, 'm', SYMBOL_CHARSET, CP_SYMBOL },                  /* mu */
            { 957, 'n', SYMBOL_CHARSET, CP_SYMBOL },                  /* nu */
            { 958, 'x', SYMBOL_CHARSET, CP_SYMBOL },                  /* xi */
            { 959, 'o', SYMBOL_CHARSET, CP_SYMBOL },             /* omicron */
            { 960, 'p', SYMBOL_CHARSET, CP_SYMBOL },                  /* pi */
            { 961, 'r', SYMBOL_CHARSET, CP_SYMBOL },                 /* rho */
            { 962, 'V', SYMBOL_CHARSET, CP_SYMBOL },              /* sigmaf */
            { 963, 's', SYMBOL_CHARSET, CP_SYMBOL },               /* sigma */
            { 964, 't', SYMBOL_CHARSET, CP_SYMBOL },                 /* tau */
            { 965, 'u', SYMBOL_CHARSET, CP_SYMBOL },             /* upsilon */
            { 966, 'f', SYMBOL_CHARSET, CP_SYMBOL },                 /* phi */
            { 967, 'c', SYMBOL_CHARSET, CP_SYMBOL },                 /* chi */
            { 968, 'y', SYMBOL_CHARSET, CP_SYMBOL },                 /* psi */
            { 969, 'w', SYMBOL_CHARSET, CP_SYMBOL },               /* omega */
            { 977, 'J', SYMBOL_CHARSET, CP_SYMBOL },            /* thetasym */
            { 978, 161, SYMBOL_CHARSET, CP_SYMBOL },               /* upsih */
            { 982, 'v', SYMBOL_CHARSET, CP_SYMBOL },                 /* piv */

            /* additional punctuation */
            { 8211, 150, ANY_125X_CHARSET, 0 },                    /* ndash */
            { 8212, 151, ANY_125X_CHARSET, 0 },                    /* mdash */
            { 8216, 145, ANY_125X_CHARSET, 0 },                    /* lsquo */
            { 8217, 146, ANY_125X_CHARSET, 0 },                    /* rsquo */
            { 8218, 130, ANY_125X_CHARSET, 0 },                    /* sbquo */
            { 8220, 147, ANY_125X_CHARSET, 0 },                    /* ldquo */
            { 8221, 148, ANY_125X_CHARSET, 0 },                    /* rdquo */
            { 8222, 132, ANY_125X_CHARSET, 0 },                    /* bdquo */
            { 8224, 134, ANY_125X_CHARSET, 0 },                   /* dagger */
            { 8225, 135, ANY_125X_CHARSET, 0 },                   /* Dagger */
            { 8226, 149, ANY_125X_CHARSET, 0 },                     /* bull */
            { 8230, 133, ANY_125X_CHARSET, 0 },                   /* hellip */
            { 8240, 137, ANY_125X_CHARSET, 0 },                   /* permil */
            { 8242, 162, SYMBOL_CHARSET, CP_SYMBOL },              /* prime */
            { 8243, 178, SYMBOL_CHARSET, CP_SYMBOL },              /* Prime */
            { 8249, 139, ANY_125X_CHARSET, 1252 },                /* lsaquo */
            { 8250, 155, ANY_125X_CHARSET, 1252 },                /* rsaquo */
            { 8254, '`', SYMBOL_CHARSET, CP_SYMBOL },              /* oline */
            { 8260, 164, SYMBOL_CHARSET, CP_SYMBOL },              /* frasl */
            
            /* letter-like symbols */
            { 8465, 193, SYMBOL_CHARSET, CP_SYMBOL },              /* image */
            { 8472, 195, SYMBOL_CHARSET, CP_SYMBOL },             /* weierp */
            { 8476, 194, SYMBOL_CHARSET, CP_SYMBOL },               /* real */
            { 8482, 153, ANY_125X_CHARSET, 0 },                    /* trade */
            { 8501, 192, SYMBOL_CHARSET, CP_SYMBOL },            /* alefsym */
            
            /* arrows */
            { 8592, 172, SYMBOL_CHARSET, CP_SYMBOL },               /* larr */
            { 8593, 173, SYMBOL_CHARSET, CP_SYMBOL },               /* uarr */
            { 8594, 174, SYMBOL_CHARSET, CP_SYMBOL },               /* rarr */
            { 8595, 175, SYMBOL_CHARSET, CP_SYMBOL },               /* darr */
            { 8596, 171, SYMBOL_CHARSET, CP_SYMBOL },               /* harr */
            { 8629, 191, SYMBOL_CHARSET, CP_SYMBOL },              /* crarr */
            { 8656, 220, SYMBOL_CHARSET, CP_SYMBOL },               /* lArr */
            { 8657, 221, SYMBOL_CHARSET, CP_SYMBOL },               /* uArr */
            { 8658, 222, SYMBOL_CHARSET, CP_SYMBOL },               /* rArr */
            { 8659, 223, SYMBOL_CHARSET, CP_SYMBOL },               /* dArr */
            { 8660, 219, SYMBOL_CHARSET, CP_SYMBOL },               /* hArr */
            
            /* mathematical operators */
            { 8704, 34, SYMBOL_CHARSET, CP_SYMBOL },              /* forall */
            { 8706, 182, SYMBOL_CHARSET, CP_SYMBOL },               /* part */
            { 8707, '$', SYMBOL_CHARSET, CP_SYMBOL },              /* exist */
            { 8709, 198, SYMBOL_CHARSET, CP_SYMBOL },              /* empty */
            { 8711, 209, SYMBOL_CHARSET, CP_SYMBOL },              /* nabla */
            { 8712, 206, SYMBOL_CHARSET, CP_SYMBOL },               /* isin */
            { 8713, 207, SYMBOL_CHARSET, CP_SYMBOL },              /* notin */
            { 8715, '\'', SYMBOL_CHARSET, CP_SYMBOL },                /* ni */
            { 8719, 213, SYMBOL_CHARSET, CP_SYMBOL },               /* prod */
            { 8721, 229, SYMBOL_CHARSET, CP_SYMBOL },                /* sum */
            { 8722, '-', SYMBOL_CHARSET, CP_SYMBOL },              /* minus */
            { 8727, '*', SYMBOL_CHARSET, CP_SYMBOL },             /* lowast */
            { 8730, 214, SYMBOL_CHARSET, CP_SYMBOL },              /* radic */
            { 8733, 181, SYMBOL_CHARSET, CP_SYMBOL },               /* prop */
            { 8734, 165, SYMBOL_CHARSET, CP_SYMBOL },              /* infin */
            { 8736, 208, SYMBOL_CHARSET, CP_SYMBOL },                /* ang */
            { 8743, 217, SYMBOL_CHARSET, CP_SYMBOL },                /* and */
            { 8744, 218, SYMBOL_CHARSET, CP_SYMBOL },                 /* or */
            { 8745, 199, SYMBOL_CHARSET, CP_SYMBOL },                /* cap */
            { 8746, 200, SYMBOL_CHARSET, CP_SYMBOL },                /* cup */
            { 8747, 242, SYMBOL_CHARSET, CP_SYMBOL },                /* int */
            { 8756, '\\', SYMBOL_CHARSET, CP_SYMBOL },            /* there4 */
            { 8764, '~', SYMBOL_CHARSET, CP_SYMBOL },                /* sim */
            { 8773, '@', SYMBOL_CHARSET, CP_SYMBOL },               /* cong */
            { 8776, 187, SYMBOL_CHARSET, CP_SYMBOL },              /* asymp */
            { 8800, 185, SYMBOL_CHARSET, CP_SYMBOL },                 /* ne */
            { 8801, 186, SYMBOL_CHARSET, CP_SYMBOL },              /* equiv */
            { 8804, 163, SYMBOL_CHARSET, CP_SYMBOL },                 /* le */
            { 8805, 179, SYMBOL_CHARSET, CP_SYMBOL },                 /* ge */
            { 8834, 204, SYMBOL_CHARSET, CP_SYMBOL },                /* sub */
            { 8835, 201, SYMBOL_CHARSET, CP_SYMBOL },                /* sup */
            { 8836, 203, SYMBOL_CHARSET, CP_SYMBOL },               /* nsub */
            { 8838, 205, SYMBOL_CHARSET, CP_SYMBOL },               /* sube */
            { 8839, 202, SYMBOL_CHARSET, CP_SYMBOL },               /* supe */
            { 8853, 197, SYMBOL_CHARSET, CP_SYMBOL },              /* oplus */
            { 8855, 196, SYMBOL_CHARSET, CP_SYMBOL },             /* otimes */
            { 8869, '^', SYMBOL_CHARSET, CP_SYMBOL },               /* perp */
            { 8901, 215, SYMBOL_CHARSET, CP_SYMBOL },               /* sdot */
            { 8968, 233, SYMBOL_CHARSET, CP_SYMBOL },              /* lceil */
            { 8969, 249, SYMBOL_CHARSET, CP_SYMBOL },              /* rceil */
            { 8970, 235, SYMBOL_CHARSET, CP_SYMBOL },             /* lfloor */
            { 8971, 251, SYMBOL_CHARSET, CP_SYMBOL },             /* rfloor */
            { 9001, 225, SYMBOL_CHARSET, CP_SYMBOL },               /* lang */
            { 9002, 241, SYMBOL_CHARSET, CP_SYMBOL },               /* rang */

            /* geometric shapes */
            { 9674, 224, SYMBOL_CHARSET, CP_SYMBOL },                /* loz */

            /* miscellaneous symbols */
            { 9824, 170, SYMBOL_CHARSET, CP_SYMBOL },             /* spades */
            { 9827, 167, SYMBOL_CHARSET, CP_SYMBOL },              /* clubs */
            { 9829, 169, SYMBOL_CHARSET, CP_SYMBOL },             /* hearts */
            { 9830, 168, SYMBOL_CHARSET, CP_SYMBOL }            /* diamonds */
        };
        int hi, lo;

        /*
         *   Find the character in the mapping table 
         */
        lo = 0;
        hi = sizeof(mapping)/sizeof(mapping[0]) - 1;
        for (;;)
        {
            int cur;
            
            /* if there's nothing left in the table, we didn't find it */
            if (lo > hi)
            {
                /* use the invalid character in the default character set */
                result[0] = invalid_char_val_;
                new_charset = default_charset_;
                break;
            }

            /* split the difference */
            cur = lo + (hi - lo)/2;

            /* is this the one we're looking for? */
            if (mapping[cur].html_val == charval)
            {
                /* this is it */
                result[0] = (textchar_t)mapping[cur].win_val;
                new_charset.charset = mapping[cur].win_charset;
                new_charset.codepage = mapping[cur].codepage;

                /* 
                 *   If the character set to use is the default, use the
                 *   current system default.
                 */
                if (new_charset.charset == DEFAULT_CHARSET)
                    new_charset = default_charset_;

                /*
                 *   If the new character set is the special ANY_125X_CHARSET
                 *   value, it means any Windows 125x character set will
                 *   work, because it's a character that's in common (and at
                 *   the same code point) in all of these.  In these cases,
                 *   we can simply use the default character set if it's a
                 *   Windows 125x character set.  Otherwise, we'll have to
                 *   fall back on 1252.  
                 */
                if (new_charset.charset == ANY_125X_CHARSET)
                {
                    /* 
                     *   we can use the default character set if it's a 125x
                     *   code page; otherwise we have to use 1252 
                     */
                    if (default_charset_.codepage >= 1250
                        && default_charset_.codepage <= 1259)
                    {
                        /* use the current character set */
                        new_charset = default_charset_;
                    }
                    else
                    {
                        /* use code page 1252 */
                        new_charset.charset = ANSI_CHARSET;
                        new_charset.codepage = 1252;
                    }
                }

                /*
                 *   If the character set is not the default, and we're
                 *   not allowed to change character sets, return the
                 *   invalid character. 
                 */
                if (new_charset.charset != default_charset_.charset
                    && charset == 0)
                {
                    result[0] = invalid_char_val_;
                    new_charset = default_charset_;
                }

                /* done */
                break;
            }
            else if (mapping[cur].html_val < charval)
            {
                /* we want something higher - look in the upper half */
                lo = (cur == lo ? lo + 1 : cur);
            }
            else
            {
                /* we want something smaller - look in the lower half */
                hi = (cur == hi ? hi - 1 : cur);
            }
        }
    }

    /* return information on a character set change, if possible */
    if (charset != 0)
    {
        /* tell them the new character set */
        *charset = new_charset;

        /* tell them whether it's the same as the default character set */
        *changed_charset = !new_charset.equals(default_charset_);
    }
    else if (!new_charset.equals(default_charset_))
    {
        /* 
         *   We need to change to a non-default character set, but the caller
         *   doesn't allow this.  We cannot map the character in this case,
         *   because the character doesn't exist in the caller's character
         *   set.  Return a single missing-character mapping.  
         */
        result[0] = invalid_char_val_;
        len = 1;
    }

    /* return the number of bytes in the result */
    return len;
}

/* pause before exiting */
void CHtmlSys_mainwin::pause_for_exit() {

}

/* show the MORE prompt */
void CHtmlSys_mainwin::pause_for_more() {

}

extern "C"{
int os_show_popup_menu(int default_pos, int x, int y,
                   const char *txt, size_t txtlen, os_event_info_t *evt)
{
	return 0;
}
}