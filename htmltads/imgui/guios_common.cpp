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

#include "guios.h"


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
