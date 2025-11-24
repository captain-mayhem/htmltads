#include <htmlsys.h>

/*
 *   Kill the process 
 */
void CHtmlSysFrame::kill_process()
{
    exit(0);
}

extern "C"{
int os_show_popup_menu(int default_pos, int x, int y,
                   const char *txt, size_t txtlen, os_event_info_t *evt)
{
	return 0;
}
}