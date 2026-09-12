/*
 *   guios_portable.cpp - non-Windows backend for the guit3 OS-service hooks
 *   (guios.h)
 *
 *   This is the cross-platform counterpart of guios_w32.cpp: same hooks, no
 *   <windows.h>.  CMake selects exactly one backend per build - guios_w32.cpp
 *   on WIN32, this file everywhere else (see htmltads/imgui/CMakeLists.txt).
 *
 *   Coverage as of M3 (see migration.md 5.4/B, D-F, K, 5.5):
 *     - D. clipboard has_text  glfwGetClipboardString probe (set/get are the
 *                              shared glfwSet/GetClipboardString() path in
 *                              guios_common.cpp)
 *     - D. wait cursor ...... no-op (GLFW has no busy cursor shape; see below)
 *     - E. system colors .... fixed sensible values
 *     - F. shell ............ xdg-open / open via fork+exec
 *     - B. resources ........ generated string table (below) + the embedded
 *                              runtbar.bmp / license.txt byte arrays
 *                              (guires_data.h)
 *     - K. character encoding TADS charmap layer (charmap.h), routed through
 *                              a small codepage-number -> table-name map
 *                              (below)
 *
 *   D's tick clock is platform-independent (std::chrono) and lives in the
 *   shared guios_common.cpp, not here.
 *
 *   The build gate in CMakeLists.txt (if NOT WIN32 return()) is still
 *   closed, so nothing links this yet - that gate lifts in M4, which is also
 *   the first time any of this can actually be exercised.
 */

#ifdef _WIN32
#error "guios_portable.cpp is the non-Windows backend; Windows builds use guios_w32.cpp"
#endif

#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <GLFW/glfw3.h>

#include "tadshtml.h"     /* th_malloc / th_free */
#include "guios.h"
#include "guires_data.h"  /* embedded runtbar.bmp / license.txt bytes */
#include "htmlres.h"      /* IDS_* string ids */
#include "charmap.h"      /* CCharmapToUni / CCharmapToLocal - item K */
#include "resload.h"      /* CResLoader - finds charmap/<name>.tcm files */


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard - has_text only
 *
 *   set/get are the shared glfwSet/GetClipboardString() path in
 *   guios_common.cpp.  GLFW offers no format query, so the only "is there
 *   text" test is a full fetch; unlike Win32 (IsClipboardFormatAvailable),
 *   this backend has nothing cheaper, and can_paste() - which calls this -
 *   runs every frame from the toolbar.  Acceptable for now; a real
 *   non-Windows port can add a lighter probe if it matters.
 */

