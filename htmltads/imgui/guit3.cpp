#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32t3.cpp - TADS 3 version-specific implementation
Function
  
Notes
  
Modified
  01/20/00 MJRoberts  - Creation
*/

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include "tadsplat.h"
#endif

#include "t3main.h"
#include "guimain.h"
#include "hos_gui.h"
#include "guios.h"

/* include the TADS 2 VM version information */
#include "trd.h"

/* include the TADS 3 VM version information */
#include "vmvsn.h"

/* include some needed T3 headers */
#include "vmerr.h"
#include "vmimage.h"
#include "osifcnet.h"
#include "t3std.h"


/* ------------------------------------------------------------------------ */
/*
 *   ask the web UI window to yield the foreground
 */
void w32_webui_yield_foreground()
{
#ifdef _WIN32
    osnet_webui_yield_foreground();
#endif
}

/*
 *   bring the web UI window to the foreground
 */
void w32_webui_to_foreground()
{
#ifdef _WIN32
    osnet_webui_to_foreground();
#endif
}

/* ------------------------------------------------------------------------ */
/*
 *   Web UI launch hook: open a Web UI game's start page in the system's
 *   default Web browser.  guit3 has no bundled tadsweb.exe (that's a
 *   separate executable built only by the classic htmltads targets, and
 *   even then it lands in a different install/output directory than
 *   guit3.exe - see win32/osnet-connect.cpp's launch_tadsweb(), which looks
 *   for tadsweb.exe next to the running executable), so registering this
 *   hook (see guimain.cpp) replaces that lookup with os_open_url() (guios.h)
 *   - ShellExecute "open" on Windows, xdg-open/"open" elsewhere - which
 *   works with whatever browser the user has set as their default.
 *
 *   This gives up a few things the tadsweb.exe/named-pipe protocol
 *   provides - see the doc comment on os_webui_launch_hook_t in
 *   osifcnet.h for the details - but standard Web UI games built on
 *   lib/webui.t's getInputFile() don't depend on any of them, since that
 *   code path already has to work without them in client/server mode.
 */
int guit3_webui_launch_hook(const char *addr, int port, const char *path,
                            char **errmsg)
{
    char url[1024];
#ifdef _WIN32
    _snprintf(url, sizeof(url), "http://%s:%d%s", addr, port, path);
#else
    snprintf(url, sizeof(url), "http://%s:%d%s", addr, port, path);
#endif
    url[sizeof(url) - 1] = '\0';

    if (!os_open_url(url))
    {
        *errmsg = lib_copy_str(
            "Unable to open the game's start page in a Web browser");
        return FALSE;
    }

    *errmsg = 0;
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   list memory blocks in the T3 VM subsystem 
 */
#ifdef TADSHTML_DEBUG

void th_list_subsys_memory_blocks()
{
    t3_list_memory_blocks(&os_dbg_sys_msg);
}

#endif /* TADSHTML_DEBUG */

