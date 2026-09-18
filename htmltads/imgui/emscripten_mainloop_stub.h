/*
 *   EMSCRIPTEN_MAINLOOP_BEGIN/END - same stub Dear ImGui's own Emscripten
 *   examples use (e.g. examples/example_glfw_opengl3/main.cpp) to turn a
 *   blocking `while (...) { ... }` loop into a per-frame callback that
 *   emscripten_set_main_loop() can drive from the browser's requestAnimationFrame.
 *
 *   Scope note: this only gets CHtmlSys_mainwin::event_loop()'s *outermost*
 *   call - the real app main loop - to compile and step frame-by-frame.
 *   event_loop() is also re-entered recursively as a self-pumping blocking
 *   loop for modal dialogs (open_blocking(), tadswin_message_box() - see
 *   migration.md 3.3); emscripten_set_main_loop() is a single global
 *   registration, so a nested call still runs its lambda to completion
 *   synchronously inside the current callback invocation rather than
 *   yielding to the browser each frame the way the outer loop does. That
 *   makes nested/self-pumping dialogs synchronous-but-blocking under
 *   Emscripten instead of cooperating with the browser's frame pump -
 *   functionally incomplete, deliberately deferred (see migration.md 5.6)
 *   as real design work, not part of this compile-only pass.
 */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <functional>
static std::function<void()> MainLoopForEmscriptenP;
static void MainLoopForEmscripten() { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN MainLoopForEmscriptenP = [&]()
#define EMSCRIPTEN_MAINLOOP_END \
    ; \
    emscripten_set_main_loop(MainLoopForEmscripten, 0, true)
#endif