int os_clipboard_has_text(void)
{
    const char *s = glfwGetClipboardString(NULL);
    return s != NULL && s[0] != '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Wait cursor
 *
 *   GLFW 3.5 has no busy/hourglass standard cursor (only arrow, I-beam,
 *   crosshair, hand, the resize shapes and not-allowed), and the operations
 *   that want a busy cursor block the frame loop, so there is nothing useful
 *   to set here yet.  guit3 already shows a "Working..." status-line message
 *   alongside every one of these, so the visual cue is not lost.  A real busy
 *   cursor would need a bundled image fed through glfwCreateCursor() - future
 *   work, tracked with item B's other embedded assets.
 */
os_cursor_token_t os_set_wait_cursor(void)
{
    return NULL;
}

void os_restore_cursor(os_cursor_token_t /*prev*/)
{
}


/* ------------------------------------------------------------------------ */
/*
 *   F. Shell integration
 */
int os_open_url(const char *url)
{
#if defined(__APPLE__)
    const char *opener = "open";
#else
    const char *opener = "xdg-open";
#endif

    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0)
    {
        /* child - detach from our process group so the browser outlives us,
           then hand off to the platform opener */
        setsid();
        execlp(opener, opener, url, (char *)NULL);
        _exit(127);
    }

    /* parent - xdg-open / open spawn the handler and exit promptly, so a
       blocking wait just reaps the child and reports whether it launched */
    int status = 0;
    if (waitpid(pid, &status, 0) != pid)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   E. System colors
 *
 *   Off Windows there is no single system palette to read (it varies by
 *   desktop environment and theme), so return fixed values that read well on
 *   a light UI.  Same packed 0x00BBGGRR encoding as a Win32 COLORREF, so the
 *   results still feed COLORREF_to_HTML_color() and GetRValue()/GetGValue()/
 *   GetBValue() unchanged.
 */
static unsigned long pack_bgr(unsigned r, unsigned g, unsigned b)
{
    return (unsigned long)r | ((unsigned long)g << 8) | ((unsigned long)b << 16);
}

unsigned long os_get_sys_color(os_sys_color_t which)
{
    switch (which)
    {
    case OS_SYS_COLOR_HIGHLIGHT:       return pack_bgr(0x00, 0x78, 0xD7); /* blue   */
    case OS_SYS_COLOR_HIGHLIGHT_TEXT:  return pack_bgr(0xFF, 0xFF, 0xFF); /* white  */
    case OS_SYS_COLOR_WINDOW:          return pack_bgr(0xFF, 0xFF, 0xFF); /* white  */
    case OS_SYS_COLOR_WINDOW_TEXT:     return pack_bgr(0x00, 0x00, 0x00); /* black  */
    }
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   B. Bundled resources
 *
 *   Windows pulls these out of the compiled .exe resources (LoadString(),
 *   LoadImage() of IDB_TERP_TOOLBAR, FindResource() of IDX_LICENSE_TEXT).
 *   There's no resource compiler off Windows, so this backend uses a
 *   generated string table (below - one entry per IDS_* id actually routed
 *   through os_load_string(), text copied verbatim from win32/htmlcmn.rc's
 *   STRINGTABLE) and the embedded runtbar.bmp / license.txt byte arrays in
 *   guires_data.h/.cpp (mechanically generated with `xxd -i`, not
 *   hand-maintained).
 */

namespace {

struct string_table_entry_t { int id; const char *text; };
const string_table_entry_t string_table[] =
{
    { IDS_MORE_PROMPT,           "  *** More ***  " },
    { IDS_MORE_STATUS_MSG,       "*** MORE *** [press the space bar to continue]" },
    { IDS_WORKING_MSG,           "Working..." },
    { IDS_PRESS_A_KEY_MSG,       "Please press a key..." },
    { IDS_EXIT_PAUSE_MSG,        "Press any key to exit." },
    { IDS_NO_GAME_MSG,           "(No game loaded.)" },
    { IDS_GAME_OVER_MSG,         "(The game has ended.)" },
    { IDS_FIND_NO_MORE,          "No more matches found" },
    { IDS_CANNOT_OPEN_HREF,      "Unable to start browser. You must have a web browser installed to show this link." },
    { IDS_LINK_PREF_CHANGE,      "Note: the Game Chest page always shows links, so your change to the 'Show Links' setting won't affect the current page. The change will take effect when you play a game using this theme." },
    { IDS_ABOUT_GAME_WIN_TITLE,  "About This Game" },
    { IDS_REALLY_NEW_GAME_MSG,   "Starting a new game will quit the current game without saving. Are you sure you want to proceed?" },
    { IDS_REALLY_QUIT_MSG,       "You are about to quit the game without saving. Do you really want to quit?" },
    { IDS_MANAGE_PROFILES,       "&Add/Delete Themes..." },
    { IDS_SET_DEF_PROFILE,       "Set \"%s\" as &Default Theme" },
    { IDS_CUSTOMIZE_THEME,       "&Customize \"%s\" Theme..." },
    { IDS_THEMES_DROPDOWN,       "Customize \"%s\" Theme" },
    { IDS_REALLY_GO_GC_MSG,      "This will quit the current game without saving your position - any work that you have done since you last saved will be lost. Do you really want to do this?" },
    { IDS_CHOOSE_NEW_GAME,       "Choose a game to load" },
    { IDS_ABOUTBOX_1,            "<font color=white face=Arial size=-1><b>Release " },
    { IDS_ABOUTBOX_2,            "</b></font><br><br><tab align=right><font face='Arial' size=-1><b><a forced href='http://www.tads.org/'>www.tads.org</a> &nbsp;&nbsp; <a forced href='credits'>Credits</a> &nbsp;&nbsp; <a forced href='license'>License</a> &nbsp;&nbsp; <a forced href='close'>Close</a></b></font>" },
    { IDS_CHOOSE_GAME,           "Select a TADS Game" },
    { IDS_THEMEDESC_MULTIMEDIA,  "The basic Windows look and feel." },
    { IDS_THEMEDESC_PLAIN_TEXT,  "A retro look recalling the classic text adventures of the 80's." },
    { IDS_THEMEDESC_WEB_STYLE,   "A clean, modern look based on popular Web page styles." },
};
const int string_table_cnt = sizeof(string_table) / sizeof(string_table[0]);

} // namespace

int os_load_string(int id, char *buf, size_t buflen)
{
    for (int i = 0 ; i < string_table_cnt ; ++i)
    {
        if (string_table[i].id == id)
        {
            size_t len = strlen(string_table[i].text);
            if (len >= buflen)
                len = buflen > 0 ? buflen - 1 : 0;
            memcpy(buf, string_table[i].text, len);
            if (buflen > 0)
                buf[len] = '\0';
            return (int)len;
        }
    }

    if (buflen > 0)
        buf[0] = '\0';
    return 0;
}

/*
 *   Minimal BMP reader for runtbar.bmp's exact format: an uncompressed,
 *   palette-indexed (1/4/8 bpp) Windows DIB, which is all a toolbar icon
 *   strip like this has ever needed.  Fields are read a byte at a time
 *   rather than through a packed struct, since BMP's on-disk layout doesn't
 *   match any C++ struct's natural alignment.  Returns a newly allocated
 *   top-down 32bpp RGBA buffer (th_malloc()'d) with *width/*height filled
 *   in, or null if the data isn't a BMP in one of these formats.
 */
namespace {

unsigned int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
unsigned int le32(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24);
}

unsigned char *decode_indexed_bmp(const unsigned char *data, size_t size,
                                  int *width, int *height)
{
    if (size < 54 || data[0] != 'B' || data[1] != 'M')
        return 0;

    unsigned int bits_offset = le32(data + 10);
    unsigned int hdr_size = le32(data + 14);
    int w = (int)le32(data + 18);
    int h = (int)le32(data + 22);
    unsigned int bitcount = le16(data + 28);
    unsigned int compression = le32(data + 30);
    unsigned int colors_used = le32(data + 46);

    if (compression != 0 /* BI_RGB */
        || (bitcount != 1 && bitcount != 4 && bitcount != 8)
        || w <= 0 || h == 0)
        return 0;

    int top_down = (h < 0);
    if (top_down)
        h = -h;

    if (colors_used == 0)
        colors_used = 1u << bitcount;
    const unsigned char *palette = data + 14 + hdr_size;
    if (palette + colors_used * 4 > data + size)
        return 0;

    unsigned int row_bytes = ((w * bitcount + 31) / 32) * 4;
    if (bits_offset + (size_t)row_bytes * h > size)
        return 0;

    unsigned char *pixels = (unsigned char *)th_malloc((size_t)w * h * 4);
    for (int y = 0 ; y < h ; ++y)
    {
        /* BMP rows are bottom-up unless the height field was negative */
        int src_row = top_down ? y : (h - 1 - y);
        const unsigned char *row = data + bits_offset
            + (size_t)src_row * row_bytes;
        unsigned char *out = pixels + (size_t)y * w * 4;

        for (int x = 0 ; x < w ; ++x)
        {
            unsigned int idx;
            if (bitcount == 8)
                idx = row[x];
            else if (bitcount == 4)
                idx = (x & 1) ? (row[x / 2] & 0x0f) : (row[x / 2] >> 4);
            else /* bitcount == 1 */
                idx = (row[x / 8] >> (7 - (x % 8))) & 1;

            const unsigned char *bgr = palette + idx * 4;
            out[x*4 + 0] = bgr[2];   /* R */
            out[x*4 + 1] = bgr[1];   /* G */
            out[x*4 + 2] = bgr[0];   /* B */
            out[x*4 + 3] = 0xFF;
        }
    }

    *width = w;
    *height = h;
    return pixels;
}

} // namespace

unsigned char *os_load_toolbar_rgba(int *width, int *height)
{
    unsigned char *pixels = decode_indexed_bmp(
        g_runtbar_bmp_data, g_runtbar_bmp_size, width, height);
    if (pixels == 0)
        return 0;

    /* same color-key -> alpha conversion as the Win32 backend: the top-left
       pixel's color is the mask color, turned into a zero alpha channel */
    unsigned char mask_r = pixels[0], mask_g = pixels[1], mask_b = pixels[2];
    int npix = (*width) * (*height);
    for (int i = 0 ; i < npix ; ++i)
    {
        unsigned char *p = pixels + i*4;
        p[3] = (p[0] == mask_r && p[1] == mask_g && p[2] == mask_b) ? 0 : 255;
    }

    return pixels;
}

char *os_load_license_text(size_t *len)
{
    char *result = (char *)th_malloc(g_license_txt_size);
    memcpy(result, g_license_txt_data, g_license_txt_size);
    *len = g_license_txt_size;
    return result;
}


/* ------------------------------------------------------------------------ */
/*
 *   K. Character encoding
 *
 *   The Win32 backend converts through a numeric Windows code page
 *   (MultiByteToWideChar/WideCharToMultiByte).  Off Windows there's no such
 *   thing, so this backend routes the same numeric code page through the
 *   TADS charmap layer the VM itself uses to load game character sets
 *   (charmap.h, charmap/<name>.tcm, bundled into charmap/cmaplib.t3r next to
 *   the built executable - see CMakeLists.txt's POST_BUILD step). The
 *   mapping from code page number to charmap table name is just "cp<N>" -
 *   that's the exact naming convention the table files under tads3/charmap/
 *   already use (cp1252.tcm, cp1250.tcm, ...); the one exception is 65001
 *   (CP_UTF8), which needs no table at all.  Loaded mappers are cached
 *   forever (there are only ever a handful of code pages in play in one
 *   process) and a table that fails to load falls back to plain ASCII
 *   rather than failing the conversion outright, so a missing/corrupt
 *   cmaplib.t3r degrades gracefully instead of losing all GUI text.
 *
 *   This mirrors guimain.cpp not yet existing off Windows (M4): there is no
 *   portable "directory the executable lives in" query wired up yet, so the
 *   CResLoader used here has no root directory and therefore searches the
 *   current working directory, same as CResLoader's other bare-constructor
 *   callers (e.g. msgcomp.cpp).  That's fine for the common case of running
 *   guit3 from its own install/build directory; if that proves too fragile
 *   once there's a real M4 Linux build to test against, give this its own
 *   exe-relative CResLoader the way t3main.cpp's MyHostIfc does.
 */

namespace {

void codepage_table_name(unsigned int codepage, char *buf, size_t buflen)
{
    if (codepage == 65001 /* CP_UTF8 */)
        snprintf(buf, buflen, "utf-8");
    else if (codepage == 0 /* CP_ACP */)
        snprintf(buf, buflen, "cp1252");   /* matches htmlgui.cpp's own default */
    else
        snprintf(buf, buflen, "cp%u", codepage);
}

CResLoader *cmap_res_loader()
{
    static CResLoader *loader = new CResLoader();
    return loader;
}

/* small linear-scan caches - only ever a handful of code pages are live */
struct to_uni_cache_entry_t { unsigned int codepage; CCharmapToUni *cmap; };
struct to_local_cache_entry_t { unsigned int codepage; CCharmapToLocal *cmap; };

CCharmapToUni *get_to_uni(unsigned int codepage)
{
    static to_uni_cache_entry_t cache[16];
    static int cache_cnt = 0;

    for (int i = 0 ; i < cache_cnt ; ++i)
        if (cache[i].codepage == codepage)
            return cache[i].cmap;

    char table_name[32];
    codepage_table_name(codepage, table_name, sizeof(table_name));

    CCharmapToUni *cmap = CCharmapToUni::load(cmap_res_loader(), table_name);
    if (cmap == 0)
        cmap = new CCharmapToUniASCII();

    if (cache_cnt < (int)(sizeof(cache) / sizeof(cache[0])))
        cache[cache_cnt++] = { codepage, cmap };

    return cmap;
}

CCharmapToLocal *get_to_local(unsigned int codepage)
{
    static to_local_cache_entry_t cache[16];
    static int cache_cnt = 0;

    for (int i = 0 ; i < cache_cnt ; ++i)
        if (cache[i].codepage == codepage)
            return cache[i].cmap;

    char table_name[32];
    codepage_table_name(codepage, table_name, sizeof(table_name));

    CCharmapToLocal *cmap = CCharmapToLocal::load(cmap_res_loader(), table_name);
    if (cmap == 0)
        cmap = new CCharmapToLocalASCII();

    if (cache_cnt < (int)(sizeof(cache) / sizeof(cache[0])))
        cache[cache_cnt++] = { codepage, cmap };

    return cmap;
}

} // namespace

char *os_local_to_utf8(unsigned int codepage,
                       const char *src, size_t srclen, size_t *out_len)
{
    CCharmapToUni *cmap = get_to_uni(codepage);

    size_t needed = cmap->map_str(0, 0, src, srclen);
    char *buf = (char *)th_malloc(needed + 1);
    cmap->map_str(buf, needed, src, srclen);
    buf[needed] = '\0';

    if (out_len != 0)
        *out_len = needed;
    return buf;
}

os_utf16_t *os_local_to_utf16(unsigned int codepage,
                              const char *src, size_t srclen, size_t *out_cnt)
{
    CCharmapToUni *cmap = get_to_uni(codepage);

    /* one local character can never expand to more than one code unit, so
       srclen units is always enough room */
    os_utf16_t *out = (os_utf16_t *)
        th_malloc((srclen > 0 ? srclen : 1) * sizeof(os_utf16_t));

    size_t n = 0;
    const char *p = src;
    size_t len = srclen;
    wchar_t ch;
    while (len > 0 && cmap->mapchar(ch, p, len))
        out[n++] = (os_utf16_t)ch;

    if (out_cnt != 0)
        *out_cnt = n;
    return out;
}

char *os_utf8_to_local(unsigned int codepage, const char *utf8, size_t *out_len)
{
    CCharmapToLocal *cmap = get_to_local(codepage);

    utf8_ptr src((char *)utf8);
    size_t needed = cmap->map_utf8z(0, 0, src);
    char *buf = (char *)th_malloc(needed + 1);
    cmap->map_utf8z(buf, needed + 1, src);
    buf[needed] = '\0';

    if (out_len != 0)
        *out_len = needed;
    return buf;
}
