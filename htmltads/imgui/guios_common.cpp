/*
 *   guios_common.cpp - platform-independent backend for the guit3 OS-service
 *   hooks (guios.h)
 *
 *   Most guios.h hooks have a genuinely different implementation per platform
 *   and live in guios_w32.cpp / guios_portable.cpp.  A few are the same C++
 *   everywhere; those live here, and this file is compiled into every build
 *   alongside exactly one of the per-platform backends (see
 *   htmltads/imgui/CMakeLists.txt).
 */

#include <chrono>
#include <cstring>

#include <GLFW/glfw3.h>

#include "tadshtml.h"     /* th_malloc / th_free */
#include "guios.h"


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard (plain text, UTF-8)
 *
 *   GLFW moves UTF-8 bytes to/from the system clipboard on every platform
 *   (Win32: CF_UNICODETEXT), so set/get are the same code everywhere and
 *   live here rather than in the per-platform backends.  The window argument
 *   has been deprecated-and-ignored since GLFW 3.0, so NULL is fine.
 *
 *   The engine's text is local-codepage; the copy/paste call sites in
 *   htmlgui.cpp convert with os_local_to_utf8() / os_utf8_to_local() (item K)
 *   on the way through.  CR/LF normalization stays the caller's job, exactly
 *   as with the old CF_TEXT path.
 *
 *   os_clipboard_has_text() is deliberately NOT here - see guios.h.
 */

int os_clipboard_set_text(const char *text)
{
    glfwSetClipboardString(NULL, text);
    return 1;
}

char *os_clipboard_get_text(void)
{
    const char *s = glfwGetClipboardString(NULL);
    if (s == NULL)
        return NULL;

    /* GLFW owns 's' only until the next clipboard call, so copy it now */
    size_t len = strlen(s) + 1;
    char *result = (char *)th_malloc(len);
    if (result != NULL)
        memcpy(result, s, len);
    return result;
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Millisecond tick clock
 *
 *   std::chrono::steady_clock is monotonic and high-resolution on every
 *   target we care about (MSVC backs it with QueryPerformanceCounter), which
 *   makes it a strict upgrade over the old Win32 GetTickCount() - finer
 *   granularity, and callers only ever diff two readings anyway.  Measured
 *   from the first call; as an unsigned long the count wraps after ~49 days
 *   of process uptime where long is 32-bit (Windows) and effectively never
 *   where it is 64-bit.
 */
unsigned long os_get_tick_ms(void)
{
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return (unsigned long)
        duration_cast<milliseconds>(steady_clock::now() - start).count();
}
