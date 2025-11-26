/* TADS OS header */
#include "os.h"

#include "t3main.h"
#include "htmlemscripten.h"
#include <htmlprs.h>
#include <htmlfmt.h>
#include <htmlrf.h>

int main(int argc, char** argv){
	appctxdef appctx;
	/* 
	 *   clear out the application context - if any fields are added in
	 *   the future that we don't deal with, this will ensure they're
	 *   cleared out 
	 */
	memset(&appctx, 0, sizeof(appctx));
	appctx.usage_app_name = "htmlt3";
	
	CHtmlParser* parser = new CHtmlParser(TRUE);
	CHtmlFormatterInput *formatter = new CHtmlFormatterInput(parser);
	CHtmlSys_mainwin *win = new CHtmlSys_mainwin(formatter, parser, FALSE);
	
	formatter->get_res_finder()->init_appctx(&appctx);
	formatter->get_res_finder()->set_debugger_mode(FALSE);
	
	int ret = os0main2(argc, argv, t3main, "", nullptr, &appctx);
	
	formatter->release_parser();
	delete parser;
	delete formatter;
	return ret;
}
