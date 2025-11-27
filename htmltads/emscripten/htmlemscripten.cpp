#include "htmlemscripten.h"

/*
 *   Kill the process 
 */
void CHtmlSysFrame::kill_process()
{
    exit(0);
}

CHtmlSys_mainwin::CHtmlSys_mainwin(class CHtmlFormatterInput *formatter,
                     class CHtmlParser *parser, int in_debugger) : parser_(parser){
	/* set the global frame object pointer to point to me */
	CHtmlSysFrame::set_frame_obj(this);
}

CHtmlSys_mainwin::~CHtmlSys_mainwin(){
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

}

void CHtmlSys_mainwin::start_new_page() {

}

void CHtmlSys_mainwin::display_output(const textchar_t* buf, size_t len) {
	fputs(buf, stdout);
	fflush(stdout);
}

void CHtmlSys_mainwin::display_output_quoted(const textchar_t* buf, size_t len) {

}


textchar_t CHtmlSys_mainwin::wait_for_keystroke(int pause_only) {
	return getchar();
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