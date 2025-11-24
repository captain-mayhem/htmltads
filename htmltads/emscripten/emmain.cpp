/* TADS OS header */
#include "os.h"

#include "t3main.h"

int main(int argc, char** argv){
	appctxdef appctx;
	/* 
	 *   clear out the application context - if any fields are added in
	 *   the future that we don't deal with, this will ensure they're
	 *   cleared out 
	 */
	memset(&appctx, 0, sizeof(appctx));
	int ret = os0main2(argc, argv, t3main, "", nullptr, &appctx);
	return ret;
}
