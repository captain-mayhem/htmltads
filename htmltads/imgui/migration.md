# guit3 Migration Plan — from Win32 htmlt3 to cross-platform Dear ImGui

## 1. Where things live

- `htmltads/htmltads/win32/` — the original Win32 `htmlt3` application (untouched, kept as reference).
- `htmltads/htmltads/imgui/` — the **guit3** rewrite. It started as a copy of the `win32` sources
  (`git log` shows the first commit is literally "Copied platform specific headers for htmltads into
  new folder") and is being converted file-by-file to GLFW + Dear ImGui + OpenGL3, with FreeType for
  font rendering.
- `htmltads/imgui/` — vendored Dear ImGui + backends (`imgui_impl_glfw`,
  `imgui_impl_opengl3`) plus its own unused `main.cpp` demo (not part of the `guit3` target). Lives
  top-level alongside the other 3rdparty libs (`glfw`, `freetype`, etc.), not nested under
  `htmltads/htmltads/imgui/` — that directory is the `guit3` app itself, which includes it via
  `#include <imgui/imgui.h>` (its `guit3` CMakeLists.txt puts `htmltads/` on the include path).
- `htmltads/htmltads/emscripten/` — a **separate**, older, parallel port effort that talks to the T3
  VM/console layer directly for a browser build (`hos_emscripten.*`, `htmlemscripten.*`). It does not
  share code with `imgui/` and is not part of the `guit3` target. Worth reconciling later, but out of
  scope for this plan.
- Build target: `htmltads/htmltads/imgui/CMakeLists.txt` defines the `guit3` executable. **It currently
  hard-gates the whole target to Windows**:

  ```cmake
  # for now, while porting, we only allow win32
  if (NOT WIN32)
      return()
  endif()
  ```

  So today "platform independent" is aspirational — nothing in `guit3` builds on Linux/macOS yet, even
  though GLFW/OpenGL3/FreeType are all cross-platform. Removing this gate (incrementally, per-subsystem)
  is the end goal.

Progress so far (25+ commits): GLFW/ImGui window creation, keyboard input, FreeType-based font loading
and metrics, text display, child-window handling, coloring, resizing, link coloring, positioning,
character-encoding fixes, image rendering via GL textures, text selection, mouse hover, and text
highlighting. This is genuinely useful progress, and now the application chrome has its first
ImGui-native pieces too: the menu bar, toolbar, and status bar (§3.1, §3.2) are all done, and so are
all five dialogs - Options, Customize Theme, the file open/save dialog, the Find dialog, and the folder
picker (§3.3). The banner border, scrollbars, size-grip, and tooltip are no longer real child `HWND`s
either (§3.4), and — as of §3.4a — **neither is `handle_` itself**: every `CTadsWin`'s `handle_` is now
just an opaque token, there are no real Win32 windows in guit3 at all (only the one `GLFWwindow`), and the
timer / deferred-reformat / self-`WM_CLOSE` mechanisms that used to ride on it were reimplemented per
frame.

## 2. Root cause: "both windows open"

`CTadsWin::create_system_window()` in
[tadswin.cpp:407-452](tadswin.cpp#L407-L452) creates **two** windows for every top-level window:

```cpp
handle_ = sysifc->syswin_create_system_window(...);      // real Win32 HWND via CreateWindowEx
if (parent == nullptr) {
    m_window = sysifc->syswin_create_system_window(...); // GLFWwindow via glfwCreateWindow
}
```

- `CTadsSyswin::syswin_create_system_window(DWORD ex_style, ...)` ([tadswin.cpp:3318](tadswin.cpp#L3318))
  calls `CreateWindowEx` and returns a normal `HWND`.
- The overload `CTadsSyswin::syswin_create_system_window(const textchar_t*, DWORD, int,int,int,int)`
  ([tadswin.cpp:3334](tadswin.cpp#L3334)) calls `glfwCreateWindow(...)`.

Both run unconditionally for the top-level (`parent == nullptr`) window. `setVisible()`
([tadswin.cpp:2431](tadswin.cpp#L2431)) only calls `ShowWindow(handle_, ...)` on the Win32 HWND, but a
GLFW window is visible by default the moment `glfwCreateWindow` returns — so the GLFW/ImGui window pops
up on its own, and the leftover Win32 HWND also gets shown by `setVisible(true)`. The result: two
top-level windows on screen.

The Win32 `HWND` is also functionally dead: there is **no `GetMessage`/`DispatchMessage` pump anywhere
in the `guit3` sources** (confirmed via search — only isolated `PeekMessage` calls for specific custom
messages, and `TranslateMessage`/`DispatchMessage` in `CTadsApp::process_message()`
([tadsapp.cpp:444-445](tadsapp.cpp#L444-L445)), which nothing calls in a loop). So the stray HWND never
paints or processes input; it just sits there as a blank, frozen "TADS" window alongside the working
ImGui one. The actual rendering happens entirely in the GLFW/ImGui loop in
`CHtmlSys_mainwin::event_loop()` ([htmlgui.cpp:14437](htmlgui.cpp#L14437)).

**Why the HWND still gets created at all**: everything below the top-level window — banners
(`CreateWindow("TADS.BannerBorder", ...)`, [htmlgui.cpp:3736](htmlgui.cpp#L3736)), scrollbars
(`tadswin.cpp:2535/2550/2633`), the size-grip box, tooltips ([htmlgui.cpp:5360](htmlgui.cpp#L5360)),
the status bar, menu, and all dialogs — are still real Win32 child controls that need an `HWND` parent
to attach to. The port kept creating the Win32 top-level frame so those still-unported children have
somewhere to live.

**Recommended fix path**: this isn't a one-line fix, it's the crux of the whole migration. Two
reasonable approaches:
1. **Short term**: stop creating/showing the Win32 top-level HWND for the *main* window once nothing
   still parents to it (i.e. once menu bar, status bar, scrollbars and banners are ImGui widgets), or at
   minimum call `glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE)` / keep the HWND hidden. This just hides the
   symptom, not the underlying dual-window-system problem.
2. **Real fix**: eliminate `CTadsSyswin::syswin_create_system_window(DWORD ex_style, ...)` (the HWND
   path) for the main frame entirely, and reimplement each child ("window") that currently relies on a
   real Win32 `HWND`/`WNDPROC` as an ImGui-native construct (docked panel, custom-drawn widget, or an
   internal lightweight "window" object that doesn't need an OS handle). This is a large, incremental
   effort — see §3 for the breakdown.

**Status: short-term fix implemented.** `CTadsWin::setVisible()` ([tadswin.cpp:2431](tadswin.cpp#L2431))
now branches on whether the window owns a top-level `m_window`: if so, it calls
`glfwShowWindow`/`glfwHideWindow` on the GLFW window and never touches `handle_`'s visibility at all, so
the Win32 frame stays hidden permanently (it's still created, since scrollbars/banners/dialogs still
need a real `HWND` parent per §3.4). Separately, `CTadsSyswin::syswin_create_system_window(...)` (the
GLFW overload, [tadswin.cpp:3348](tadswin.cpp#L3348)) now sets `glfwWindowHint(GLFW_VISIBLE,
GLFW_FALSE)` before `glfwCreateWindow`, because GLFW shows new windows by default — without this hint
the GLFW window would flash visible immediately regardless of the caller's `show` argument, which was
actually the more visible half of the original "two windows" symptom. Verified by launching
`guit3.exe` and enumerating its windows: the `TADS_Window`-class HWND is now `Visible=False` and only
the `GLFW30`-class window is visible.

One related, smaller gap noticed while testing (not fixed here, left for later):
- `CHtmlSys_mainwin::show_normal()`/`bring_owner_to_front()` ([htmlgui.cpp:14136](htmlgui.cpp#L14136))
  still call `ShowWindow`/`BringWindowToTop`/`IsIconic` directly on `handle_`, which is now permanently
  hidden — minimize/restore/bring-to-front driven through those paths won't affect the real (GLFW)
  window until they're switched to the `glfw*` equivalents too.

**`-debugwin` opened an unmovable, unclickable window — fixed.** `guit3 -debugwin` (wired up in
[guimain.cpp:486-563](guimain.cpp#L486-L563)) creates a `CHtmlSys_dbglogwin`, a second top-level
`CTadsWin` (`parent == nullptr`) alongside the main window. Correction to an earlier note here: this
does *not* hit the "real Win32 HWND with no message pump" problem described above — the top-level
(`parent == nullptr`) branch of `CTadsWin::do_render_content_begin()`
([tadswin.cpp:1746](tadswin.cpp#L1746)) already renders *any* parentless `CTadsWin` as an ImGui
`Begin()`/`End()` window drawn inside the one real OS window, and `CHtmlSys_mainwin::event_loop()`
already calls `dbgwin_->do_render()` right after `main_win_->do_render()`
([htmlgui.cpp:15131-15135](htmlgui.cpp#L15131-L15135)) — so the debug log genuinely was an ImGui window
layered on top of the main one, exactly the shape wanted (confirmed by launching `guit3.exe -debugwin
tests/ditch3.t3` and screenshotting it: an "HTML Debug Log" panel with its own title bar/scrollbar
appeared correctly positioned over the game window). It just didn't work right, for two independent
reasons, both now fixed:

1. That same top-level branch passed `ImGuiWindowFlags_NoInputs` and called
   `ImGui::SetNextWindowPos()`/`SetNextWindowSize()` with no `ImGuiCond` (i.e. `ImGuiCond_Always`) every
   frame. `NoInputs` disables ImGui's own title-bar-drag/resize/focus/collapse handling for that window,
   and forcing the position every single frame would have fought a drag back to its fixed spot even
   without that flag — together these made the window fully inert (verified: simulating a title-bar drag
   via `SendInput`-style mouse events produced zero movement, pixel-for-pixel identical before/after).
   That flag combo turns out to be tailored for the *main* window specifically — `CHtmlSys_mainwin` has
   its own `do_render_content_begin()` override two clicks away
   ([htmlgui.cpp:11596](htmlgui.cpp#L11596)) that also uses `NoInputs`, deliberately, because the main
   window relies entirely on `event_loop()`'s hand-rolled `do_leftbtn_down()`/`do_mousemove()` mouse
   routing rather than ImGui's own widget input, and needs ImGui to never intercept clicks meant for that
   routing. The base-class top-level branch is the *only* thing a secondary floating window like the
   debug log goes through, since it doesn't get its own override — so it was inheriting main-window-only
   behavior it never wanted. Fixed by dropping `NoInputs` there and switching both `SetNextWindowPos()`
   and `SetNextWindowSize()` to `ImGuiCond_FirstUseEver`, then reading `ImGui::GetWindowPos()`/
   `GetWindowSize()` back into `m_pos`/`m_size` after `Begin()` so `get_screen_pos()` (used for the
   window's own mouse-coordinate math) stays correct once the user has dragged it. Left resizing off
   (`ImGuiWindowFlags_NoResize`) for now since `CTadsWin::do_resize()`'s child cascade is a no-op
   (commented out) — wiring live resize through to the debug window's HTML panel is a separate, later
   task.
2. Even with dragging fixed, the window's *content* still couldn't react to clicks: `event_loop()`'s
   mouse routing ([htmlgui.cpp:15098-15129](htmlgui.cpp#L15098-L15129)) unconditionally called
   `do_leftbtn_down()`/`do_leftbtn_up()`/`do_setcursor()` on `this` (the main window) whenever
   `!io.WantCaptureMouse`, and skipped that whole block otherwise. Once the debug window stopped using
   `NoInputs`, hovering it correctly makes ImGui set `io.WantCaptureMouse = true` (so it correctly stops
   routing clicks into the main window's content underneath), but nothing was forwarding those events
   into the debug window's own tree either — its content was just as inert as before, only for a
   different reason. Fixed by computing a `mouse_over_dbgwin` rect test against `dbgwin_->m_pos`/
   `m_size` (accessible directly since `CTadsWin` already declares `CHtmlSys_mainwin` a friend) and
   routing the uncaptured click/hover calls to `dbgwin_` instead of `this` when the mouse is inside it;
   the existing `getMouseCapture()`-based routing for in-progress drags (e.g. the debug window's own
   scrollbar, rendered as a real ImGui widget so it worked already) was untouched since it's already
   window-agnostic.

Verified end-to-end by launching `guit3.exe -debugwin tests/ditch3.t3`, simulating a title-bar drag
(window moved and its children/scrollbar/status-bar followed correctly), clicking its collapse triangle
(collapsed to just the title bar, then re-expanded at the same position on a second click), and typing a
game command into the main window while the debug window was open (main window input/output unaffected).
Not yet covered: keyboard-focus routing between the two windows (the debug log is presumably read-only
so this may not matter) and live resizing (deliberately left off, see above).

**Correction to the above: `-debugwin` still showed a real, blank second OS window — fixed.** Reported
as "the debug window opens, but it is displayed with a strange black border and no menu." The dragging/
click-routing fixes above were real and necessary, but incomplete: they fixed the *ImGui-rendered*
debug-log overlay, while a second, entirely separate problem meant a real native window was *also* on
screen at the same time, sitting on top of (and visually indistinguishable in front of) that overlay.

Root cause: `CTadsWin::create_system_window()` ([tadswin.cpp:513](tadswin.cpp#L513)) creates a real
Win32 `HWND` (`handle_`) unconditionally for every window, top-level or not — see §2's "both windows
open" writeup, which describes exactly this for the *main* window, but the fix that followed
(`setVisible()` hiding `handle_` for top-level windows) was accidentally scoped to only apply when the
window also owns a GLFW context (`m_window != 0`). That's true for the main window, but
`CHtmlSys_dbglogwin` is a *second* top-level window (`parent_ == nullptr`,
`guimain.cpp:552`); `CTadsSyswin::syswin_create_system_window()`'s GLFW overload
([tadswin.cpp:3617](tadswin.cpp#L3617)) deliberately refuses to create a second real GLFW window ("we
want only one real main window") and returns `nullptr`, so the debug window's `m_window` is always
`nullptr`. `setVisible()`'s `if (m_window)` check was therefore always false for it, falling through to
the plain-Win32-window `else` branch and calling `ShowWindow(handle_, SW_SHOW)` — putting a real, blank,
undecorated-by-content native window on screen, with no `WM_PAINT` ever reaching it (no message pump,
per §2) and no native menu rendering meaningfully either. That real window is exactly what "black border
and no menu" was describing, layered right on top of the correctly-working ImGui debug-log overlay
underneath it.

**Fix**: `CTadsWin::setVisible()` ([tadswin.cpp:2594](tadswin.cpp#L2594)) now branches on `parent_ ==
nullptr` (i.e. "is this a top-level window at all") rather than `m_window != 0` (i.e. "does this
top-level window happen to also own a real GLFW context"). Every top-level window's `handle_` now stays
permanently hidden; only the one that actually owns a GLFW window (`m_window`, the main window only)
gets `glfwShowWindow`/`glfwHideWindow` calls, same as before. A secondary top-level window like the debug
log has nothing to show/hide at that point at all — it's purely an ImGui overlay — and `isVisible()`
(which gates that overlay's rendering in `do_render()`) already reflects `m_visible` regardless of which
branch ran. Verified by enumerating the process's windows with `EnumWindows` before and after: before the
fix, `guit3.exe -debugwin` had **two** visible top-level windows (`GLFW30` plus a second, real
`TADS_Window`-class HWND positioned at the debug window's saved screen rect); after the fix, only
`GLFW30` is visible, and both `TADS_Window`-class HWNDs (main and debug) report `IsWindowVisible() ==
FALSE`.

**The debug window's own menu — ported.** `IDR_DEBUGWIN_MENU` (`win32/htmlcmn.rc`: File > Hide Window;
Edit > Copy, Select All) was previously flagged in §3.1 as one of the not-yet-ported native menus (its
`load_menu()`/`SetMenu()` call, [htmlgui.cpp](htmlgui.cpp), was exactly as dead as the main window's old
native menu, for the same no-message-pump reason). `CHtmlSys_dbglogwin::render_menu_bar()`
([htmlgui.cpp](htmlgui.cpp), declared in [htmlgui.h](htmlgui.h) next to `load_menu()`) now draws it as an
in-window ImGui menu bar, dispatching through `check_command()`/`do_command()` on `this` exactly like
`CHtmlSys_mainwin::render_menu_bar()` does (§3.1) — both command IDs (`ID_FILE_HIDE`, `ID_EDIT_COPY`,
`ID_EDIT_SELECTALL`) were already fully implemented in `CHtmlSys_dbglogwin::do_command()`/
`check_command()`, just never reachable from any UI before this.

Unlike the main window, the debug window isn't the main ImGui viewport, so it can't use
`BeginMainMenuBar()`/`BeginViewportSideBar()` (those are viewport-anchored). Instead,
`CHtmlSys_dbglogwin::do_render_content_begin()` (new override) adds `ImGuiWindowFlags_MenuBar` to the
base top-level `Begin()` call it otherwise duplicates from `CTadsWin::do_render_content_begin()`'s
parentless branch, then calls `render_menu_bar()` — which wraps a plain `ImGui::BeginMenuBar()`/
`EndMenuBar()` pair — immediately afterwards, before any child content renders. `ImGui::BeginMenuBar()`
must be called directly inside the `Begin()`/`End()` it belongs to (not from a nested child window), so
this has to happen in `do_render_content_begin()` itself rather than, say, a separate call from
`do_render()` the way the main window's `render_menu_bar()` is invoked.

**Verification note for a fresh session**: `Graphics.CopyFromScreen()` can silently capture the Windows
*lock screen* instead of the real desktop — it returned the same stock photo regardless of which window/
process/region was captured, which briefly looked like "screenshots don't work in this sandbox" until the
user pointed out the session had been locked the whole time. Unlocking fixed it immediately, no code
changes needed. If a screenshot recipe (§6) that worked before suddenly returns a suspiciously identical
image across unrelated windows, check for a locked session before concluding the capture path is broken;
`EnumWindows`/`IsWindowVisible` (a real Win32 API call, not a pixel capture) is a reasonable fallback for
yes/no visibility questions in the meantime, but it can't diagnose a rendering/layout bug the way an
actual screenshot can — see the very next entry, which needed real screenshots to crack.

**Second, separate `-debugwin` bug: the panel rendered with a solid, unpainted band across its bottom
edge — fixed.** Reported as "the black border is still there" *after* the two fixes just above had
already landed and been verified by `EnumWindows` (no more phantom OS window) and by an early screenshot
that only *looked* fine because it was cropped tighter than the actual gap. A later, full screenshot of
the debug window showed a solid ~38px-tall dark band spanning the full width, directly beneath the white
HTML content area and above the window's own bottom border — inside the window, not a second window
behind it, and unaffected by two different attempted fixes to `CHtmlSysWin_win32_dbglog`'s (the debug
log's HTML content panel, `get_html_panel()`) size before the real cause was found.

**Diagnosis.** Suspected first that the panel was simply undersized (its size otherwise comes from a
one-time `do_create()`-time snapshot of the now-permanently-hidden native `handle_`'s client rect — see
the previous fix — which predates the menu bar above it and has no reason to match the window's real
size). Fixing that (resizing the panel every frame via `calc_banner_layout()`, the same call
`CHtmlSys_mainwin::recalc_banner_layout()` makes on `main_panel_`, keyed off `ImGui::GetContentRegionAvail()`
computed *after* `render_menu_bar()`) changed nothing visible — the band was still there, same size, same
place. That ruled out sizing and pointed at *positioning*: instrumented the panel's `BeginChild()` call to
also paint its entire nominal rect solid magenta, and compared that against
`ImGui::GetWindowDrawList()->GetClipRectMin()/Max()` at the same point. The magenta rect and the visible
white area matched exactly (confirming the panel's *paintable* region was correct and un-clipped there) —
but the clip rect's top edge sat exactly one menu-bar-height *below* the panel's own nominal top edge,
while its bottom edge matched the panel's nominal bottom exactly. In other words, the panel's window was
being positioned one menu-bar-height too high, and only the portion of it that happened to still fall
inside its parent's actual content area (below the parent's own menu bar) was drawable at all — the
17px-ish invisible strip was the part of the panel ImGui had to clip away because it overlapped the parent
window's own menu bar chrome above it.

**Root cause**: `CTadsWin::do_render_content_begin()`'s `parent_ != nullptr` branch
([tadswin.cpp:1747](tadswin.cpp#L1747)) positions a child at `parent_pos + m_pos`, where `parent_pos` is
`ImGui::GetWindowPos()` for the *parent* window — its absolute top-left corner, above its own title bar
*and* menu bar, since (per the previous entry) the debug window draws its menu bar with a plain
`ImGuiWindowFlags_MenuBar` inside its own single `Begin()`/`End()`, not via a separate
`BeginMainMenuBar()` call the way the main window does. `calc_banner_layout()` (called from
`CHtmlSys_dbglogwin::do_render_content_begin()` per the sizing fix above) was seeding the panel's `m_pos`
at `(0, 0)` — correct only if `parent_pos` already excluded the menu bar, which for *this* window it
doesn't. The main window's own children don't hit this, because there the content window is positioned at
`viewport->WorkPos`, which the menu bar has *already* shrunk by the time it's read (see §3.1's
`BeginViewportSideBar()` gotcha) — `GetWindowPos()` there already means "below the menu bar." For the
debug window, menu bar and content share one `Begin()`, so `GetWindowPos()` means "above the menu bar,"
and `(0, 0)` was the wrong origin for anything meant to start below it.

**Fix**: [htmlgui.cpp](htmlgui.cpp), `CHtmlSys_dbglogwin::do_render_content_begin()` now anchors the
panel's rect at `ImGui::GetCursorPos()` (the window-relative cursor position, already correctly advanced
past the menu bar by `render_menu_bar()`'s `BeginMenuBar()`/`EndMenuBar()`) instead of `(0, 0)`, both for
the rect's origin and, added to `avail`, its far corner. Verified by screenshot: the white HTML panel now
fills the debug window's content area edge-to-edge, no dark band, in both a narrow crop and the full
window.

**Lesson for next time a "some of my child fills, some doesn't" symptom shows up**: don't jump straight to
a sizing fix. Confirm size *and* position independently — paint the child's full nominal rect a
throwaway solid color and diff it against `GetWindowDrawList()->GetClipRectMin()/Max()`; if the visible
paint and the clip rect agree with each other but disagree with the child's own claimed `Pos`/`Size`, the
bug is in *where* the child thinks it starts, not how big it is. This is the same
`parent_pos`/`m_pos` coordinate-space class of bug §6 already documents for the main window's children
(rendering pinned near screen `(0,0)` instead of following the parent) — worth checking first any time a
*non-main-viewport* top-level window (the only other one so far being the debug log) gets its own
in-window chrome that consumes space at the top, since `GetWindowPos()`'s meaning quietly depends on
whether that chrome was reserved at the viewport level (main window) or inside the same `Begin()` (debug
window).

**Startup `MessageBox` — fixed.** A native `MessageBox(..., "TADS", ...)` was observed popping up as a
real, separate Win32 dialog (`#32770` window class) on every startup. Traced it to
`MyClientIfc::display_error()` in [t3main.cpp:68](t3main.cpp#L68) — the VM's error-display callback,
which the T3 engine invokes synchronously (not from within `event_loop()`) whenever it wants to show the
player a message, most commonly the "no mapping file is available for the local character set" warning
raised at startup by `tads3/vmerrmsg.cpp` when no charmap file is found for the OS codepage. It routes
through `w32_msgbox()` in [guitr.cpp:181](guitr.cpp#L181), which called `MessageBox()` directly.

Replaced with `tadswin_message_box()`, a new function in
[tadswin.cpp](tadswin.cpp)/[tadswin.h](tadswin.h) that mimics `MessageBox()`'s signature and blocking
call contract (same `MB_OK`/`MB_OKCANCEL`/`MB_YESNO` input, same `IDOK`/`IDCANCEL`/`IDYES`/`IDNO` return
values) but renders an ImGui `BeginPopupModal` instead: it runs its own local
`glfwPollEvents`/`ImGui::NewFrame`/`ImGui::Render`/`glfwSwapBuffers` loop on the caller's `GLFWwindow*`,
blocking until a button is clicked, exactly mirroring how the native call blocked. `w32_msgbox()` now
looks up `CHtmlSys_mainwin::get_main_win()->get_glfw_window()` (a new public getter for `CTadsWin`'s
`m_window`, previously protected with no accessor) and passes it in; if there's no window yet (an error
before the main window exists), it falls back to a real `MessageBox()` so early failures still surface.
Verified by launching `guit3.exe`: the `#32770` dialog is gone, the warning now renders as a centered
dark ImGui popup titled "TADS" with the same message text and an OK button, and clicking OK dismisses it
and lets the game proceed normally (confirmed via screenshot before/after a simulated click).

**The warning itself is now also fixed at the source, so it no longer fires at all.** Its real trigger
(see the corrected note under "Running/verifying it visually" below) was that `guit3`'s
[CMakeLists.txt](CMakeLists.txt) never copied a `charmap` folder next to the built `guit3.exe`, unlike
`t3run`'s. Added the same `POST_BUILD` custom command `t3run` uses
([tads3/CMakeLists.txt:778](../../../tads-runner/tads3/CMakeLists.txt#L778)) to `guit3`'s
CMakeLists.txt, copying `tads3/charmap/cmaplib.t3r` into `$<TARGET_FILE_DIR:guit3>/charmap/` after every
build. Verified by rebuilding and launching from `tests\` (which always triggered the warning before):
the game now goes straight to its title screen with no popup at all.

**Scope note for next time:** this only converts `w32_msgbox()` (the VM's generic message-box hook,
`guitr.cpp:181`). The many other `MessageBox()` call sites — `htmlgui.cpp` (quit/save-overwrite
confirmations, `foldsel2.cpp`, `htmlpref.cpp`, `guifndlg.cpp` — see the `grep -i MessageBox` results —
are still native and were deliberately left alone. Most of those fire *from inside* `event_loop()`'s
per-frame handling (e.g. a menu command handler), where calling `tadswin_message_box()` would nest a
second `ImGui::NewFrame()`/`ImGui::Render()` inside the one already in progress and hit an ImGui assert;
porting those needs the caller's frame to finish first (return a "show this modal next frame" flag to
`event_loop()`) rather than a synchronous nested pump like `w32_msgbox()` uses. That's really the same
problem as the dialog framework in §3.3 — treat it as part of that effort, not a quick follow-up to this
fix.

## 3. Subsystem inventory — what's ported vs. what's still Win32

Ranked by Win32-API density (`HWND`/`WNDCLASS`/`CreateWindow*`/`#include <windows.h>` occurrence count)
to help prioritize:

| File | Win32 refs | Status |
|---|---:|---|
| `tadswin.h` / `tadswin.cpp` | 67 / 61 | Core window framework — **mixed**. Has the dual HWND/GLFW window creation described above, plus win32-only scrollbars, size-box, MDI frame/client support that's presumably obsolete for a single-window ImGui app. |
| `htmlgui.cpp` / `.h` | 44 / 38 | The big one — main display engine, banners, tooltips, popup menus, dialogs, the actual `event_loop()`. Rendering path is ported; menu/banner/tooltip child-window creation is not. |
| `htmlpref.cpp` / `.h` | 43 / 4 | Preferences — **not started**. Classic Win32 property-sheet dialog. |
| `tadsdlg.h` / `.cpp`, `tadsdlg2.cpp` | 35 / 28 / 19 | Generic dialog framework built on `CreateDialogParam`/`DialogBoxParam` — **not started**. Everything that uses `CTadsDialog` (most app dialogs) depends on this. |
| `tadswebctl.h` | 17 | COM/OLE ActiveX browser control embedding (`exdisp.h`) for the in-game Web UI feature. Legacy IE technology — **recommend dropping**, not porting, unless the Web UI feature is still required (see §4). |
| `tadsapp.cpp` / `.h` | 13 / 8 | App-level message routing, accelerators, modeless dialog list, MDI handling — tied to the Win32 message pump; needs redesign once there's no HWND to pump. |
| `foldsel.h` / `foldsel2.cpp` | 11 / 10 | Custom folder-picker dialog - **superseded**. The live call site (Options > Starting tab's Browse button) now uses the ImGui-native `CTadsFolderDialog` (`tadsfolderdlg.h`/`.cpp`); `foldsel.h`/`foldsel2.cpp` are left compiled but unused, same "harmless dead code" reasoning as elsewhere in this port. |
| `guifndlg.h` / `.cpp` | 6 / 10 | Find dialog (`CTadsDialogFind`) — **ported** (see §3.3's `CTadsFindDialog` entry); this file itself is left compiled but unused. Its `CTadsDialogFindReplace`/`CTadsDialogFindRegex` classes belong to the out-of-scope Workbench debugger and were never ported. |
| `tadscbtn.h`, `guiwebui.h`, `guisnd.h`, `tadsstat.h`, `htmlpref.h` | 6, 5, 4, 4, 4 | Custom button control, web-UI glue, status line, prefs header — **not started**. `guisnd.h` (sound glue) is **ported** for digitized audio — see §3.7. |

Everything else (tadswav/tadsvorb/tadsmidi/tadssnd/tadsreg/tadsimg/tadsjpeg/tadspng/tadscar/tadscsnd/
tadstab/tadscom) has only 1-3 stray references, mostly just `#include <windows.h>` or a type alias —
low `HWND` density but not necessarily low effort. The audio ones (`tadssnd`/`tadscsnd`/`tadswav`/
`tadsvorb`/`tadsmidi`) are **mostly ported** now — digitized-audio output goes through miniaudio and
the threading is `std::`-based; only the decoders' file reads and MIDI remain Win32 (§3.7).

### 3.1 Menu bar and toolbar — done
`CHtmlSys_mainwin::render_menu_bar()` ([htmlgui.cpp:11510](htmlgui.cpp#L11510), declared in
[htmlgui.h](htmlgui.h) next to `do_render()`) replaces the native menu with an ImGui one, called from
`do_render()` ([htmlgui.cpp:11464](htmlgui.cpp#L11464)) before `CTadsWin::do_render()` runs. It's
data-driven, hardcoded from `win32/htmlcmn.rc`'s `IDR_MAIN_MENU` resource and `htmlres.h`'s command
IDs — File/Edit/View (with Timer/Timer Format/Text Size submenus)/Themes/Go/Help, matching the
original structure. The old `LoadMenu`/`SetMenu` call in `do_create()`
([htmlgui.cpp:10806](htmlgui.cpp#L10806)) and the native `HMENU` it builds were left in place rather
than removed — they're harmless dead code (see below) and touching them risked breaking
`popup_container_`/`statusline_popup_`, which are separate context menus loaded from the same call
site and not part of this pass.

**Dispatch**: click handlers call `do_command(0, id, 0)` and per-frame enabled/checked state comes
from `check_command(&check_cmd_info(id))` — the exact same virtuals the native
`WM_COMMAND`/`WM_INITMENUPOPUP` handlers called, so no application logic was duplicated or rewritten,
just re-entered from a new call site. This was safe to do unconditionally because — confirmed by
grepping the whole `imgui/` tree — **there is no `GetMessage`/`DispatchMessage` pump anywhere in
`guit3`** (see §2), so `WM_COMMAND` never fired for the hidden top-level HWND and the native menu was
already 100% inert at runtime; `render_menu_bar()` is the *first* time these command IDs actually
become reachable from the UI in the ImGui build.

**Dynamic bits**, rebuilt from source data every frame instead of mutating a cached `HMENU`:
- **Recent games** (File menu): re-derives the same list `CHtmlSys_mainwin::set_recent_game_menu()`
  ([htmlgui.cpp:11594](htmlgui.cpp#L11594), still present but now effectively dead code, same
  reasoning as the `LoadMenu` call above) builds, reading `prefs_->get_recent_game_order()`/
  `get_recent_game()` directly; clicking an entry calls `do_command(0, ID_FILE_RECENT_1 + i, 0)`.
- **Themes submenu**: re-derives the profile list `CHtmlSys_mainwin::load_menu_with_profiles()`
  ([htmlgui.cpp:10893](htmlgui.cpp#L10893)) builds, by enumerating the same registry key
  (`CTadsRegistry::open_key`/`RegEnumKeyEx` on `<prefs>\Profiles`) directly; clicking a profile calls
  `prefs_->save()` + `set_game_specific_profile(name)` inline (bypassing the dead `profile_menu_`/
  `GetMenuItemInfo` round-trip `do_command()`'s native-menu code path used, since there's no `HMENU`
  backing the ImGui items to read the name back from).
- **Game Chest** (Go menu) is gated on the runtime check `CHtmlSys_mainwin::is_game_chest_present()`
  rather than the `.rc`'s compile-time `#ifdef HTMLTADS_GAME_CHEST`, matching how `htmlpref.cpp`
  already gates the same feature elsewhere.

**Gotcha — don't hand-reserve space for `ImGui::BeginMainMenuBar()`/`BeginViewportSideBar()`.** The
first implementation added a `menu_bar_height_` member and manually added it into
`CHtmlSys_mainwin::recalc_banner_layout()`'s `y_offset` (mirroring how the toolbar's height used to be
added there, back when the toolbar was still a native `HWND`). This produced a large empty gap under
the menu bar when tested. Root cause: `ImGui::BeginMainMenuBar()` internally calls
`BeginViewportSideBar()`, which adds the bar's height into `viewport->BuildWorkInsetMin.y`
([imgui_widgets.cpp:9109](imgui/imgui_widgets.cpp#L9109)) — this **automatically** shrinks
`GetMainViewport()->WorkPos`/`WorkSize` (on the next frame; `do_render()` already re-reads them every
frame, so in steady state this is transparent). `do_render()` feeds that already-shrunk work area
straight into `m_size` (via `do_resize()`) and into the outer content window's on-screen position (via
`SetNextWindowPos(viewport->WorkPos)`), so the menu bar's space is reserved automatically, once, at the
outer-window level — adding it again in `recalc_banner_layout()` double-reserves it. The fix was to
remove the manual addition entirely and leave a comment explaining why
([htmlgui.cpp:12469](htmlgui.cpp#L12469)-ish, drifts). Once this was understood, `render_toolbar()`
(also §3.1, done in a later session) deliberately called `ImGui::BeginViewportSideBar()` directly for
the same reason, stacking on top of the menu bar's reservation with zero manual layout code needed for
it either. The status bar (§3.2) is the one exception, and remains one on purpose: it draws via
`GetForegroundDrawList()` rather than any `Begin()`-family call, so it never participates in the
viewport work-inset system and still needs its manual height subtracted in `recalc_banner_layout()`.
**The rule going forward: anything built from `BeginViewportSideBar()`-based ImGui APIs
(`BeginMainMenuBar()`, `BeginViewportSideBar()` directly, and if ever used,
`DockSpaceOverViewport()`'s side panels) already reserves its own space; anything drawn via the
foreground draw list or a native `HWND` still needs manual reservation.**

**Gotcha — this ImGui build does not parse `&` mnemonics.** `win32/htmlcmn.rc` uses Windows'
`&Letter` convention throughout (`&File`, `&Open New Game...`, etc.); grepping `imgui/imgui/*.cpp` for
"Mnemonic" turns up nothing, so an unstripped `&` renders as a literal ampersand in the menu (visually
confirmed: `&Add/Delete Themes...` showed up with the `&` on screen). Static hardcoded labels in
`render_menu_bar()` were written without `&` in the first place; the three dynamic Themes-menu labels
that come from `LoadString()` resource strings (`IDS_MANAGE_PROFILES`, `IDS_SET_DEF_PROFILE`,
`IDS_CUSTOMIZE_THEME`, which still have `&` baked in since the resource strings are shared with the
native build) go through a small `strip_mnemonic()` lambda inside `render_menu_bar()` first
(`&&` → `&`, lone `&` dropped). Watch for this again if any future ImGui text pulls from a
`LoadString()`/`.rc` string table.

**Keyboard shortcuts are display-only for now.** `ImGui::MenuItem()`'s shortcut-text parameter
(`"Ctrl+O"` etc., hardcoded to match the `.rc`) is cosmetic — it doesn't bind an actual key handler.
The native accelerator-table dispatch (`CTadsAccelerator`, `tadsapp.cpp`) is equally dead code today
(only reachable from the same unreached `CTadsApp::process_message()` pump), so wiring up real
keyboard shortcuts is a related but separate follow-up: it needs a new dispatcher hooked into
`event_loop()`'s existing `ImGui::IsKeyPressed()`-based input handling that calls `do_command()`
directly, independent of this menu-bar rendering change.

**Not ported in this pass, left for later**: `iconmenu.cpp`'s owner-drawn menu icons (also dead code
today for the same "no `WM_MEASUREITEM`/`WM_DRAWITEM`" reason — the toolbar's GL-texture icon loading,
below, is the pattern to follow if menu icons are wanted later, rather than reviving this Win32
owner-draw path). The game-text right-click context/edit popup menu (from `IDR_EDIT_POPUP_MENU`) is now
ported — see §3.1a below. The debug-log window's `IDR_DEBUGWIN_MENU` is now ported too — see the
"`-debugwin` still showed a real, blank second OS window" entry further down. The status bar's right-click
popup (`IDR_STATUSBAR_POPUP`, `statusline_popup_`) is now ported too — see §3.2. The debug-log window's own
right-click popup (`IDR_DEBUGLOG_POPUP`, loaded via the same `load_context_popup()`) is the one exception
left genuinely unported, and it's a deliberate non-issue rather than a gap: `CHtmlSysWin_win32_dbglog`'s
constructor only calls `load_context_popup(IDR_DEBUGLOG_POPUP)` when `debugger_ifc_ != 0`
([htmlgui.cpp](htmlgui.cpp), `CHtmlSysWin_win32_dbglog::CHtmlSysWin_win32_dbglog`), and — per the
"synchronous virtual interface had to become callback-based" entry in §3.3 — `guimain.cpp`'s only call site
in `guit3` always constructs the debug window with a null `debugger_ifc_` (that interface belongs to the
out-of-scope Workbench debugger/editor, see the "MDI" decision in §4). So this popup is unreachable in the
client build regardless of porting effort; porting it would be dead code from the moment it landed.

**Toolbar**: `CHtmlSys_mainwin::render_toolbar()` (`htmlgui.cpp`, defined right after
`load_toolbar_texture()`, called from `do_render()` right after `render_menu_bar()`) replaces the
native `CreateToolbarEx()` control built (but, same as the pre-port menu, never actually shown - see
`create_toolbar()`, `htmlgui.cpp:11238`) with an ImGui icon-button row. It's data-driven from the same
15-button/5-separator list `create_toolbar()` built by hand into a `TBBUTTON[]` array, and dispatches
through `check_command()`/`do_command()` exactly like the menu bar - so, again, no application logic
was duplicated, just re-entered from a new call site.

- **Icons**: the toolbar's icon strip is a real asset, `win32/runtbar.bmp` (compiled in as the
  `IDB_TERP_TOOLBAR` bitmap resource, 304x15px, 4bpp indexed, nineteen 16x15 icon frames), loaded once
  by `load_toolbar_texture()` into a single GL texture atlas: `LoadImage(..., LR_CREATEDIBSECTION)` +
  `GetDIBits()` to expand the indexed bitmap to 32bpp, then a manual BGRA→RGBA conversion that also
  turns the bitmap's top-left-pixel color-key (the same transparency convention the native `ImageList`
  used via `ImageList_AddMasked()` in `iconmenu.cpp`) into a real alpha channel, since GL textures have
  no color-key-mask equivalent. `glTexImage2D` uploads it once; each button then samples a `1/19`-wide
  UV slice of the atlas via `ImGui::ImageButton()`. This is the **first GL-texture-loading code in the
  port that isn't part of the JPEG/PNG banner-image pipeline** (`tadsjpeg.cpp`'s
  `glGenTextures`/`glTexImage2D` call was the only precedent, and it assumes a decoded photo, not an
  indexed bitmap resource with color-key transparency) - worth reusing/generalizing if another Win32
  bitmap resource ever needs the same treatment (e.g. reviving menu icons, per above).
- **Filtering**: `GL_NEAREST`, not `GL_LINEAR` - the icons are packed edge-to-edge in the atlas with no
  padding, and linear filtering visibly bled each icon's edge pixels into its neighbor's when tried
  first. Worth remembering for any future small, densely-packed icon atlas in this codebase.
- **Layout**: like the menu bar, the toolbar calls `ImGui::BeginViewportSideBar()` directly (the same
  internal function `BeginMainMenuBar()` calls) rather than a plain `ImGui::Begin()`, so it stacks its
  own space reservation on top of the menu bar's in the same automatic viewport work-area system - no
  manual `y_offset` bookkeeping needed. This *replaced* manual bookkeeping that used to be live and
  correct: `recalc_banner_layout()`'s old `IsWindowVisible(toolbar_)`/`GetWindowRect(toolbar_, ...)`
  block read the native (never-painted, but still real and correctly laid out) toolbar `HWND`'s
  geometry - removed now that an ImGui toolbar exists to reserve the space itself.
- **The `ID_THEMES_DROPDOWN` split button** (a themes/profile picker, distinct from the ordinary
  buttons) doesn't call `do_command()` on click at all, matching the native `TBN_DROPDOWN` handler
  (`htmlgui.cpp:12437`-ish, drifts) it replaces: it opens an `ImGui::BeginPopup()` showing the profile
  list. That content is shared with the menu bar's Themes submenu via a new
  `CHtmlSys_mainwin::render_themes_menu_items()` helper (extracted out of `render_menu_bar()`, which
  now just wraps it in `BeginMenu("Themes")`/`EndMenu()`) - the native code showed literally identical
  content in both places, since both were built from the same `load_menu_with_profiles()` call.
- **Tooltips**: `ImGui::SetItemTooltip()` per button, text hardcoded from the `IDS_*` string-table
  values the native `TTN_NEEDTEXT` handler (`htmlgui.cpp:12337`-ish, drifts) used to look up (also dead
  code today, same no-message-pump reason as everything else `WM_*`-based) - shown regardless of the
  button's enabled state, matching that handler, which never checked enabled state either.
- **Not carried over**: `IsWindowVisible(toolbar_)` is gone from the toolbar's own visibility check too
  - `render_toolbar()` reads `prefs_->get_toolbar_vis()` directly instead, which is what actually
  mattered (see the menu bar's already-existing `View > Show Toolbar` checkbox, and
  `do_command()`'s `ID_VIEW_TOOLBAR` case, both unchanged and still the source of truth for this flag).
  The native `toolbar_` `HWND` and `create_toolbar()` were left in place, same reasoning as the
  never-removed native menu.
- **Chrome color**: `render_menu_bar()` and `render_toolbar()` each `PushStyleColor`/`PopStyleColor`
  around their `BeginMainMenuBar()`/`BeginViewportSideBar()` call (`ImGuiCol_WindowBg`, plus
  `ImGuiCol_MenuBarBg` for the menu bar) to give both the status bar's own light grey
  (`IM_COL32(212, 212, 212, 255)`, §3.2) rather than letting them pick up the active ImGui theme's
  window background, which looked inconsistent against the always-grey status bar underneath them.
  The app's base ImGui style is `StyleColorsDark()` (`do_create()`, `htmlgui.cpp:10405`-ish), whose
  default (light) text doesn't read well against that light chrome, so `render_menu_bar()` has a
  `menu(label)` helper (wraps `ImGui::BeginMenu()`) that pushes `ImGuiCol_Text` black just for
  rendering each top-level label ("File", "Edit", etc. - the six calls that sit directly on the menu
  bar's light background) and pops it again immediately after the call, before any of that menu's
  dropdown items are drawn. The dropdown popups themselves (and nested submenus like View > Timer)
  render on the theme's normal dark popup background, so they're left alone to use the theme's default
  light text - only wrapping the label itself, rather than pushing black for the whole
  `BeginMenu()`/`EndMenu()` block, is what keeps those readable too. Tried an all-over darker chrome
  grey (`60, 60, 60`) and, before that, an all-over `StyleColorsLight()` switch as alternatives; this
  targeted per-label push is what stuck since it keeps the dark theme everywhere else.
- **Toolbar vertical centering**: the toolbar window's `height` used to be
  `TOOLBAR_ICON_HEIGHT + style.FramePadding.y * 2 + 6`, an arbitrary `+ 6` fudge that didn't account
  for `ImGuiViewportSideBar`'s actual top/bottom `WindowPadding.y` inset. Since ImGui only applies
  padding above the button row, not below, the window was too short and the icon buttons overflowed
  the bottom edge instead of sitting centered. Fixed by computing `height` as
  `button_height + style.WindowPadding.y * 2` (symmetric padding top and bottom), with the vertical
  divider lines between button groups redrawn to span exactly `button_height` (previously hardcoded
  `p.y + 2` to `p.y + height - 10`, which no longer lined up once `height`'s formula changed).
- **Toolbar button color**: `ImGui::ImageButton()` always paints its frame with the regular Button
  colors, even at rest and not just on hover/press (see `ImageButtonEx()` in `imgui_widgets.cpp`),
  which in the app's dark theme is a semi-transparent blue - it clashed against the toolbar's grey
  chrome (above). `render_toolbar()` now pushes `ImGuiCol_Button` transparent (so idle icons show no
  box at all) and `ImGuiCol_ButtonHovered`/`ImGuiCol_ButtonActive` to plain greys instead of the
  theme's blue, so hovering/pressing a toolbar icon darkens it slightly without introducing a color
  that doesn't belong to this chrome.

### 3.1a Right-click edit context menu — done

The game-text window's right-click popup (`Undo Typing` / `Cut` / `Copy` / `Paste` / `Delete` /
`Select All` / `Select Command` / `Find Text...` / `Find Next` — same set and order as
`win32/htmlcmn.rc`'s `IDR_EDIT_POPUP_MENU`) is now an ImGui popup,
`CHtmlSys_mainwin::render_context_menu()` (`htmlgui.cpp`, declared in `htmlgui.h` next to
`render_menu_bar()`), called every frame from `CHtmlSys_mainwin::do_render()` right after
`prefs_->render_options_dialog()`. Same "dispatch through the existing virtuals, don't duplicate
application logic" approach as §3.1: items call `check_command()`/`do_command()` on `this`
(`CHtmlSys_mainwin`), exactly like `render_menu_bar()`'s Edit menu — which is what makes this work
regardless of which banner subwindow was actually right-clicked, since `CHtmlSys_mainwin::do_command()`/
`check_command()` already forward `ID_EDIT_CUT/COPY/DELETE/SELECTALL` to `get_focus_subwin()` and
everything else to `main_panel_`/`hist_panel_`, and the click itself moves focus to the right subwindow
before the popup opens (see below). `CHtmlSysWin_win32::do_rightbtn_up()`'s old
`track_context_menu(edit_popup_, x, y)` call (native `TrackPopupMenu()`) is now
`ImGui::OpenPopup("EditContextMenu")`; `load_context_popup()`/`popup_container_`/`edit_popup_`
themselves were left in place as harmless dead code, same reasoning as §3.1's native `HMENU`.

**Gotcha — `ImGui::OpenPopup()` and `ImGui::BeginPopup()` must be called from matching ID-stack
depths for the same string ID to refer to the same popup**, since ImGui hashes string IDs against
whatever window/child ID is currently on the stack. `do_rightbtn_up()` fires from
`event_loop()`'s manual mouse-routing block, which runs right after `ImGui::NewFrame()` with nothing
pushed yet (root level) - so `render_context_menu()` has to run at that same root level too. It does,
because it's called from `CHtmlSys_mainwin::do_render()` *after* `CTadsWin::do_render()` returns
(which already closed its own `Begin()`/`BeginChild()`) - the same spot `render_options_dialog()` uses
for the same reason. Calling `BeginPopup()` from inside a per-window `do_render_content_begin()`/`_end()`
pair (nested inside that window's own `BeginChild()`) would silently never match the root-level
`OpenPopup()` call and the menu would never appear - worth remembering for the still-unported debug-log
and status-bar popups (previous section), since it's tempting to open/draw those from wherever their
own right-click handler happens to live instead.

**`CTadsWin::do_rightbtn_down()` needed the same recursive child-dispatch `do_leftbtn_down()` already
had** (`tadswin.cpp`) - the base class's version was a `{ return FALSE; }` stub (`tadswin.h`), so a
right-click on `CHtmlSys_mainwin` (the `uncaptured_target` `event_loop()` calls into) never reached the
actual leaf banner/text window's overridden `do_rightbtn_down()`. Added an out-of-line
`CTadsWin::do_rightbtn_down()` that loops over `m_children` exactly like `do_leftbtn_down()` does.
`do_rightbtn_up()` didn't need the same fix: mouse capture (set by `do_leftbtn_down()`, which
`do_rightbtn_down()`'s existing override already calls) already identifies the right window by the time
button-up fires, so `event_loop()`'s existing "dispatch to the captured window" logic (mirrored from the
left-button case) finds it without any recursion.

**Verified** by launching `guit3.exe tests/ditch3.t3`, right-clicking the game text area, and
screenshotting (per the recipe in §6): the popup appears positioned at the click point with the correct
item set/order and correct enabled/disabled state (Cut/Copy/Delete/Undo greyed out with no selection,
Paste/Find enabled); clicking "Find Text..." dispatched through to `do_command(ID_EDIT_FIND)` and opened
the (still-native) Find dialog, confirming the click-to-command path works end to end.

### 3.2 Status bar — done
`CTadsStatusline` ([tadsstat.h](tadsstat.h)/[tadsstat.cpp](tadsstat.cpp)) no longer owns a native
status-bar control. `CTadsStatusSource`/`CTadsStatusPart` (the "ask each registered source for a
message" protocol) were left unchanged, as planned — only the rendering backend was swapped out.

What changed:
- `CTadsStatusline` now holds its own layout/text state (`part_edges_`/`part_texts_`, mirroring the old
  control's `SB_SETPARTS`/`SB_SETTEXT` model) instead of an `HWND`. `set_parts()`/`set_part_text()`
  replace the raw `SendMessage(handle_, SB_SETPARTS/SB_SETTEXT, ...)` calls that existed both inside
  the class (`update()`) and at two external call sites that poked the status control directly:
  `CHtmlSys_mainwin::adjust_statusbar_layout()` (the optional elapsed-time-timer column,
  [htmlgui.cpp:12521](htmlgui.cpp#L12521)) and `CHtmlSys_mainwin::do_timer()` (the timer's actual text,
  [htmlgui.cpp:11178](htmlgui.cpp#L11178)).
- `notify_parent_resize()` keeps its original fixed/proportional-width layout algorithm (for
  `CTadsStatusPart::calc_width()`-driven multi-part layouts via `add_part()`), just writing into
  `part_edges_` via `set_parts()` instead of `SendMessage`; it now takes the new width as a parameter
  instead of reading it via `GetClientRect` on a control handle.
- `CTadsStatusline::render(x, y, width)` draws the bar for the current ImGui frame: a bottom-anchored,
  undecorated ImGui window with a top border line, one clipped region per part (so long text can't spill
  into the neighboring part), and a vertical separator between parts — deliberately mimicking the look
  of the original Win32 status bar. It's called once per frame from
  `CHtmlSys_mainwin::do_render()` ([htmlgui.cpp:11465](htmlgui.cpp#L11465)), anchored to the bottom of
  `ImGui::GetMainViewport()`'s work area.
- `get_height()` replaces the `GetClientRect(statusline_->get_handle(), &statrc)` call that
  `CHtmlSys_mainwin::recalc_banner_layout()` ([htmlgui.cpp:12449](htmlgui.cpp#L12449)) already used to
  reserve space for the status bar above it — so the main text panel/banners correctly leave room for
  the bar without needing any new layout wiring, they just needed the height from a different source.
- `owner_draw()`/`WM_DRAWITEM` handling was dropped — it turned out to be dead code with zero callers
  even in the original Win32 version (nothing in the app ever creates an owner-drawn status part), so
  there was nothing to port.
- `get_handle()` is kept (returning `0`/null) purely so `guiwebui.h` — the not-yet-ported, phase-two
  Web UI code (§4) that still calls `GetClientRect(statusline_->get_handle(), ...)` for its own layout —
  keeps compiling. It's never actually invoked at runtime today since nothing instantiates the Web UI
  control yet.

Also removed while verifying this: the leftover `ImGui::ShowDemoWindow()` call in the main event loop
([htmlgui.cpp:14437](htmlgui.cpp#L14437)) — it was overlapping and obscuring the status bar (and would
have obscured the menu bar next), so it's gone now rather than deferred.

Verified by screenshot: the bar renders as a full-width strip at the bottom with a visible separator
above it (main content correctly stops short of it), and the elapsed-time part
(`prefs_->get_show_timer()`) updates live and right-aligns correctly next to the (currently empty)
main message part.

The bar background is a fixed light grey (`IM_COL32(212,212,212,255)`, classic Win32 status-bar
color) with black text, independent of the app's overall dark ImGui theme, matching how a native
status bar always looked regardless of the rest of the UI. It's drawn via
`ImGui::GetForegroundDrawList()` (`AddRectFilled`/`AddLine`/`AddText`) rather than as a normal
`ImGui::Begin()` window: an earlier attempt using `ImGui::PushStyleColor(ImGuiCol_WindowBg, ...)`
inside an ordinary window compiled fine but never actually showed the grey — something else in this
large, partially-ported window tree draws over a normal window's background most frames. Drawing
directly into the always-on-top foreground list sidesteps window z-order and any stray style
push/pop imbalance elsewhere entirely, which seems like the more robust default for fixed chrome
like this (and likely the pattern to reach for again for the menu bar next, if it runs into the same
issue).

**Right-click context menu — done.** The status bar's own right-click popup (`IDR_STATUSBAR_POPUP`:
Pause Timer/Reset Timer/Show Timer/Timer Format submenu) was the one piece of §3.1's "not ported in this
pass" list that was still actually reachable in `guit3` (unlike the debug-log window's equivalent, see
§3.1's writeup) - the bar itself was ImGui-native from the start of this section, but nothing ever routed
a right-click on it anywhere. `CHtmlSys_mainwin::render_statusbar_context_menu()`
([htmlgui.cpp](htmlgui.cpp), declared in [htmlgui.h](htmlgui.h) next to `render_context_menu()`) draws it
as an `ImGui::BeginPopup()`, dispatched through `check_command()`/`do_command()` exactly like
`render_menu_bar()`'s View > Timer submenu - which shows identical content, since both come from the same
underlying preferences/timer state.

**Claiming the click needed two overrides, not one, because of an ordering trap.** The natural first
attempt - override just `CHtmlSys_mainwin::do_rightbtn_up()` to rect-test the click and call
`ImGui::OpenPopup()`, mirroring how `CHtmlSysWin_win32::do_rightbtn_up()` opens `"EditContextMenu"` for the
game text window - compiled fine but never fired: a right-click squarely inside the status bar's own band
still opened the *game text's* context menu instead. Root cause: `do_rightbtn_up()` only runs (per
`event_loop()`'s mouse routing) when no window currently has mouse capture; but capture gets set on
button-*down*, and `CTadsWin::do_rightbtn_down()`'s recursive child dispatch (added for §3.1a) reaches
`main_panel_`/`hist_panel_` (via `CHtmlSysWin_win32::do_leftbtn_down()`, which `do_rightbtn_down()`
forwards to) regardless of where in the window the click landed - that leaf handler doesn't bounds-check
the click against its own rect (it only checks the vertical-scrollbar strip), so it unconditionally claims
capture for *any* right-click anywhere in the window, including over the status bar sitting below it. By
the time button-up fires, `event_loop()` routes it to the captured child, never to `CHtmlSys_mainwin`'s own
`do_rightbtn_up()` override at all. **Fix**: added a matching `do_rightbtn_down()` override that rect-tests
first and returns `TRUE` *without* recursing into `CTadsWin::do_rightbtn_down()`'s child dispatch when the
click is over the status bar - so no child ever gets a chance to claim capture for it, and the existing
`do_rightbtn_up()` override then correctly receives the uncaptured button-up. Both overrides share the rect
test via a new `CHtmlSys_mainwin::over_statusbar(x, y)` helper. Worth remembering for any other
chrome-that-isn't-a-`CTadsWin`-child needing its own click handling: claim it on button-*down*, not just
button-up, or a bounds-unchecked leaf window elsewhere in the tree will silently steal it first.

**Second bug, found only by actually looking at a screenshot: the popup's bottom rows never appeared,
regardless of their content.** After the capture fix above, right-clicking the status bar correctly opened
*a* popup, but it only ever showed the first ~4 text rows (e.g. "Pause Timer"/"Reset Timer"/"Show Timer"
plus one more) before cutting off - the "Timer Format" submenu row was simply missing. Swapping which
command occupied the last visible slot (confirmed via a temporary debug build) showed the *content* didn't
matter - whatever landed in that row-position vanished, which ruled out anything specific to
`ImGui::BeginMenu()` or to any one command ID. Also ruled out a leaked `ImGuiWindowSizeConstraint` (a real
prior-art bug class in this codebase, e.g. from an unclosed `BeginCombo()`) by dumping
`g.NextWindowData.HasFlags` as on-screen text right before `BeginPopup()` - it read `0`, clean. **Actual
root cause**: the status bar draws via `ImGui::GetForegroundDrawList()` (see above), which - being the
foreground list - always composites on top of *every* ordinary window/popup at the end of the frame,
regardless of the order draw calls were issued in during that frame. The popup naturally opens anchored at
the click point, which is *inside* the status bar's own rect by construction (that's where the user
right-clicked it) - so however the popup grew from there, its rows nearest the status bar ended up
geometrically underneath the bar's opaque background, which then painted over them every single frame.
Not a sizing or content bug at all - the rows were rendering correctly, just getting silently covered up a
moment later in the same frame. **Fix**: `render_statusbar_context_menu()` now calls
`ImGui::SetNextWindowPos()` with pivot `(0, 1)` (bottom-left) anchored at `(mouse_x, status_bar_top)` before
`BeginPopup()`, so the whole popup is pinned to grow *upward* from just above the bar instead of outward
from the click point - guaranteeing no overlap with the always-on-top band underneath it. **Worth reusing
this diagnostic sequence for any future "some rows/content mysteriously don't appear" report near a
foreground-draw-list element (currently just the status bar, but potentially the caret or scrollbar too,
see §4 in this file)**: don't assume it's a sizing/constraint bug just because it looks like one - check
whether the missing content's *screen position* overlaps something drawn on the foreground list, since that
list always wins the paint order regardless of submission order.

**Verified** by screenshot and simulated clicks (`SetCursorPos`/`mouse_event`, per the recipe in §6):
right-clicking the status bar shows all four rows plus the "Timer Format" flyout (hovering it opens
"Hours:Minutes:Seconds"/"Hours:Minutes" with the correct one checked); clicking "Pause Timer" dispatches
correctly and closes the popup cleanly, leaving the main window's command line focused and responsive; a
follow-up right-click on the game text area still opens the unrelated `"EditContextMenu"` correctly (no
regression from the new `do_rightbtn_down()` override intercepting clicks it shouldn't).

### 3.2a Banner subwindow clicks (room-exit links etc.) were swallowed by `main_panel_`

**Symptom** (user report): clicking the `west` hyperlink in a room/status banner - e.g. `ditch3.t3`'s
"Control Room / Exits: **west**" banner at the top of the window - did nothing. The same click as a
typed `west` command worked fine.

**Root cause**: the exact bounds-check gap already called out in §3.2's right-click writeup ("that leaf
handler doesn't bounds-check the click against its own rect ... so it unconditionally claims capture for
*any* click anywhere in the window"), but for plain left-clicks and therefore never worked around.
`event_loop()`'s manual mouse routing calls `do_leftbtn_down()` on `CHtmlSys_mainwin`, which flows into
`CTadsWin::do_leftbtn_down()`'s recursive child dispatch: it offers the same absolute click to every
visible subwindow in child-creation order, stopping at the first that returns `TRUE`. `main_panel_` is
created before any game-created banner, and `CHtmlSysWin_win32::do_leftbtn_down()` returns `TRUE` (and
grabs mouse capture) for essentially every click it's handed - it only ever declined the
vertical-scrollbar strip (`x >= rc.right`). So `main_panel_` consumed the click before the banner
subwindow behind it in the list was ever asked, and the banner's link hit-test never ran.

**Fix**: added `CHtmlSysWin_win32::pt_in_screen_rect(x, y)` ([htmlgui.h](htmlgui.h), right after the
mouse-event-handler declarations) - a plain geometric test of the incoming absolute coordinate against
this subwindow's own `get_screen_pos()`/`m_size` rect, the same technique `CHtmlSys_mainwin::over_statusbar()`
uses. `CHtmlSysWin_win32::do_leftbtn_down()` and `do_setcursor(int, int)` now return `FALSE` up front
when the point isn't inside them, so the recursive dispatch continues to the sibling that actually
contains it. `do_rightbtn_down()` forwards to `do_leftbtn_down()` so it's covered too;
`do_mousemove()`/`do_leftbtn_up()` were left alone because `event_loop()` dispatches those straight to
the capture-holding window (a text-selection drag that leaves the subwindow must keep receiving them),
never through this recursive search.

**Verified** by screenshot + simulated clicks (§6 recipe): launched `guit3.exe tests/ditch3.t3`, clicked
"begin the game", then clicked `west` in the "Exits:" banner - the command line echoed `>west` and the
game replied "You simply can't leave until you get the SCU-1100DX working" (turn counter ticked
0/0 -> 0/1). The "begin the game" click in `main_panel_` itself still worked in the same run, confirming
no regression to ordinary in-panel link clicks.

### 3.3 Dialogs (preferences, find/replace, folder picker, generic app dialogs)
All dialog infrastructure (`tadsdlg.cpp`/`tadsdlg2.cpp`, `htmlpref.cpp`, `foldsel2.cpp`,
`guifndlg.cpp`) is built on `CreateDialogParam`/`DialogBoxParam` with resource-defined (`.rc`) layouts,
`WM_INITDIALOG`/`WM_COMMAND` handlers, and native controls (buttons, tree views, tab controls via
`tadsdlg2.cpp`'s `CreateWindow("SysTreeView32", ...)`).

**The "Options" dialog (Edit > Options, `run_preferences_dlg()`'s native property sheet) is now
ImGui-native - done.** Rather than building the general `CTadsDialog`-mirroring base class this section
used to recommend, this went with a more direct approach that turned out not to need one:
`CHtmlPreferences::open_options_dialog()`/`render_options_dialog()` (new, in
[htmlpref.cpp](htmlpref.cpp)/[htmlpref.h](htmlpref.h)) draw the whole dialog - all 7-8 tabs
(Appearance/Keyboard/File Safety/Network Safety/Memory/Starting/Quitting, plus Game Chest when
`is_game_chest_present()`) - as one `ImGui::BeginPopupModal()` with a `BeginTabBar()`, called from
`CHtmlSys_mainwin::do_render()` every frame. The `ID_VIEW_OPTIONS` menu handler
([htmlgui.cpp](htmlgui.cpp)) now calls `open_options_dialog()` instead of the old (blocking)
`run_preferences_dlg()`; the old property-sheet code and all eight `CHtmlDialog*PropPage` classes are
left in place unused, same "harmless dead code" reasoning as the never-removed native menu elsewhere in
this port.

**No "Apply" staging step.** The original pages worked in two phases - edit controls, then commit
everything on `PSN_APPLY` via each page's `has_changes(save)` method. The ImGui version drops that
entirely: every control writes straight through to the corresponding `CHtmlPreferences` setter the
instant it changes (radio buttons on click, text fields via `InputText()`'s changed-this-frame return
value), exactly like the rest of the already-ported ImGui chrome (menu bar, toolbar) reads/writes
`prefs_` live. This matches the *original* app's own persistence model, not just a simplification: the
per-field setters (`set_emacs_ctrl_v()` etc.) only ever mutate the in-memory property list -
`CHtmlPreferences::save()` (the actual registry write) is called from very few places, mainly
`CHtmlSys_mainwin`'s destructor (app exit) and on a profile switch - so the native dialog's "Apply"
button never persisted anything to disk either; it just committed the dialog's pending edits into the
same in-memory object the ImGui version now writes to directly.

**"Customize Theme..." (the Fonts/Colors/More/Media property sheet) is now ImGui-native too - done.**
Same approach and same file as Options:
`CHtmlPreferences::open_customize_theme_dialog()`/`render_customize_theme_dialog()`
([htmlpref.cpp](htmlpref.cpp)/[htmlpref.h](htmlpref.h)) draw all four former property pages
(Font/Colors/More/Media) as one `ImGui::BeginPopupModal()` with a `BeginTabBar()`, called from
`CHtmlSys_mainwin::do_render()` right after `render_options_dialog()`. Both call sites that used to
invoke the blocking `run_appearance_dlg()` - the Options dialog's "Customize Theme..." button and the
`ID_APPEARANCE_OPTIONS`/`ID_THEMES_DROPDOWN` menu handlers in [htmlgui.cpp](htmlgui.cpp) - now call
`open_customize_theme_dialog()` instead; `run_appearance_dlg()` and the four
`CHtmlDialog{Fonts,Color,More,Media}` property-page classes are left in place unused, same "harmless
dead code" reasoning as elsewhere in this port. Same no-Apply-step, immediate-write convention as
Options too.

The one genuinely new piece of work here (nothing in the Options dialog needed it) was the **Font
tab's font-name enumeration**: the native version's `CTadsDialog::init_font_popup()` enumerates via
`EnumFontFamiliesEx()` straight into a combo box `HWND`'s item list, which doesn't exist in the ImGui
world. The replacement, `CHtmlPreferences::cust_refresh_font_lists()`, enumerates into plain fixed-size
name arrays (`cust_fonts_all_`/`_fixed_`/`_serif_`/`_sans_`/`_script_`/`_typewriter_`, each capped at
`CUST_MAX_FONTS` entries) once when the dialog opens, de-duplicating names as it goes (a single face
gets reported once per style/script by `EnumFontFamiliesEx`). The family-selector callbacks
(`cust_font_select_serif/sans/script/typewriter`) are verbatim ports of the original
`CHtmlDialogFonts::font_select_*` filtering logic (checking `lfPitchAndFamily` bits), just retargeted at
the plain-array enumeration instead of a combo. `CUST_MAX_FONTS`/`CUST_FONT_NAME_LEN` had to move to the
class's `public:` section (unlike the rest of the `cust_`-prefixed dialog state, which is private)
because the file-scope `EnumFontFamiliesEx` callback needs them and isn't a class member.

Color swatches use `ImGui::ColorEdit3(..., ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs)`
- `NoInputs` matters here: without it the swatch expands into inline R/G/B text fields that get crushed
against whatever's next on the same line (found by actually looking at a screenshot, not just reading
the code - the input-color swatch next to the command-font size combo rendered as a squeezed, unreadable
"0" field before this flag was added). `HTML_color_t`/`ImVec4` conversion reuses the existing
`HTML_color_to_ImVec4()`/`ImVec4_to_HTML_color()` helpers in [tadswin.h](tadswin.h) (already used
elsewhere for input/selection coloring), so no new conversion code was needed there.

The More tab's radio-button labels are the other lesson worth recording: `ImGui::RadioButton()` labels
do **not** wrap, so the native dialog's full one-sentence labels ("Show a MORE prompt in the game
window, and halt scrolling until you press a key") rendered past the popup's right edge and bled into
the main window behind it. Fix was to shorten each label to a short phrase and move the rest of the
sentence to an indented `ImGui::TextWrapped()` line below the radio button - not a one-off workaround,
worth remembering for any future dialog port that carries long native radio/checkbox label text.

One sub-flow reachable from the Options dialog remains Win32, deliberately: the Starting tab's
folder-browse button (`CTadsDialogFolderSel2::run_select_folder()`) - see the "Not yet ported" note
below. The Game Chest tab's two file-browse buttons, which used to call `GetOpenFileName()`, are now
ImGui-native too - see the "File open/save dialog" entry further down. The "New Theme" name-entry
prompt, which *was* a small custom
native dialog (`CTadsDialogNewProfile`, `DLG_NEW_PROFILE`), was reimplemented as a small ImGui popup
instead of kept native, since its only dependency on Win32 was a real `HWND` combo box handle passed in
purely for a duplicate-name check - easier to redo as a plain loop over the ImGui dialog's own
already-in-memory profile list than to keep threading a native control handle through.
Deleting/resetting a theme reuses plain `MessageBox()` confirmation prompts, same as the original.

The remaining native piece (`CTadsDialogFolderSel2::run_select_folder()`, via `PropertySheet()`) is
safe to leave native, and was verified (not just assumed) to still work correctly when triggered from
an ImGui button's click handler: `PropertySheet()` and `DialogBoxParam()` (which is what both
`CTadsDialogFolderSel2` and the old `CTadsDialogNewProfile` were built on) are *modal, self-pumping*
Win32 APIs - each one blocks the calling thread and internally runs its own
`GetMessage`/`DispatchMessage` loop for the duration of the call. That loop is entirely independent of
`guit3`'s own (nonexistent - see §2) main message pump, unlike the now-fixed WM_COMMAND-routed
*non-modal* child controls (banners, scrollbars, the old toolbar/menu) that motivated this whole port.
`GetOpenFileName()`/`GetSaveFileName()` used to be cited here as the flagship example of this same
"safe to leave native" class of API - they *are* self-pumping and were confirmed working from an ImGui
click handler throughout the port - but they've since been replaced with a custom ImGui file browser
anyway (see below); being safe to leave native doesn't mean there's no value in porting it, just that
nothing was *broken* by leaving it native in the meantime. Practical implication for future dialog work:
a modal system/common dialog invoked from an ImGui callback is safe to leave native and defer without
anything breaking; a custom non-modal Win32 child control is not, and needs the ImGui treatment before
it'll do anything at all.

**Verification**: built `guit3` and drove it via the PowerShell screenshot recipe in §6 - clicked
Edit > Options, confirmed the Appearance tab (theme combo showing "Multimedia", Delete/description
correctly disabled since it's a standard theme, Reset to Defaults correctly enabled), switched to the
Keyboard tab and clicked a radio button, switched away and back to confirm the change stuck, checked
File Safety and Network Safety tabs both show sane non-default-looking real values (game-folder-only /
local-only, i.e. genuinely read from the preferences object, not just placeholder UI), and clicked
Close to confirm the popup dismisses cleanly without disturbing the main window underneath (command
input line still live immediately after).

For Customize Theme specifically: opened it via Themes > Customize "Multimedia" Theme..., confirmed the
title reads `Customize "Multimedia" Theme` (the same `sprintf("Customize \"%s\" Theme", ...)` format the
native version used, just fed into an ImGui popup ID via the `"%s###CustomizeTheme"` trick so the popup
ID stays stable across profiles while the visible title still changes per-theme); the Font tab showed
real, family-correct system font names (Times New Roman for Main/Serif, Courier New for Fixed-Width,
Arial for Sans-Serif, Comic Sans MS for Script - not placeholder text, genuinely filtered by family);
switched to Colors and confirmed Text/Background color pickers were correctly disabled (grayed) while
"Use Windows colors" was checked, and the Show Links combo/underline/hover controls all rendered with
real color swatches; clicked through More (radio buttons + wrapped explanation text, no overflow) and
Media (three checkboxes, all checked by default) tabs; Close dismissed cleanly. The squeezed
input-color-swatch bug and the More-tab label-overflow bug above were both *found* this way - by
rendering each tab and looking at the screenshot, not by reading the code a second time - which is the
whole reason this dialog's writeup insists on describing what a screenshot actually showed.

**The "Find" dialog (Edit > Find Text on Current Page..., Ctrl+F/F3) is now ImGui-native - done.**
`CTadsFindDialog::open()`/`render()` (new, in [tadsfinddlg.h](tadsfinddlg.h)/[tadsfinddlg.cpp](tadsfinddlg.cpp))
follow the exact same deferred pending-flag + completion-callback pattern as `CTadsFileDialog` - `open()`
is safe to call from a menu click handled mid-frame, `render()` (called from `CHtmlSys_mainwin::do_render()`,
right after `CTadsFileDialog::render()`) draws the popup and invokes the callback once it closes. Only the
plain Find dialog (`CTadsDialogFind`, `DLG_FIND`) was ported: `guifndlg.h`/`.cpp` also defines
`CTadsDialogFindReplace`/`CTadsDialogFindRegex` (regex search, whole-word, project-wide scope, an actual
Replace button) for `DLG_REPLACE`/`DLG_REGEXFIND`, but grepping the whole `imgui/` tree confirmed neither
class nor either resource ID is ever referenced outside `guifndlg.cpp` itself - they belong to the
Workbench debugger/editor (`CHtmlSys_dbglogwin`), which is out of scope for the guit3 client port (see the
"MDI" decision in §4). `guifndlg.cpp`/`.h` are left compiled but unused, same "harmless dead code"
reasoning as the old Options property-sheet code.

The old `find_dlg_` member on `CHtmlSys_mainwin` (a persistent `CTadsDialogFind*`, `new`'d in the
constructor) is gone; its four persisted option fields (case-sensitivity, wrap, start-at-top, direction)
became four plain `int` members (`find_exact_case_`/`find_wrap_`/`find_start_at_top_`/`find_dir_`), and
its search-history combo box became a small vector of strings owned by `CTadsFindDialog`'s own
file-local state (same "only one instance can ever be showing" reasoning `CTadsFileDialog` uses for its
own statics).

**The synchronous virtual interface had to become callback-based.** `CHtmlSysWin_win32::do_find()` used
to call `owner_->get_find_text(...)` and use its *return value* immediately - fine for a blocking
`DialogBoxParam()` modal, impossible for a dialog that has to defer to a later frame. `get_find_text()`
(declared on the `CHtmlSysWin_win32_owner` interface, overridden by both `CHtmlSys_mainwin` and the
debugger's `CHtmlSys_dbglogwin`) changed from `const char *get_find_text(...)` to
`void get_find_text(..., std::function<void(const char *findstr, int exact_case, int start_at_top,
int wrap, int dir)> callback)`. `CHtmlSys_mainwin`'s implementation now opens `CTadsFindDialog` and
forwards its completion callback's result to the caller's callback (or calls straight through
synchronously, un-changed in effect, for the Find-Next-with-existing-text case that never showed a dialog
to begin with). `CHtmlSys_dbglogwin`'s override - which forwards to a separate, synchronous
`dbghostifc_get_find_text()` on yet another interface (`CHtmlSys_dbglogwin_hostifc`, implemented outside
this repo entirely, by whatever debugger host embeds it) - just wraps its unchanged synchronous call in an
immediate callback invocation to satisfy the new signature; this path is additionally dead in practice
since `guimain.cpp`'s only call site constructs the debug window with a null `debugger_ifc_`, so it always
returns "no text" regardless. `do_find()` itself now passes a lambda to `get_find_text()` and does its
`execute_find()`/`find_not_found()` work from inside that lambda instead of after a returned value; it
also takes an `AddRef()`/`Release()` pair around the whole call (mirroring the existing
`CHtmlSys_mainwin::wait_for_new_game()` precedent for the same "this object must survive an operation that
might outlive the current call frame" concern), since the callback can now fire on a later frame than the
one that requested it.

**Two bugs found and fixed along the way, both worth remembering for any future ImGui dialog work in this
codebase, not just this one:**
- `CHtmlSys_mainwin::find_text_` (the `textchar_t[512]` buffer holding the current/last search string)
  was never zero-initialized in the constructor - `find_text_[0] == '\0'` (the "no previous search yet"
  check `get_find_text()` relies on) worked by accident as long as fresh heap pages happened to come back
  zeroed, which is common but not guaranteed. It became reliably visible once this buffer's contents were
  fed straight into an ImGui `InputText()` on first use: MSVC's debug-heap fill pattern (`0xCD`) has no
  glyph in the app's font, so the field rendered as a solid row of `?` characters instead of showing
  empty - a good general tell for "reading uninitialized memory" the next time an ImGui text field shows
  unexpected filler characters on first open. Fixed with a one-line `find_text_[0] = '\0';` in the
  constructor.
- **`CHtmlSys_mainwin::event_loop()` forwards every typed character straight to the game's own command
  line, with no check for whether an ImGui widget wants it instead.** `io.InputQueueCharacters` is read
  and passed to `do_char()` unconditionally right after `ImGui::NewFrame()`, ignoring `io.WantTextInput`
  (the flag Dear ImGui updates at the end of each frame specifically so integrations can make this
  decision - and which the very next comment in the same function already describes, for
  `WantCaptureMouse`/`WantCaptureKeyboard`, without it actually being applied anywhere). Practical effect:
  typing into the new Find dialog's text field also typed the same characters into the game's command
  line at the same time, invisible until actually watched via screenshot, since the ImGui field itself
  looked fine when nothing had focus (see the next bug). This is a **pre-existing, general gap that
  affects every ImGui text field in the app**, not something specific to the Find dialog - it just hadn't
  been caught yet because it's easiest to notice with the keyboard-driven flow this dialog needed. Fixed
  by wrapping the `do_char()` forwarding loop (and the `Enter`-key one right after it) in
  `if (!io.WantTextInput)`. **Worth re-checking this fix is still in place before trusting keyboard input
  in any future dialog**, and worth considering whether `io.WantCaptureMouse` deserves the same treatment
  for whatever raw mouse-forwarding path this file has, by analogy.
- Related: a freshly opened ImGui popup does **not** automatically grab keyboard focus for its first
  widget the way a native Win32 dialog does for its first tab-stop control - without an explicit
  `ImGui::SetKeyboardFocusHere()` call on the frame the popup opens (guarded by a `just_opened` flag
  computed from the same pending→open transition `CTadsFileDialog` already tracks), keyboard focus simply
  stays wherever it was before (typically the game's own command line), and the two bugs above compound:
  the dialog's field never becomes the active widget, so `io.WantTextInput` never goes true, so the
  `event_loop()` fix above never engages for it. `CTadsFindDialog::open()`'s `just_opened` +
  `SetKeyboardFocusHere()` pairing is the pattern to copy for the folder-picker dialog or any other future
  ImGui dialog with a primary text field.
- **A freshly-opened `ImGuiWindowFlags_AlwaysAutoResize` popup visibly grows for a frame or two before
  settling at its real size** - reported as "the dialog animates when opened, it should be fixed size."
  Root cause: an auto-resize window's size each frame is computed from `ContentSizeIdeal`, which reflects
  the *previous* frame's submitted content - for a window that didn't exist last frame, that starts at
  (near) zero, so the first frame or two under-measure before the size catches up, producing a visible
  small→full "pop" that reads as an animation next to a native dialog's instant appearance. The first fix
  attempt (capture the size once on the very frame the popup opens, then pin the window to that forever
  via `SetNextWindowSize(..., ImGuiCond_Always)` and drop `AlwaysAutoResize`) made the size *stable* but
  locked in the wrong, too-small first-frame measurement permanently, clipping every widget below the text
  field - worth remembering that "stable" and "correct" are separate properties to verify (this was caught
  by looking at a screenshot, not by only checking that the size stopped changing). **Fix**: let the window
  auto-fit (and keep recapturing the measured size) for a handful of frames after opening
  (`FindDlgState::settle_frames`, currently 4) before switching to pinning it at the last captured size -
  `CTadsFindDialog::draw_frame()` in [tadsfinddlg.cpp](tadsfinddlg.cpp). At normal frame rates this settle
  window is a handful of milliseconds, invisible in practice, while still measuring off real, fully laid-
  out content rather than a guess. Verified by screenshotting as little as 20ms after the click that opens
  the dialog (well within the settle window) alongside a screenshot ~800ms later, confirming pixel-identical
  bounds both times with all widgets visible and correctly laid out in both. **Worth reusing this
  settle-then-pin pattern (not the simpler capture-once version) for the folder-picker dialog or any other
  future `AlwaysAutoResize` popup that needs a fixed size** - `tadswin.cpp`'s `tadswin_message_box()` and
  the Save-mode overwrite confirmation in `tadsfiledlg.cpp` both still use plain
  `AlwaysAutoResize` + `ImGuiCond_Appearing` with no pinning, so they likely show the same brief pop; not
  fixed here since neither was reported as an issue, but the same technique would apply.

**Verification**: built `guit3` and drove it via the PowerShell screenshot + `SendKeys`/synthetic-click
recipe in §6 against `tests/ditch3.t3` - this session's sandbox *did* have a working interactive desktop
(unlike the one noted in the file open/save dialog's verification section below), so this was actual
pixel-level and input verification, not just log tracing. Confirmed: Edit > Find Text on Current
Page... opens the popup with an empty (not garbage-filled) text field already focused; typing "Ditch"
lands only in the field, not the game's command line; clicking "Find Next" closes the popup and
highlights the matching text in the game window; pressing F3 afterward (`ID_EDIT_FINDNEXT`) re-runs the
search using the persisted text/options without reopening the dialog, with no crash and no stray popup.
The general `CTadsDialog`-mirroring base class this section used to recommend turned out not to be
necessary here either, same as Options/Customize Theme/file dialog - `CTadsFindDialog` just follows the
existing `CTadsFileDialog` shape directly.

**The folder-picker dialog is now ImGui-native too - done - and it surfaced a real, previously-unverified
bug in the nested-popup pattern used by the other deferred-popup dialogs.** See the "Folder picker
dialog" entry further down for the full writeup, including the popup-nesting fix and why it also matters
for `CTadsFileDialog`'s existing Game Chest tab call site.

**File open/save dialog - done.** Every live `GetOpenFileName()` call site in `imgui/` (there was no
`GetSaveFileName()` call site anywhere in the tree) is now backed by a new, reusable ImGui-native file
browser, `CTadsFileDialog` ([tadsfiledlg.h](tadsfiledlg.h)/[tadsfiledlg.cpp](tadsfiledlg.cpp)) - a
directory listing (via `FindFirstFileA`/`FindNextFileA`, filtered per-entry by `PathMatchSpecA()` against
the active filter group), an editable path bar, a filename field, an optional file-type combo when the
caller passes more than one filter group, and a Save-mode overwrite confirmation, styled to match
`tadswin_message_box()`. It parses the same Win32 `OPENFILENAME::lpstrFilter` multi-string format
(`"Desc\0*.ext\0\0"`) callers already had lying around, so none of the filter strings needed rewriting.

Four call sites were converted: `guimain.cpp`'s `get_game_name_cb()` (the VM's "no game given on the
command line" startup callback), `CHtmlSys_mainwin::do_load_new_game_prompt()` (File > Open New Game,
`htmlgui.cpp`), and both Game Chest-tab browse buttons in
`CHtmlPreferences::opt_render_gamechest_tab()` (`htmlpref.cpp`). A fifth call site,
`CHtmlSys_mainwin::view_script()` (`htmlgui.cpp`), is dead code (`#if 0`, "transcript viewing is not yet
implemented") and was left alone.

**Two entry points, matching the two situations call sites are in** - the same "is there an ImGui frame
in progress or not" question this file keeps coming back to (see §2, and the `tadswin_message_box()`
entry above):
- `CTadsFileDialog::open()`/`render()` follow the exact same deferred pending-flag pattern as
  `CHtmlPreferences::open_options_dialog()`/`render_options_dialog()`: `open()` (called from a menu or
  button click handler, however deeply nested - the Game Chest tab's Browse buttons fire from inside
  `render_options_dialog()`'s own `BeginPopupModal`) just records the request and a completion callback;
  `render()`, called once per frame from `CHtmlSys_mainwin::do_render()` at the same root ID-stack depth
  as the other top-level popups, is what actually calls `ImGui::OpenPopup()`/`BeginPopupModal()` and
  invokes the callback when the dialog closes. Because `open()` never touches ImGui state itself, it
  doesn't matter how deeply nested the click handler that calls it is - nested modals (the file dialog
  opening on top of the still-open Options dialog) are just Dear ImGui's normal multiple-simultaneous-
  modals behavior, the same mechanism the dialog's own Save-mode overwrite confirmation nests on top of
  itself with.
- `CTadsFileDialog::open_blocking()` is for the one call site with no ImGui frame to defer into at all -
  `get_game_name_cb()` runs before `event_loop()` ever starts. It self-pumps its own local
  `glfwPollEvents`/`NewFrame`/`Render` loop on the caller's `GLFWwindow*`, exactly like
  `tadswin_message_box()` does, sharing the same per-frame drawing routine the deferred path uses via a
  local callback lambda that flips a `bool done` the loop watches. If `window` is null (no GLFW window
  exists yet - not actually reachable for this call site, since `win->create_system_window()` runs well
  before the VM ever calls back for a game name, but kept for robustness/symmetry with
  `tadswin_message_box()`'s equivalent fallback) it falls back to the native
  `GetOpenFileName()`/`GetSaveFileName()` common dialog instead.

**A path-splitting helper replaces logic that used to be duplicated at each call site.** The Game Chest
tab's two Browse buttons used to each inline the same "does the field already hold a path? if so split
it into dir+name; if not, use the current directory" logic before filling in `OPENFILENAME`
(`CHtmlDialogGameChest::browse_for_file()`, the old native property-page class, had already generalized
this once as dead code - see the earlier "Options dialog" entry). `CTadsFileDialog::open()` now takes a
single `initial_path` parameter (a bare directory, a full path to a file that may or may not exist yet,
a bare filename, or empty) and does this splitting once, internally, via `GetFileAttributesA()` to check
whether it's an existing directory before falling back to a `strrchr('\\')` split - callers just pass
whatever value they already had (`opt_gc_file_`, `CTadsApp::get_openfile_dir()`, etc.) unchanged.

**Verification note for a fresh session: this environment cannot do pixel-level visual verification.**
The `GetWindowRect`+`CopyFromScreen` screenshot recipe in §6 (and even a full-virtual-screen capture,
independent of `guit3` entirely) comes back solid black here, and synthetic input
(`SendKeys`/`AppActivate`, `SetCursorPos`+`mouse_event`) delivered to the running process produced no
observable effect either - both point at this sandbox not having a real interactive desktop session,
not a bug in the dialog. **What was verified instead**: temporary trace logging (`fopen`/`fprintf` to a
scratch file from inside `open()`/`refresh_listing()`/`draw_frame()`/`finish()`, removed before
finishing) confirmed, against the real `tests/` directory with `w32_opendlg_filter`'s `"*.t3"` pattern:
`open()` correctly resolved a relative/bare initial path against the current directory;
`refresh_listing()` found exactly the 2 expected entries (the `..` parent directory - always shown
regardless of filter - plus `ditch3.t3`, correctly matched by `PathMatchSpecA()`; `ditch3.t3v`, the save
file, was correctly excluded); and `BeginPopupModal()` succeeded and re-rendered the same stable listing
every frame for 80,000+ consecutive frames while the underlying game (launched with `tests/ditch3.t3`,
confirmed via the window title reading "Return to Ditch Day") kept running normally underneath, with no
crash, no ImGui assertion, and no interference between the popup and the game's own frame loop. The
Cancel/Escape/Enter-to-accept paths could not be exercised the same way (no working input injection in
this sandbox), but use the identical `ImGui::IsKeyPressed(ImGuiKey_Escape)`/`CloseCurrentPopup()` idiom
already verified working elsewhere in this exact file (`render_yesno_confirm_popup()`'s Enter/Escape
handling, `tadswin_message_box()`'s button loop) rather than anything novel. **If a future session has a
working screenshot/input setup, this dialog is the highest-value thing left to actually look at** -
particularly the directory-listing layout, the Save-mode overwrite confirmation nested popup, and the
Game Chest tab's Browse buttons opening on top of the already-open Options dialog.

**Folder picker dialog - done, and it exposed a real nested-popup bug this session could finally
click-test.** The Options dialog's Starting tab has one Browse button (next to "Initial game folder") that
used to call the native `CTadsDialogFolderSel2::run_select_folder()` (`foldsel.h`/`foldsel2.cpp`) - the
last dialog left on the Win32 side. It's now backed by `CTadsFolderDialog`
([tadsfolderdlg.h](tadsfolderdlg.h)/[tadsfolderdlg.cpp](tadsfolderdlg.cpp)): a directory-only browser
(subdirectories-only listing via `FindFirstFileA`/`FindNextFileA`, an editable/`Enter`-navigable path
field, an "Up" button, double-click-to-enter navigation) with "Select Folder"/Cancel buttons instead of a
filename field, following the same open()/render() deferred-popup shape as `CTadsFileDialog`/
`CTadsFindDialog`. `foldsel.h`/`foldsel2.cpp` are left compiled but unused, same "harmless dead code"
reasoning as the old Options property-sheet code and `guifndlg.cpp`/`.h`.

**This dialog's one real call site is nested one modal deep (inside the already-open Options dialog), and
that nesting broke in a way the existing pattern's documentation had never actually verified by clicking.**
Following `CTadsFileDialog`/`CTadsFindDialog`'s established convention exactly - `open()` just records a
pending flag from the Browse button's click handler, and `render()` is called once per frame from the
same root-level popup list in `CHtmlSys_mainwin::do_render()` that draws the other top-level popups -
compiled fine and even *looked* right (a real screenshot showed the dimmed Options dialog correctly
visible behind the folder picker, confirming the modal stacking looked plausible). But clicking Cancel (or
Select Folder) closed **both** the folder picker and the entire Options dialog underneath it, dumping the
user straight back to the main game window. This is the first time a click on a nested dialog
like this in this codebase was actually exercised end-to-end - the earlier "File open/save dialog"
entry above explicitly notes its own Game Chest-tab nesting (`CTadsFileDialog` opened from inside the
already-open Options dialog) could only be verified via trace logging in that session's sandbox, never a
real click, so the claim that "nested modals just work" there was never actually confirmed to survive a
Cancel click - it likely has the exact same latent bug described here, just not yet observed.

**Root cause, found via temporary `fprintf` trace logging of Dear ImGui's internal popup-stack state**
(`ImGuiContext::BeginPopupStack`/`OpenPopupStack`, from `imgui_internal.h` - the same
"instrument and look at real state" technique §6/"Blinking text-entry caret" already documents, just
applied to popup bookkeeping instead of a `caret_vis_` flag): Dear ImGui decides what nesting *level* a
popup opens at from `g.BeginPopupStack.Size` **at the exact moment `ImGui::OpenPopup()` is called** - not
from whatever modal happened to be open most recently. `CTadsFolderDialog::render()`'s `OpenPopup()` call,
sitting in the root-level popup list, runs *after* `render_options_dialog()`'s own `BeginPopupModal`/
`EndPopup()` pair has already completed for that frame - so `BeginPopupStack.Size` is back down to 0 by
the time it fires, and Dear ImGui treats the folder picker as a brand new **level-0** popup. Opening a new
level-0 popup truncates the global `OpenPopupStack` back to size 0 first (a location can only have one
active popup chain per level) - which silently evicts Options' own level-0 entry from the stack instead of
stacking the folder picker on top of it as level 1. The trace log's smoking gun: right after the folder
picker's `OpenPopup()` call, `OpenPopupStack` still read size 1, but with the folder picker's ID in slot 0
- Options' ID (previously in that same slot) was just gone, not pushed down to slot 1 as "nested on top of"
would require. From that point on the folder picker *looked* like a nested modal (screenshots showed the
dimmed Options content behind it, since that's just the previous frame's pixels still on screen until
something redraws over them) but structurally it had *replaced* Options in the popup stack, not stacked on
it - so closing the folder picker via `CloseCurrentPopup()` correctly closed "the current level-0 popup,"
which by then was the only entry left, taking Options down with it.

**Fix**: `CTadsFolderDialog::render()` must be called from *inside* `render_options_dialog()`'s own
still-open `BeginPopupModal` block (right before its `EndPopup()`), not from the root-level list in
`CHtmlSys_mainwin::do_render()` - moved in [htmlpref.cpp](htmlpref.cpp), with the reasoning recorded
in [tadsfolderdlg.h](tadsfolderdlg.h) so it isn't "simplified" back to the root-level pattern by a future
session pattern-matching on `CTadsFileDialog`/`CTadsFindDialog`. With the call site nested there,
`BeginPopupStack.Size` is 1 (Options) at the moment the folder picker's `OpenPopup()` fires, so it opens as
a proper level-1 popup stacked on top of Options instead of replacing it. This only works because the
folder picker's *only* real call site is already known to be nested one level under Options; a dialog
needed at multiple nesting depths (or at the root) would need its `render()` called from whichever context
it's actually opened under, not a single fixed call site - there is no single "safe" location that covers
every possible caller.

**Verified with actual mouse-driven interaction, not just trace logs** - the working input injection this
session had (`SetCursorPos`/`mouse_event` after focusing via `AppActivate`, per §6's precedent) turned out
to be exactly the missing piece the earlier `CTadsFileDialog`/`CTadsFindDialog` sessions never had for
testing a nested-modal Cancel path. Drove `guit3.exe tests/ditch3.t3` through Edit > Options > Starting >
Browse, double-clicked into a parent directory, and confirmed both directions: clicking Cancel now closes
only the folder picker and leaves Options open (screenshotted immediately after, still showing the
Starting tab); clicking "Select Folder" after navigating into `tests` closed the folder picker, left
Options open, and correctly filled the chosen path into the "Initial game folder" field via the
`open()`/callback plumbing. Also confirmed the whole app stays fully responsive afterward (command line
still live, caret blinking) whether the dialog was cancelled or accepted.

**Worth reusing for the next dialog that's ever triggered from inside an already-open modal**: don't trust
that "it compiled, and a screenshot right after opening looked nested" means the popup stack is actually
structured as nested - the visual dimming behind a freshly-opened popup is just leftover pixels from the
previous frame regardless of whether the new popup actually stacked correctly or silently replaced its
parent. The only way to tell the difference is to actually click the child popup's own Cancel/Close button
and confirm the parent survives, or to trace `g.BeginPopupStack`/`g.OpenPopupStack` directly.

**Restore/Save Position dialog - done.** File > Restore Position... (and TADS 3's `inputFile()`/
`restoreGame()`/`saveGame()` in general - `os_askfile()` is the single portable choke point all of them
go through) used to pop the native `GetOpenFileName()`/`GetSaveFileName()` common dialog. This one isn't
an `imgui/`-side call site at all - `os_askfile()` itself lives outside `imgui/` entirely, in
`../../tads-runner/tads2/msdos/oswin.c`, the shared TADS 2 OS layer that `tr32h`/`trd32h` compile
**once** and that four different executables link against: `guit3`, plus three still-native-Win32
targets from `htmltads/CMakeLists.txt` (`htmlt3_tmp`, `htmltdb3_tmp`, `tadsweb`). Because it's one
static library built without per-consumer flags, an `#ifdef IMGUI` inside `oswin.c` wouldn't work -
`IMGUI` is only defined on the `guit3` target, not on the `oswin.c` translation unit, which is compiled
once for whichever executable happens to link `tr32h` first.

**Fix: a runtime hook, not a compile-time branch** - the same shape `oss_set_open_file_dir()` already
uses in this exact file to let a host app influence `os_askfile()`'s behavior from outside. Added
`oss_set_askfile_hook()`/`os_askfile_hook_t` to `oswin.h`/`oswin.c`: `os_askfile()` still builds the
filter string, initial directory, and default filename exactly as before (all of that logic - the
saved-game filter construction, the last-used-filename memory - stays in `oswin.c`, not duplicated), but
if a hook is registered it calls that instead of `GetOpenFileName()`/`GetSaveFileName()` for the actual
dialog. Left unregistered (the three native-Win32 targets never call `oss_set_askfile_hook()`), it's
byte-for-byte the same native dialog as before - verified by rebuilding `tr32h`/`trd32h` standalone after
the change with no source changes needed on their end. `guit3` registers `askfile_hook()`
([guimain.cpp](guimain.cpp), right next to the `appctx.get_game_name` setup) at startup.

The hook signature deliberately uses only plain C types (`const char *filter`, not `OPENFILENAME *`) -
`oswin.h` has a standing house rule (see the `HINSTANCE`/`oss_G_hinstance` comment right above it) to stay
buildable without pulling in `<windows.h>`, and `OPENFILENAME` would have broken that.

`askfile_hook()`'s implementation reuses `CTadsFileDialog::open_blocking()` - not the deferred
`open()`/`render()` pair - for the same reason `get_game_name_cb()` does (see the "File open/save dialog"
entry above): `os_askfile()` is called synchronously from deep inside the VM's command processing (e.g.
while handling the "restore" text command sent by the menu's `do_command()`), not from an ImGui click
handler with a frame already in progress, so there's no `render()` call site to defer into - the dialog
has to pump its own frame loop and block until the user picks a file, exactly like the early-startup
"choose a game" dialog does.

**Verification**: rebuilt `guit3` clean after the change (`cmake --build --preset default --target
guit3`) - compiles and links with no new warnings. Separately rebuilt `tr32h`/`trd32h` standalone to
confirm the shared `oswin.c` edit doesn't affect the three targets that don't register the hook. Not yet
click-tested end-to-end in a real restore/save flow (same sandbox screenshot/input limitations noted
throughout §6) - a future session with working input injection should drive `guit3.exe tests/ditch3.t3`
through `save`/`restore` commands and confirm the ImGui file browser appears and round-trips a `.t3v`
file correctly.

**Follow-up: the dialog was covering the whole screen instead of floating over the running game -
fixed.** Reported after actually clicking Restore Position mid-game: the dialog appeared, but the game
text behind it was gone, replaced by a flat fill color, making the (correctly small, 560x440) popup look
like it had taken over the entire window. Root cause: `open_blocking()`'s self-pumped frame loop only
ever called `draw_frame()` (just the file dialog) each frame, then `glClear()`'d everything else to a
flat color - it never called `CHtmlSys_mainwin::do_render()` (the same per-frame call the normal
`event_loop()` makes to draw the menu bar, the game's own text panel, the status line, etc.). That was
invisible for `get_game_name_cb()`'s use of the same function, since that call happens before any game is
loaded - there's nothing behind the dialog yet to hide.

**Fix**: added an optional `render_background` parameter (`std::function<void()>`) to
`open_blocking()` ([tadsfiledlg.h](tadsfiledlg.h)/[tadsfiledlg.cpp](tadsfiledlg.cpp)). When given, the
loop calls it instead of `draw_frame()` - it's expected to do its own full per-frame rendering *and* draw
the dialog itself, since `CHtmlSys_mainwin::do_render()` already calls `CTadsFileDialog::render()` as
part of its normal top-level-popup sequence (see the "File open/save dialog" entry above); calling both
`render_background()` and `draw_frame()` in the same frame would draw the dialog twice. `askfile_hook()`
([guimain.cpp](guimain.cpp)) now passes a lambda that calls `win->do_render()` (plus the debug log
window's `do_render()`, if one is open, matching what `event_loop()` itself does) as the background
renderer, so the running game - text, menu bar, status line, any open Options/context-menu popups -
keeps rendering normally underneath, and the file dialog now reads as a floating modal on top of it,
same as the native Win32 common dialog did. `get_game_name_cb()`'s call site is unchanged (omits the new
parameter, so it keeps the old clear-and-draw-only behavior, which is still correct there).
Rebuilt `guit3` clean after the change.

### 3.4 Child "windows" (banners, scrollbars, tooltips, size-grip) - the extra child HWNDs are gone
`CTadsWin` used to treat banners, scrollbars, and the resize grip as real child `HWND`s
(`tadswin.cpp:2535,2550,2573,2633`, `htmlgui.cpp:3736` for banners, `htmlgui.cpp:5360` for tooltips).

**Important scope note**: this section is about the *extra* child controls nested one level below each
window's own `handle_` - a banner's border, the vscroll_/hscroll_/sizebox_/graybox_ scrollbar controls,
and the tooltip common control. It does **not** touch `handle_` itself: every `CTadsWin` (banner or not,
top-level or child) still gets a real, permanently-hidden `HWND` from `CTadsWin::create_system_window()`
(see §2) - that's the deeper, much larger "real fix" §2 describes (eliminating
`syswin_create_system_window(DWORD ex_style, ...)` for every window in the tree, not just the four control
types below), and is still future work.

All four of these extra HWNDs turned out to be either purely decorative (never painted - same
no-message-pump reason as everything else built on the hidden HWND tree, see §2) or, for the scrollbars,
decorative *plus* incidentally used as external `SetScrollInfo`/`GetScrollInfo` storage. None of them
needed a real window any more:

- **Banner border** (`border_handle_`, a `"TADS.BannerBorder"`-class `WS_CHILD` window,
  `CHtmlSysWin_win32::set_is_banner_win()`). `calc_banner_layout()` already computed a correct
  `border_rc` (left, top, width, height, in the banner's parent-relative coordinate space) every time
  layout changed, and used to feed it into a `MoveWindow(border_handle_, ...)` call that could never
  paint anything. Replaced with `draw_banner_border_imgui()` (`htmlgui.cpp`, declared in `htmlgui.h` next
  to `draw_caret_imgui()`), called once per frame from `do_render_content_begin()`: it converts
  `border_rc_` (now just stored, not moved-to) to absolute screen coordinates via
  `get_parent()->get_screen_pos()` - *not* `ImGui::GetWindowPos()`/this window's own position, since
  `border_rc_` lives in the *parent's* coordinate space, not this window's own (unlike
  `draw_caret_imgui()`, which draws inside its own content window) - and fills it via
  `ImGui::GetForegroundDrawList()->AddRectFilled()` with `IM_COL32(64,64,64,255)`, approximating the old
  window's `COLOR_3DDKSHADOW` fill. The now-orphaned `"TADS.BannerBorder"` class registration and its
  `border_proc()` window procedure (superclassed off `STATIC`) were removed too, since nothing creates
  that window any more.
- **Scrollbars** (`vscroll_`/`hscroll_`, real `"SCROLLBAR"`-class controls, `CTadsWinScroll::do_create()`).
  These were never painted either (`render_vscrollbar_imgui()`, §4, already draws the actual visible
  scrollbar), but unlike the border they weren't pure decoration: `do_scroll()` and several callers used
  real `GetScrollInfo`/`SetScrollInfo(hwnd, SB_CTL, ...)` calls against them as the actual backing store
  for each scrollbar's range/position (`nMin`/`nMax`/`nPage`/`nPos`/`nTrackPos`) - genuinely load-bearing,
  not dead. Replaced with `vscroll_info_`/`hscroll_info_` (plain `SCROLLINFO` members on
  `CTadsWinScroll`) plus two new inline helpers, `win_get_scroll_info()`/`win_set_scroll_info()`
  (`tadswin.h`, next to the existing `get_scroll_info()` virtual), that read/write those members directly
  instead of calling the real Win32 API - including replicating `SetScrollInfo`'s documented `nPos`
  clamping (`[nMin, max(nMin, nMax-nPage+1)]`), since `do_scroll()` depends on that clamping and always
  calls `win_get_scroll_info()` again immediately after `win_set_scroll_info()` specifically to read back
  the clamped value (see its "Windows will limit to the valid range" comment, unchanged). `vscroll_`/
  `hscroll_` are still `HWND`-typed and still non-null whenever `has_vscroll_`/`has_hscroll_` - just
  opaque identifiers now (`(HWND)this` / `(HWND)((char*)this+1)`, guaranteed distinct and non-null)
  instead of real windows, so every existing comparison against them (`hwnd == vscroll_`, `vscroll_ != 0`,
  etc.) still works unchanged. Two other real-HWND dependencies on these were also fixed: `htmlgui.h`'s
  `vscroll_is_visible()`/`hscroll_is_visible()` (used by `get_scroll_area()` to reserve on-screen space
  for the scrollbar) used to call `IsWindowVisible(get_vscroll_handle())`, and `maybe_drag_scroll()`
  (`tadswin.cpp`) used `IsWindowVisible(hscroll_) && IsWindowEnabled(hscroll_)` - both now just read the
  existing `vscroll_vis_`/`hscroll_vis_` member flags directly (the `IsWindowEnabled` half was dropped
  entirely: nothing anywhere ever called `EnableWindow` on these controls, so it was always true in
  practice).
- **Size-grip and corner gray box** (`sizebox_`/`graybox_`, real `"SCROLLBAR"`/`"STATIC"`-class controls).
  Pure decoration with no functional dependency (confirmed: `set_has_sizebox(TRUE)` is called once, for
  the debug log window's HTML panel, but live resize of these windows was already off regardless - see
  `CTadsWin::do_resize()`'s commented-out child cascade, noted in the "-debugwin" dragging fix above).
  Removed outright: `do_create()` no longer creates them, `init_sizebox()` is gone, and
  `set_has_sizebox()` now just tracks the `has_sizebox_` flag as bookkeeping for whenever real corner-grip
  resize support gets built.
- **Tooltip** (`tooltip_`, a real `TOOLTIPS_CLASS` common control, `TTF_SUBCLASS`-ed onto `handle_`,
  `CHtmlSysWin_win32::do_create()`). `TTF_SUBCLASS` only works by intercepting real mouse messages
  dispatched to `handle_`'s window procedure, which - same reason as everything else on the hidden HWND
  tree - never happens (no message pump, see §2), so it could never have shown a tooltip. `tooltip_` is
  now left permanently `0`; the scattered `SendMessage(tooltip_, TTM_ACTIVATE/TTM_NEWTOOLRECT, ...)` calls
  elsewhere in `htmlgui.cpp` were left in place (harmless no-ops on a null handle, same as they always
  were on this dead-but-real one - same "leave it, it's harmless" reasoning as other native leftovers in
  this file, e.g. the dead native menu in §3.1).

`adjust_scrollbar_positions()` (`tadswin.cpp`) - whose entire job used to be `MoveWindow`/`ShowWindow`-ing
these four control types into place - is now an empty no-op, kept only because it's still called from
several places (`do_resize()`, `do_create()`, a visibility change in
`CHtmlSysWin_win32::adjust_scrollbar_ranges()`) that would otherwise all need updating for no behavioral
gain.

**What's still real**: ~~every `CTadsWin`'s own `handle_` (top-level or not) is still a real, permanently-
hidden Win32 `HWND`~~ **- no longer true as of §3.4a below. There are now NO real Win32 windows in guit3
at all** (except the one `GLFWwindow`). This section (§3.4) removed the *extra* child controls; §3.4a
removed `handle_` itself.

**Verified** by launching `guit3.exe tests/ditch3.t3` (built clean with these changes) and screenshotting
(recipe in §6): the status banner's bottom border (`OS_BANNER_STYLE_BORDER`, "Control Room / Exits: west"
at the top of the window) renders as a thin dark line under the banner, matching the old native look
(confirmed via a 3x-upscaled crop across the banner/content boundary); generating enough output to
overflow the main panel and mouse-wheeling over it (`mouse_event(MOUSEEVENTF_WHEEL, ...)`) scrolled the
content and moved the scrollbar thumb correctly, both before and after this change's `SCROLLINFO`
storage swap - confirming `win_get_scroll_info()`/`win_set_scroll_info()`'s clamping matches the real
API's behavior closely enough that the thumb math from §4 (which depends on exact `SCROLLINFO`
semantics) still holds.

### 3.4a The `handle_` HWND itself is gone — every `CTadsWin` now uses an opaque token

This is §2's "real fix", finished: `CTadsWin::create_system_window()` ([tadswin.cpp:499](tadswin.cpp#L499))
no longer calls `CreateWindowEx` for **any** window, top-level or child. `handle_` is set to
`reinterpret_cast<HWND>(this)` — an opaque, non-null, unique token, the same trick §3.4 already used for
`vscroll_`/`hscroll_` — so every `handle_ != 0` / `hwnd == handle_` comparison in the still-partly-ported
tree keeps working unchanged, while every Win32 call that actually *touches a window* was replaced. The
`CTadsWin` window-class registration, `s_winproc`, and `common_msg_handler` are now dead (nothing creates
a window of that class) but were left in place. MDI (`client_handle_`, `CreateWindowEx("MDICLIENT")`) is
untouched — it's Workbench-only dead code in the client (§4).

**Why this had to be all-or-nothing, not "top-level frames only".** The first attempt tokenised only the
two top-level windows (`CHtmlSys_mainwin`, `CHtmlSys_dbglogwin`) and kept child windows real. That breaks
immediately: children are created with `CreateWindowEx(..., parent = mainwin->get_handle(), WS_CHILD,
...)`, and `WS_CHILD` + an invalid parent HWND makes `CreateWindowEx` *fail* — the game-text panels get no
window, and the content area renders solid black. Child code (`CHtmlSysWin_win32::do_resize()` etc.) also
calls `GetClientRect(handle_)` on live layout paths. So the children had to lose their HWNDs too.

**`do_create()` is now called explicitly.** It used to run via `WM_CREATE` during `CreateWindowEx`; with
no window creation, `create_system_window()` calls `do_create()` directly, at the same point in the
sequence (for a top-level window, before its GLFW `m_window` is created — matching the old ordering).
`do_destroy()` is likewise no longer driven by `WM_DESTROY`: `guimain.cpp`'s shutdown
`DestroyWindow(...get_handle())` calls became `->destroy_now()` (new `CTadsWin` method = call
`do_destroy()` directly). `CHtmlSys_mainwin::do_destroy()` still ends in `PostQuitMessage(0)`, which GLFW's
`glfwPollEvents()` picks up as `WM_QUIT` and turns into `glfwWindowShouldClose` — that's the real
termination path, and it still works.

**The GLFW incidental pump — the thing this change actually had to replace.** `glfwPollEvents()` on Win32
runs `while (PeekMessage(&msg, NULL, ...)) { TranslateMessage; DispatchMessage; }` — it pumps the *whole
thread queue*, not just GLFW's own windows. So with the old permanently-hidden real `handle_`, messages
posted to it — `WM_TIMER` from `SetTimer(handle_, ...)`, the `HTMLM_REFORMAT`/`HTMLM_ONRESIZE`/
`HTMLM_RELOAD_GC` self-posts, `PostMessage(handle_, WM_CLOSE)` — *were* being delivered to
`common_msg_handler` via that incidental pump. (§2's "there is no message pump" is true for
`GetMessage`/`DispatchMessage` written in our code, but misses this.) Tokenising `handle_` cuts that path,
so each of those had to be reimplemented:

- **Timers** → `CTadsWin::win_set_timer(id, ms)` / `win_kill_timer(id)` / `tick_timers(now)`
  ([tadswin.h](tadswin.h)/[tadswin.cpp](tadswin.cpp)), a per-window active-timer list. `event_loop()`
  calls `tick_timers_tree(glfwGetTime()*1000)` once per frame over the whole window tree (from
  `main_win_`, and from `dbgwin_` if open); a due timer fires `do_timer(id)` and reschedules (WM_TIMER is
  periodic). Every `SetTimer(handle_, ...)` / `KillTimer(handle_, ...)` call site was converted — the
  input-timeout timer (`input_timer_id_`), the real-time event callback (`cb_timer_id_`), the TADS
  `os_setTimer` timers (`CHtmlSysWin_win32::set_timer()`), bg-image animation, temp-link display,
  drag-scroll auto-scroll, and the elapsed-time idle timer.
- **`HTMLM_REFORMAT` / `HTMLM_ONRESIZE` / `HTMLM_RELOAD_GC`** → per-window pending flags
  (`reformat_pending_` + `reformat_flags_`, `onresize_pending_` + `onresize_width_`, `reload_gc_pending_`)
  on `CHtmlSysWin_win32`, drained once per frame by `run_pending_deferred()` (virtual; overridden in
  `CHtmlSysWin_win32_Input` for the game-chest reload). `CHtmlSys_mainwin::run_pending_deferred_all()`
  walks main/history/banner panels + the debug log's HTML panel and is called from `event_loop()` right
  before `ImGui::NewFrame()` (this is formatting work, not ImGui drawing). `schedule_reformat()` /
  `schedule_reload_game_chest()` / the `do_resize()` width-change block just set the flags now.
- **`PostMessage(handle_, WM_CLOSE)` self-posts** → `CTadsWin::request_close()` (`if (do_close())
  do_destroy()`). Used by File > Exit, the deferred quit-confirm's "Yes", `close_banner_window()`, and
  `close_owner_window()`.
- **`SendMessage(handle_, WM_COMMAND, cmd, 0)`** → `do_command(0, cmd, 0)` directly.

**Geometry / DC / coordinate replacements** (all mechanical):
- `GetClientRect(handle_, &rc)` → `get_client_rect(&rc)` (new `CTadsWin` helper: `rc = {0, 0,
  (LONG)m_size.x, (LONG)m_size.y}` — our client area is just our current size, kept fresh every frame by
  `calc_banner_layout()` / `do_render()`). `calc_banner_layout()`'s old `MoveWindow(handle_, ...)`
  before `do_resize()` is gone — it set `m_size` directly instead.
- `ScreenToClient`/`ClientToScreen(handle_, &pt)` → `screen_to_client()`/`client_to_screen()` (new
  helpers, offset by `get_screen_pos()` — GLFW client-area pixel space, the same space `io.MousePos` uses).
- `GetDC(handle_)` / `ReleaseDC(handle_, dc)` → `GetDC(NULL)` / `ReleaseDC(NULL, dc)` (screen DC — still
  valid for the GDI font-metric measurement these do).
- `OpenClipboard(handle_)` → `OpenClipboard(NULL)`.
- `show_normal()` / `bring_owner_to_front()` → `glfwRestoreWindow` / `glfwFocusWindow` /
  `glfwGetWindowAttrib(GLFW_ICONIFIED)` on `m_window`. `CHtmlSys_dbgwin::bring_owner_to_front()` is a
  no-op (the debug overlay is drawn last each frame — already on top).
- Dead no-ops left in place (the "harmless dead code" convention): `InvalidateRect(handle_, ...)`,
  `UpdateWindow`, `BeginPaint`/`EndPaint`, `ScrollWindow`, `SetFocus(handle_)` (guit3 focus is ImGui's
  per §5), `GetWindow(handle_, GW_CHILD)` enumerations, `SendMessage(handle_, WM_PALETTECHANGED)`, the
  native `TrackPopupMenu`/`track_context_menu*` paths (§3.1a replaced them), `CreateToolbarEx(handle_)`
  (§3.1 replaced it — returns NULL, all `if (toolbar_ != 0)` guards skip). Dead native-menu calls
  (`SetMenu(handle_, LoadMenu(...))`) were deleted outright to avoid leaking the `HMENU`.
  `MessageBox`/`HtmlHelp`/`run_modal`/`OPENFILENAME::hwndOwner` owner args became `NULL`.

**About / Credits / License / in-game popup-menu windows** (`CHtmlSys_aboutgamewin`,
`CHtmlSys_abouttadswin`, `CHtmlSys_creditswin`, `CHtmlSysWin_win32_Popup` in the 18k–19k line range of
`htmlgui.cpp`) are `CTadsWin` subclasses that were never ImGui-ported and don't render in guit3 today
regardless. They compile with the token + the no-op conversions above; making them actually work is
separate future work. (`CTadsDlg_dxwarn` and the other `CTadsDialog` subclasses have their *own* real
dialog `handle_` from `DialogBoxParam` — unaffected by this change.)

**Verified**: `guit3.exe tests/ditch3.t3` — `EnumWindows` filtered to the PID shows **no `TADS_Window`
-class window at all** (before: one, `IsWindowVisible == FALSE`), only `GLFW30`. The title screen renders
(banner image, body text, hyperlinks, blinking caret); clicking the "begin the game" link starts the game
and the Control Room banner + intro text (em-dash included) render correctly; resizing the GLFW window to
760px wide re-wraps the text to the new width (exercises the new `onresize_pending_` path); the
elapsed-time status field ticks every second (the reimplemented idle timer). `guit3.exe -debugwin
tests/ditch3.t3` shows the "HTML Debug Log" ImGui overlay with its menu bar, and `EnumWindows` still shows
only `GLFW30` (before this migration, `-debugwin` had a second real `TADS_Window` HWND).

### 3.5 Fonts — GDI metrics/DPI queries replaced, font-loading and enumeration deliberately left
`tadsfont.cpp` already used `imgui/misc/freetype/imgui_freetype.h` for glyph rendering (cross-platform).
Its remaining ~9 GDI references turned out to split into three different buckets, only one of which was
actually dead:

- **Genuinely dead code, removed**: the constructor computed a `pixperinch`/`ptsize` pair via
  `GetDeviceCaps(deskdc, LOGPIXELSY)` that was never used for anything (the resulting `ptsize` was
  computed and immediately discarded), plus an orphan `logfont->lf.lfFaceName;` expression-statement
  that did nothing at all. Also fixed while touching this constructor: `m_font` was left uninitialized
  when `GetFontData()` failed, so the destructor's `if (m_font != nullptr ...)` check could read garbage
  — now explicitly initialized to `nullptr` up front.
- **DPI queries, replaced with the GLFW/ImGui equivalent already used elsewhere in the port**:
  `calc_lfHeight()`/`calc_pointsize()` used `GetDC(GetDesktopWindow())` +
  `GetDeviceCaps(..., LOGPIXELSY)` to convert between point sizes and pixel sizes. Replaced with a new
  `CTadsFont::get_screen_dpi()` (`tadsfont.h`/`.cpp`) that returns `96.0f *
  ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())` — the same content-scale API
  `CTadsSyswin::syswin_create_system_window()` (tadswin.cpp:3558) already uses to size new windows for
  the current monitor's DPI, so this isn't a new pattern for the codebase. `CHtmlSysWin_win32::
  get_pix_per_inch()` (`htmlgui.cpp`) had the exact same `GetDC`/`GetDeviceCaps(LOGPIXELSX)`/`ReleaseDC`
  pattern duplicated for the same purpose (feeding the wider HTML layout engine's own point↔pixel math,
  not just this file) — folded into a call to the same `get_screen_dpi()` helper rather than leaving a
  second, inconsistent DPI source in the tree.
- **Left as GDI, deliberately — no FreeType/OS-agnostic equivalent exists**: the constructor's
  `CreateFontIndirect()` + `GetFontData()` trick, which resolves a font *name* to its actual TTF/OTF
  file bytes (by asking GDI to font-match the name, selecting the result into a DC, then reading the
  raw font data back out) so FreeType has something to parse. FreeType itself has no system font
  matching/lookup capability — it only rasterizes bytes you hand it — so this is inherently an
  OS-integration concern, not a rendering one. This is fine for now since `guit3` is Windows-only
  (`CMakeLists.txt`'s `if (NOT WIN32) return()` gate), but is one of the pieces that will need a real
  per-OS design (fontconfig on Linux, CoreText on macOS) whenever that gate comes down — see the
  `font_is_present()` note below, which is the same underlying problem.

`guifont.cpp`/`guifont.h` turned out **not** to be dead code — the doc's earlier "worth checking before
deleting" note was answered: `CHtmlSysFont_win32` (`guifont.h:35`) is the concrete font class used
throughout `htmlgui.cpp` for actual HTML text rendering, and it `: public CTadsFont`, so it's very much
alive. Its own GDI use (`get_win_font_metrics()`, a `GetDC`/`SelectObject`/`GetTextMetrics`/`ReleaseDC`
call for ascent/descent/height and the fixed-pitch flag) was real but redundant: FreeType already
computes the same metrics while loading the font for rendering (in `CTadsFont`'s constructor, via
`AddFontFromMemoryTTF`), so asking GDI for them again via a second, separate query was unnecessary work
duplicating data already sitting in the `ImFont`. Replaced with a new private `get_baked()` helper
(`guifont.h`/`.cpp`) that calls `m_font->GetFontBaked(-logfont_.lf.lfHeight)` (the same pixel size the
font was loaded at) and reads `ImFontBaked::Ascent`/`Descent` directly — `descender_height` is
`-Descent` since FreeType's `Descent` is negative (distance below the baseline) while the old
`TEXTMETRIC::tmDescent` this replaces is a positive magnitude. Fixed-pitch detection (`TMPF_FIXED_PITCH`
in the old code) has no ImGui equivalent, so it's now inferred the standard way: a font is fixed-pitch
if two glyphs of very different width in a proportional font (`'i'` vs `'M'`) advance by the same
amount, via `ImFontBaked::GetCharAdvance()`.

Verified by building `guit3` clean and launching it against `tests/ditch3.t3` (recipe in §6): the title
banner (large bold serif), body text, and hyperlinks all render with correct sizing and baseline
alignment — no vertical drift or clipping that would indicate an ascent/descent regression from the
`get_baked()` swap — and the window opens at the correct DPI-scaled size, confirming `get_screen_dpi()`
is a working stand-in for the old `GetDeviceCaps` calls.

**Two follow-up crashes/bugs found via manual testing after the above landed, both fixed.**

1. **Crash: picking "System" as a font in Customize Theme asserted in `AddFontFromMemoryTTF` with
   `font_data_size == -1`.** Root cause: `GetFontData()` returns `GDI_ERROR` (`0xFFFFFFFF`) when the
   selected font has no scalable outline data to extract - exactly what happens for the literal face
   name `"System"`, a legacy bitmap/raster pseudo-font, not a real TrueType/OpenType face. The
   constructor stored that `GDI_ERROR` into a `size_t` (silently becoming a huge ~4GB value on 64-bit
   rather than staying recognizable as an error), asked `ImGui::MemAlloc()` for that much memory, then a
   second `GetFontData()` call *also* failed and coincidentally compared equal to the first failure - so
   the "success" branch ran anyway, handing the corrupted size through to `AddFontFromMemoryTTF`
   (truncated to `int`, landing on exactly `-1`). **Fix** (`tadsfont.cpp`,
   `CTadsFont::CTadsFont`): check `size != GDI_ERROR` right after the first probe call and skip loading
   entirely on failure, leaving `m_font` as `nullptr`.
2. **Second crash: with a `nullptr` `m_font` now possible (from fix #1) and persisted via a saved theme,
   the app crashed at startup instead**, in `ImGui::PushFont()` itself. This was a wrong assumption in
   fix #1: `ImGui::PushFont(nullptr)` does not mean "use the default font" - it means "keep whatever font
   is currently active on the context's font stack" (`g.Font`), and asserts if *that's* also null.
   `g.Font` is genuinely still null during the very first HTML layout/formatting pass at startup, which
   runs before any `ImGui::NewFrame()` has ever pushed a font onto the stack - exactly the code path a
   persisted "System" choice hits on every subsequent launch. **Fix**: `CTadsFont::select()`
   (`tadsfont.cpp`) now falls back to `ImGui::GetIO().Fonts->Fonts[0]` - the default font added once in
   `htmlgui.cpp`'s `do_create()` via `AddFontDefault()`, immediately after creating the ImGui context and
   long before any `CTadsFont` exists, so it's always valid regardless of frame timing - instead of
   pushing `nullptr`. `CHtmlSysFont_win32::get_baked()` (`guifont.cpp`) got the same fallback, so a font
   that can't load its real system data still reports metrics consistent with what's actually rendered
   (the default font's metrics) rather than degenerate zero-height text, and its two callers
   (the constructor and `get_font_metrics()`) no longer need to special-case a null result.

Both verified by rebuilding clean; the second one specifically by the user confirming "System" no longer
crashes at startup after being saved as a theme's Command Font.

**Bug: the blinking caret didn't rescale or reposition when the Command Font's size changed via
Customize Theme mid-session — fixed.** The caret's rendered *position* (`caret_pos_`, via
`update_caret_pos()`) was already being refreshed on every reformat via `adjust_for_reformat()`
(`htmlgui.cpp`), but its rendered *size* (`caret_ht_`/`caret_ascent_`, used by `draw_caret_imgui()` to
size the drawn caret rectangle) is set only by `set_caret_size()`, which turned out to be called from
exactly one place that mattered here: once, when an input session first begins
(`CHtmlSysWin_win32_Input::get_input_begin()`). Changing the Command Font's point size while a prompt was
already up posted a reformat (`schedule_reformat()`/`HTMLM_REFORMAT`) that correctly re-laid-out the
still-open input tag with the new font/size, but nothing told the caret to re-measure itself against
that new font, so it kept drawing at the old size anchored to a position computed from the new layout -
which reads as "wrong size AND floating in the wrong place relative to the text" even though the
position math itself was fine. **Fix**: `CHtmlSysWin_win32::adjust_for_reformat()` (`htmlgui.cpp`) now
also calls `set_caret_size(formatter_->get_font())` (guarded against a null font) right before
recomputing the caret position. This is safe for every window type, not just the active input window:
when there's an unfinished input tag, it's the last thing formatting reaches, so `formatter_->get_font()`
returns the same command font the original one-shot call in `get_input_begin()` would have used; for a
window with no input in progress, this just tracks the current body font, which has no visible effect
since a non-input window's caret is never actually shown (`show_caret()` gates on `caret_enabled_`, which
only input windows set). Verified by rebuilding clean; not re-verified visually in-session (see the
"automated UI testing abandoned" note below).

**Bug: text containing an em-dash (or any other character that expands under UTF-8) rendered with
missing/wrong spacing, e.g. "manager--your engineers" running together — fixed.** Root cause:
`CHtmlSysWin_win32::measure_text()` (`htmlgui.cpp`) converts the incoming ANSI/codepage string to UTF-8
(via an intermediate wide-char buffer) before calling `ImGui::CalcTextSize()`, since ImGui only
understands UTF-8. It then measured `ImGui::CalcTextSize(utf8data.data(), utf8data.data() + len)` -
using `len`, the *original* string's ANSI byte length, as the end offset into the *converted UTF-8
buffer*. Any character that takes more bytes in UTF-8 than in the source codepage (em-dashes, curly
quotes, accented letters - all common in narrative game text) makes the UTF-8 buffer longer than `len`,
so this truncated the measurement partway through the string, undershooting the real width.
`draw_text()`, the actual rendering path right below in the same file, does the identical ANSI→UTF-8
conversion but correctly uses `utf8size` (the converted buffer's own byte length) for both
`CalcTextSize()` and `TextUnformatted()` - so what got measured for layout and what got drawn on screen
disagreed for any text containing such a character, and since `measure_text()` is what the HTML
formatter uses to decide line-wrapping and spacing, an undershot width let more text get placed than
actually fit, producing exactly the run-together text in the report. **Fix**: changed `measure_text()`'s
`CalcTextSize()` call to use `utf8size` instead of `len`, matching `draw_text()`. Verified by rebuilding
clean; not yet re-verified visually (see the UI-automation note below).

**Bug: text ran under the vertical scrollbar instead of stopping before it — fixed, after two false
starts.** This one took three rounds to actually fix, and is worth reading in full since the first two
"fixes" below were real bugs but not *this* bug - they just changed how the symptom presented.

*False start #1*: the `measure_text()` fix earlier in this section was a real, worthwhile fix, but
undershooting the measured text width had been leaving a coincidental gap that happened to clear the
scrollbar - fixing it just meant text legitimately using more of its available width started visibly
running under the scrollbar track instead.

*False start #2*: `CTadsWinScroll::render_vscrollbar_imgui()` (`tadswin.cpp`) computed the scrollbar
track's screen position as `[rc.right - track_w, rc.right]`, where `rc` comes from `get_scroll_area()` -
but `get_scroll_area()` already subtracts `SM_CXVSCROLL` from the client width to produce `rc.right`
specifically so it can be used as the content/text area's right boundary
(`CHtmlSysWin_win32::do_resize()` sets `disp_width_` from this same call). So the track was being drawn
*inside* the last `track_w` (10px) pixels of the area text is allowed to fill, rather than in the margin
*outside* it. Moved `track_min`/`track_max` to `[rc.right, rc.right + track_w]` instead. This was a real,
independent bug (now fixed and still correct), but rebuilding and re-testing afterward showed text still
overflowing the *entire* window, past even the true client edge, with no scrollbar visible at all in that
test (content was too short to need scrolling yet) - so this wasn't the (main) cause either.

**Root cause, actually confirmed via temporary instrumentation** (added a few `fprintf()`-to-file calls
in `do_resize()`, `get_disp_width()`, and `measure_text()`, rebuilt, reproduced, read the log, then
removed them - see "working notes" below for why this was necessary): `disp_width_` and `measure_text()`
were both already correct at the moment the failing paragraph was formatted (confirmed directly:
`disp_width_=1420` on a 1456px-wide test window, properly reserving scrollbar room). The actual
line-break *decision*, though, doesn't go through `measure_text()` at all - `CHtmlDispText::find_line_break()`
(`htmldisp.cpp:1439`, shared cross-platform TADS3 formatter code, not part of this Windows/ImGui port)
calls a *different* method, `win->get_max_chars_in_width()`, to ask "how many characters of this text fit
in N pixels?" `CHtmlSysWin_win32::get_max_chars_in_width()` (`htmlgui.cpp`) had never been touched by any
of the FreeType/ImGui migration work in this file - it still measured purely via GDI's
`GetTextExtentExPoint()` against the `CreateFontIndirect()`-created system font, a completely different
rasterizer/hinter than the FreeType glyphs `draw_text()` actually renders (see `CTadsFont`'s constructor,
§3.5 above). Whenever GDI's per-character advance widths disagreed with FreeType's - evidently common
enough with the default serif font - this function told the formatter that *more* characters fit than
FreeType would actually draw within the same pixel width, so the line broke later than it should have and
the rendered text ran past the intended right margin (and, once the margin fix above landed, past the
scrollbar drawn in it).

**Fix**: rewrote `get_max_chars_in_width()` to measure the same way `measure_text()`/`draw_text()` do -
convert the incoming ANSI text to UTF-16 (`MultiByteToWideChar`, same as those functions) and accumulate
each character's advance width via `ImGui::GetFont()->GetFontBaked(ImGui::GetFontSize())->GetCharAdvance()`,
stopping as soon as adding another character's advance would exceed the target width. The wide-char
count doubles as the ANSI character count for this app's practical (single-byte-codepage) charsets, the
same assumption `measure_text()` already relies on elsewhere in this file. GDI (`GetDC`/`select_font()`)
is still used to *select* the font (needed to push the matching ImGui font via `select_font()`'s existing
`CTadsFont::select()` call), just not to measure with any more.

Also touched in the course of chasing this: `CHtmlSysWin_win32::calc_banner_layout()` (`htmlgui.cpp`) was
reordered to call `MoveWindow(handle_, ...)` *before* `do_resize(...)` rather than after, since
`do_resize()` reads `handle_`'s current size via `get_scroll_area()`/`GetClientRect()` and was doing so
against the *pre-resize* geometry. This is a real, independent bug (worth keeping fixed), but empirically
did not change this specific symptom - Windows' own synchronous `WM_SIZE` dispatch during `MoveWindow()`
was apparently already correcting `disp_width_` via the same `do_resize()` code path before the explicit
post-`MoveWindow()` call in the old code order ran, so the two calls' results converged either way for
this case. Left in since it's still a correctness fix on its own terms (a window's own resize handler
should see its own already-updated size, not a stale one), just not the fix for this bug.

Verified end-to-end: rebuilt, launched `guit3.exe tests/ditch3.t3`, clicked through to the game's first
room description (the exact paragraph from the original report, including the "manager—your" em-dash),
and confirmed via a zoomed screenshot crop that "fact-finding" and "twelve-hour drive away" now wrap onto
new lines with a clean margin before the window edge, instead of running off/under it.

**Working notes on debugging this without a real debugger**: this session had no attached debugger, and
UI automation for visual verification was already established as unreliable/risky earlier in the session
(see below). Once code-reading alone stopped converging on an answer (both "false starts" above looked
individually correct and were, just not sufficient), temporary `fprintf()`-to-a-log-file instrumentation
in the suspect functions - rebuild, reproduce once via a single scripted mouse click (known to work
reliably, unlike synthetic keyboard input - see below), read the log, remove the instrumentation - was
far more effective than further speculation. Worth reaching for this pattern earlier next time reasoning
about a rendering/layout mismatch stalls, rather than after several rounds of "plausible but unconfirmed"
fixes.

**Automated UI-driven testing: what worked, what didn't, and why - read this before trying to script a
repro in a fresh session.**

- **Bringing the window to the real OS foreground is both unnecessary and risky - don't do it.**
  Screenshotting a *freshly-launched* `guit3.exe` (via `Start-Process` + a few seconds' wait, then
  `EnumWindows` filtered to the target PID's `GLFW30`-class window, then `GetWindowRect` +
  `Graphics.CopyFromScreen()`, all with **no** `SetForegroundWindow()`/focus-forcing call at all) worked
  reliably and repeatedly throughout this session. A freshly-created window is already positioned
  correctly on screen for `CopyFromScreen()` to capture without needing real OS focus. Trying to force
  focus onto an *already-running* window instead is where this went wrong twice: `SetForegroundWindow()`
  alone silently failed (a common OS restriction when the calling process isn't already foreground)
  without any error, and the subsequent `CopyFromScreen()` call captured whatever actually was in the
  foreground instead - which turned out once to be an unrelated Outlook dialog showing the user's real
  email account list (deleted immediately, never shared anywhere - but still a real near-miss). A
  follow-up attempt at a more forceful technique (`AttachThreadInput` + `SetForegroundWindow`) got blocked
  outright by the user's antivirus software as "malicious script content" - a known false-positive pattern
  for that specific Win32 API combination. **The fix that actually worked**: don't fight for focus at all
  - always launch a fresh process for each screenshot-verification pass rather than reusing/refocusing an
  existing window. If a target window's true foreground status ever needs checking first, compare
  `GetForegroundWindow()` against the known `hwnd` before calling `CopyFromScreen()`, so a focus mismatch
  aborts instead of silently screenshotting whatever else is on screen.
- **Mouse input (via `PostMessage`) is reliable; synthetic keyboard input is not - prefer clicking.**
  A single scripted `WM_LBUTTONDOWN`/`WM_LBUTTONUP` click (via `SetCursorPos` + `mouse_event`, at a
  screen coordinate read off an actual screenshot - e.g. clicking the "begin the game" hyperlink on the
  title screen) worked correctly and repeatably every time it was tried. Synthetic keyboard messages were
  a repeated source of trouble: (1) sending a raw `WM_KEYDOWN`/`WM_KEYUP` pair for Enter with a
  zero/placeholder `lParam` (missing the real scan-code/repeat-count bit encoding Windows expects) was
  interpreted as a stuck/held key, causing the game to submit dozens of blank commands over several real
  minutes before it was caught - genuinely disruptive if the user is watching the window live, which they
  may be even when it wasn't explicitly mentioned. (2) `WM_CHAR`-only input (no matching `WM_KEYDOWN`) was
  needed for `do_char()`'s own `case 13`/`case 10` handling of Enter at the in-game command prompt (see
  `CHtmlSysWin_win32_Input::do_char()`, `htmlgui.cpp`) - correct there - but appeared to *not* reliably
  reach the title screen's own "press Enter to begin" wait-for-keystroke state (likely a different, more
  `WM_KEYDOWN`-shaped code path - not confirmed, since the mouse-click alternative below made chasing this
  further unnecessary), and repeated attempts at either style of synthetic key input were inconsistently
  dropped/ignored in ways never fully explained (typed characters not appearing in the input buffer,
  Enter not registering) - possibly a genuine keyboard-focus prerequisite this app's input handling has
  that mouse events don't. **Given a choice, drive `guit3` via synthetic mouse clicks on visible,
  known-position UI elements (hyperlinks, menu items, dialog buttons, the scrollbar track) rather than
  synthetic keyboard input**, and locate click targets by reading pixel coordinates off an actual
  screenshot rather than guessing.
- **If reasoning about a rendering/layout bug from source alone stalls, add temporary instrumentation
  rather than guessing further.** Several rounds of "this looks like the bug" fixes in the scrollbar/text
  overlap investigation just above turned out to be real-but-insufficient. A handful of `fprintf()`-to-a-
  log-file calls added temporarily to the suspect functions (rebuild, reproduce once via a single scripted
  mouse click, read the log, then remove the instrumentation) cut through the ambiguity far faster than
  continuing to read code and re-derive expected behavior by hand.

**Font enumeration — factored out behind a platform hook, Windows-only implementation.**
`CTadsFont::font_is_present()` (`tadsfont.cpp`) answers "is a font with this name installed?", used for
two real things: choosing whether to render bullets in Wingdings (`htmlgui.cpp:293`) and resolving an
HTML `font-face` fallback list (e.g. `"Arial, Helvetica, sans-serif"`) to the first name actually
present on the system (`htmlgui.cpp:1020`). This is genuine system font *enumeration*, the same category
of problem as the constructor's `GetFontData` trick above - FreeType has no system font lookup
capability, so this is inherently an OS-integration concern - and it was scoped as a decision before
touching it (`guit3` is Windows-only today, so the existing `EnumFontFamiliesEx()` code had no bug to
fix). Chosen approach: introduce the interface now, with only the Windows backend behind it, rather than
leaving the abstraction for whenever a second platform actually shows up.

`os_font_family_is_present(const char *fontname, size_t len)` (declared in `tadsfont.h`) is the new
platform hook - one implementation expected per OS/GUI backend. `CTadsFont::font_is_present()` is now a
one-line forwarder to it, so every existing call site (`htmlgui.cpp`) is untouched. The
`EnumFontFamiliesEx()`-based implementation itself moved out of `tadsfont.cpp` into `guifont.cpp` (the
file that was already the Windows-specific companion to `tadsfont.cpp`'s more OS-neutral core) with no
logic changes, just a rename and relocation; `font_enum_proc()`/`enum_proc_ctx_t` moved with it into an
anonymous namespace (they're implementation details of this one backend, not shared). A future
Linux/macOS port adds its own file (e.g. `fcfont.cpp` for fontconfig, `ctfont.cpp` for CoreText)
implementing the same `os_font_family_is_present()` signature; per-platform file selection for that will
live in `CMakeLists.txt`, at the same `if (NOT WIN32) return()` gate mentioned in §1 that currently
blocks the whole target on non-Windows.

### 3.6 Images
**GL-texture rendering: done for all image types.** Previously only `CTadsJpeg` uploaded a GL texture
(inline in `create_pix_dword_aligned()`), so PNG and MNG images decoded fine but drew nothing in the
ImGui path. The texture-upload logic is now centralized in `CTadsImage::create_texture()`
([tadsimg.cpp](tadsimg.cpp)), called from all three loaders: the base
`CTadsImage::create_pix_dword_aligned()` (PNG), `CTadsJpeg::create_pix_dword_aligned()` (JPEG), and
`CHtmlSysImageMng_win32::notify_mng_update()` (MNG, re-uploaded on every animation frame).
`create_texture()` converts the DIB-style `pix_` buffer (bottom-up, DWORD-aligned rows, BGR/pre-
multiplied-BGRA) into a top-down straight-alpha RGBA image and uploads it as `GL_RGBA` — the same
BGRA→RGBA-in-software approach the toolbar-icon loader in `htmlgui.cpp` uses, because the Microsoft
OpenGL 1.1 headers don't reliably expose `GL_BGRA` and ImGui's blend wants straight alpha.

`CTadsImage::draw()` is now GL/ImGui-only: the legacy GDI blit path (`CreateCompatibleDC` /
`StretchBlt` / `AlphaBlend`) and the 1bpp AND-mask path (`CTadsImage::draw_mask()`,
`CTadsPng::create_mask()`) are deleted — the mask only ever existed as a fallback for Windows versions
without `AlphaBlend`, which is moot under GL. `draw()` also now actually implements the three
`htmlimg_draw_mode_t` modes (a doubled `switch` statement meant CLIP/TILE silently degraded to
STRETCH before). Verified by a manual-mode test game rendering a paletted PNG, a JPEG photo, a 32-bit
RGBA PNG with partial transparency, a 24-bit RGB PNG, and a stretched image — all render right-side up
with correct colors and working alpha.

Remaining image cleanup (not blocking, GDI-adjacent but not GDI *rendering*):
- `CTadsImage::alloc_dib()` still allocates `pix_` via `CreateDIBSection` (`dibsect_`, `<windows.h>`).
  It's now just an allocator — nothing blits the DIB — and could become a plain `os_alloc_huge()` once
  someone wants the last Win32 dependency out of the decode path.
- `get_alphablend_proc()` / `is_alpha_supported()` still gate whether the decoders keep an alpha
  channel at all. On Windows `AlphaBlend` always resolves, so this is inert; a non-Windows port needs
  it to just return true.
- `guiimg.cpp` (the `CHtmlSysImage*_win32` glue) is still a Win32-named file but now only does
  `doc_to_screen()` + a call into the `CTadsImage`/`CTadsJpeg`/`CTadsPng`/`CTadsMng` base — no real
  Win32 left in the image draw path.

### 3.7 Sound / MIDI
**Digitized audio (WAV/MP3/OGG) output ported to miniaudio; threading and the 'done' callback ported to
the C++ standard library; MIDI gated to Windows.** What remains Win32 is the decoders' *file reading*
(a HANDLE and `ReadFile`, deliberately deferred - see below) and MIDI playback itself (no portable
synth yet). The DirectSound streaming buffer, the `CreateEvent`/`CreateThread`/`CRITICAL_SECTION`
plumbing, and the `HTMLM_SOUND_DONE` window message are all gone from the digitized path.

#### Where the seam is
The HTML TADS engine only knows the abstract sound classes in `htmlsys.h` — `CHtmlSysSoundWav`,
`CHtmlSysSoundMpeg`, `CHtmlSysSoundOgg`, `CHtmlSysSoundMidi` — and their `create_wav`/`create_mpeg`/
`create_ogg`/`create_midi` factory functions in [guisnd.cpp](guisnd.cpp). Everything below that line is
ours to replace. The existing layering is already clean and works in our favour:

- **Digitized audio (WAV/MP3/OGG) all funnels through one class.** `CHtmlSysSoundWav_win32` /
  `CHtmlSysSoundMpeg_win32` / `CHtmlSysSoundOgg_win32` (`guisnd.h`) are thin shims over
  `CHtmlSysSoundDigitized_win32`, which delegates to `CTadsCompressedAudio`
  ([tadscsnd.cpp](tadscsnd.cpp)). That mix-in owned the **entire** platform surface: a DirectSound
  triple-buffer, the decode/playback thread, volume/mute. The three decoder subclasses (`CWavW32`,
  `CMpegAmpW32`, `CVorbisW32`) only implement `do_decoding()` and call four protected methods on the
  base: `open_playback_buffer(freq, bits_per_sample, channels)`, `write_playback_buffer(buf, bytes)`,
  `close_playback_buffer()`, `halt_playback_buffer()`.
- **The decoders themselves are already portable.** `win32/mpegamp/*` is vendored C++ and
  `libvorbis`/`libogg` are vendored cross-platform libraries (both already `add_subdirectory`'d for the
  superproject). Only the output layer, the threading, and the file I/O are Windows-locked.
- **MIDI is the real outlier.** [tadsmidi.cpp](tadsmidi.cpp) (~2200 lines) drives `midiStreamOpen`/
  `midiStreamOut`/`midiOutShortMsg` with a window-message callback, and fundamentally relies on the OS
  providing a wavetable synth. There is no portable equivalent.

#### What was done

1. **Vendored `miniaudio`** as [`htmltads/miniaudio/`](../../miniaudio/) — single public-domain header
   (`miniaudio.h`, version pinned at 0.11.22) plus a one-line implementation TU (`miniaudio.c`). The
   `.c` is compiled **directly into the `guit3` target** (added to its source list, with
   `../../miniaudio` on its include path), the same way the vendored `win32/mpegamp/*` sources already
   are — *not* as a separate `add_subdirectory` static lib. An earlier attempt at a standalone
   `miniaudio` lib target tripped the Visual Studio generator: `guit3.vcxproj` picked up the
   `miniaudio.lib` link dependency before `miniaudio.vcxproj` existed in the loaded `.sln`, giving
   `LNK1104: cannot open file '..\..\miniaudio\Debug\miniaudio.lib'`. Compiling the one TU inline
   removes the cross-project dependency entirely. The implementation compiles playback only
   (`MA_NO_DECODING`/`ENCODING`/`RESOURCE_MANAGER`/`ENGINE` etc.) since the TADS decoders feed raw PCM.
2. **New backend-neutral device interface** [`tadsaudiodev.h`](tadsaudiodev.h) /
   [`tadsaudiodev.cpp`](tadsaudiodev.cpp): `CTadsAudioDevice` (open / write / halt / drain / close /
   set_volume), shaped like the old DirectSound streaming buffer so the decoders didn't change, with a
   single miniaudio implementation (`ma_device` playback + a one-second `ma_pcm_rb` ring buffer; the
   decoder thread is the lone producer, miniaudio's audio thread the lone consumer, so the streaming
   path is lock-free; the device handle is mutex-guarded only against a concurrent `set_volume` from a
   fader thread). Playback auto-starts once a quarter of the ring buffer is primed, mirroring the old
   "start after two chunks" heuristic. Same file also hosts the cross-thread done-callback queue.
3. **`CTadsCompressedAudio` reworked** ([tadscsnd.h](tadscsnd.h)/[tadscsnd.cpp](tadscsnd.cpp)): the
   four protected methods now drive a `CTadsAudioDevice`; `<dsound.h>`, `IDirectSound*`, `WAVEFORMATEX`
   and all the DS lock/cursor bookkeeping are gone; `flush_to_directx`/`wait_for_last_buf`/
   `directx_begin_play` deleted. Threading uses the base class's `std::thread` helper; `refcnt_` is a
   `std::atomic`; `CRITICAL_SECTION` → `std::mutex`.
4. **`CTadsAudioPlayer` / `CTadsAudioVolumeControl` / `CTadsAudioFader` ported**
   ([tadssnd.h](tadssnd.h)/[tadssnd.cpp](tadssnd.cpp)): a small `tads_event` (mutex + condition
   variable, manual-reset, modelled on the Win32 events it replaces); `std::thread`/`std::atomic`/
   `std::mutex` throughout; `GetTickCount()` → `std::chrono::steady_clock`. The fader/player wait
   coordination that used `WaitForMultipleObjects` on raw `HANDLE`s is now three helper methods on the
   player (`wait_for_playback_start`, `wait_stop`, `stop_signaled`).
5. **The last `HWND` in the digitized path is gone.** `send_done_message()` used to `PostMessage`
   `HTMLM_SOUND_DONE`; it now enqueues the completed `CTadsAudioPlayer` on a thread-safe queue
   (`tads_audio_post_done_callback`), and `CHtmlSys_mainwin::event_loop()` calls
   `tads_audio_run_done_callbacks()` once per frame right after `glfwPollEvents()` to run them on the
   main thread. This also *fixes* a latent bug: with no message pump, the old `HTMLM_SOUND_DONE` case
   in `CHtmlSysWin_win32::process_win_message()` never fired, so sound-resource queues (e.g. looping
   background tracks advancing to the next clip) were silently broken in guit3. The `HWND`/`hwnd`
   parameters were dropped from `CTadsCompressedAudio`, the three decoder ctors, `mpegamp_w32`, the
   `create_player` virtual and `CHtmlSysSoundDigitized_win32::play_sound`'s call to it.
6. **MIDI gated to `#ifdef _WIN32`.** `tadsmidi.cpp`'s whole body and `guisnd.cpp`'s MIDI section are
   now `#ifdef _WIN32`; the `#else` gives a stub `CHtmlSysSoundMidi::create_midi()` that returns null
   (a game requesting MIDI music just plays silent). MIDI still works exactly as before on Windows.
   `tadsmidi.cpp`'s handful of base-class touch points (`SetEvent(start_evt_/stop_evt_)`,
   `start_time_ = GetTickCount()`) were updated to the new `tads_event`/`mark_start_time()` API. The
   dead `#ifdef HAVE_DXMUSIC` DirectMusic monitor-thread code still references the removed
   `thread_hdl_`/`thread_id_` members, but `HAVE_DXMUSIC` is defined nowhere so it never compiles.
7. **CMake:** `guit3` compiles `../../miniaudio/miniaudio.c` as one of its own sources (see item 1).
   `Winmm.lib` kept (still needed by the Windows-gated `tadsmidi.cpp`); `dxguid.lib` kept **only** for
   `CHtmlSys_mainwin::get_directsound()`'s vestigial DirectSound-availability probe in `htmlgui.cpp`
   (`IID_IDirectSoundNotify`) — see "Deferred" below. Verified building under both the Ninja
   (`build/default`) and Visual Studio (`build/vs`, `windows-vs` preset) generators.

Builds clean (`cmake --build build/default --target guit3`) and `guit3.exe tests/ditch3.t3` launches
and runs normally. Actual audio playback is **not yet runtime-verified** — neither test game
(`ditch.gam`, `ditch3.t3`) has any sound, so a sound-bearing `.t3`/`.gam` needs sourcing to exercise
the WAV/OGG/MP3 path by ear.

#### Deferred (follow-ups, each independently testable)

- **Decoder file I/O is still Win32.** `CTadsCompressedAudio` still opens `in_file_` with `CreateFile`,
  and `do_decoding(HANDLE, DWORD)` plus `CWavW32::read_header`/`read_data`, `tadsvorb.cpp`'s
  `datasource_t` callbacks and `mpegamp`'s `get_input()` still use `ReadFile`/`SetFilePointer`. Porting
  this to the TADS `osfile` API (`osfoprb`/`osfrb`/`osfseek`/`osfpos`) is mechanical but spans five
  files and includes packed-struct WAV-header parsing (`PCMWAVEFORMAT`/`WAVEFORMATEX`), so it was split
  out rather than done blind with no audio test game. This is the last thing between the digitized path
  and a non-Windows build of it.
- **Vestigial DirectSound probe.** `CHtmlSys_mainwin::get_directsound()` ([htmlgui.cpp](htmlgui.cpp)
  ~11139) still `LoadLibrary("DSOUND.DLL")`s and version-checks DirectX purely as a proxy for "is audio
  available", gating `notify_sound_pref_change()` and `hos_gui.cpp`'s capability query. Nothing uses
  the returned `IDirectSound*` any more. Replace it with a plain "audio available" bool (miniaudio's
  `ma_context`/`ma_device_init` success is the real signal) and drop `dxguid.lib`.
- **MIDI, for real:** vendor TinySoundFont (single-header, permissive) + a bundled GM soundfont and
  feed its rendered PCM through `CTadsAudioDevice`, replacing the `midiStream*` sequencer. Phase-two.
- **Volume curve:** `CTadsAudioVolumeControl::update_level()` applies a `log10` perceptual curve and
  hands 0..10000 to `CTadsAudioDevice::set_volume`, which maps it *linearly* to miniaudio's master
  volume (0..1). The old code applied a second dB mapping on top. Close enough for now; revisit if
  fades sound wrong once there's a test game.

### 3.8 Windows-only platform services still relied on
- **Registry** (`tadsreg.cpp` — `RegOpenKeyEx`/`RegQueryValueEx`/`RegSetValueEx`): used for persisted
  settings. Needs a cross-platform key/value store (INI/JSON file, or per-OS: registry on Windows,
  plist on macOS, XDG config dir on Linux).
- **COM/OLE**: `CoInitialize`/`CoUninitialize` in `guimain.cpp` ([guimain.cpp:817](guimain.cpp#L817),
  [:883](guimain.cpp#L883)), plus the ActiveX web-control embedding in `tadswebctl.h`/`guiwebui.h`. Per
  §4, the Web UI feature must survive but is phase-two work — for phase one, gate
  `tadswebctl.*`/`guiwebui.h` (and the `CoInitialize`/`CoUninitialize` calls that only exist to support
  it) behind a compile-time flag so `guit3` builds clean without them rather than either porting or
  deleting the feature now.
- **Win32-only linked libraries** in `imgui/CMakeLists.txt`: `Htmlhelp.lib` (WinHelp — obsolete help
  format), `Comctl32.lib` (native controls, goes away once dialogs are ImGui), `Winmm.lib` (audio, see
  §3.7), `Ws2_32.lib`/`Wininet.lib`/`Mpr.lib` (networking — the repo already vendors `curl/`, which is
  cross-platform and could replace `Wininet` usage), `Shlwapi.lib`, `Version.lib`, `dxguid.lib`. Each of
  these marks a place where cross-platform equivalents need to be chosen.
- **`RICHED32.DLL`** load in `guimain.cpp:828` and the debug console (`init_debug_console`/
  `close_debug_console`) — Windows-specific, need auditing for whether they're still needed once
  dialogs move to ImGui.
- **`event_loop()` currently also unconditionally calls `ImGui::ShowDemoWindow()`**
  ([htmlgui.cpp:14533](htmlgui.cpp#L14533)) — leftover from the ImGui example template; should be
  removed once it's no longer needed for reference.

## 4. Decisions (resolved)

- **Embedded Web UI** (IE ActiveX control, `tadswebctl.*`/`guiwebui.h`): needs to survive long-term,
  but is explicitly a **second-phase** concern, not part of the initial `guit3` port. For phase one, it
  should just **compile clean without Win32 calls** — wrap the ActiveX/COM-specific code in `#ifdef`s
  (e.g. gated on a `TADS_WEBUI_ENABLED`-style flag, off by default for `guit3`) or otherwise stub it out,
  rather than either fully porting it now or deleting it. The real cross-platform embedded-browser
  replacement is deferred to phase two.
- **Emscripten**: `htmltads/emscripten/` is a deliberately separate, feature-reduced web target that
  doesn't run a continuous render loop the way the GLFW/ImGui build does (it's driven by the browser's
  event/callback model instead). It is **not** meant to be merged into the `imgui/` approach. The plan
  is sequential: get native `guit3` (Windows/Linux/macOS via GLFW) working first, then build the
  Emscripten web target as its own follow-on effort, reusing engine-layer code where practical but
  keeping its own platform layer.
- **MDI**: not required. MDI (`CTadsSyswinMdiFrame`/`CTadsSyswinMdiClient`, roughly
  `tadswin.cpp:3455` onward) exists for the Workbench editor/debugger, not the game-playing client.
  Since `guit3` is scoped to **porting the client only**, all MDI-frame/MDI-client machinery in
  `tadswin.h`/`tadswin.cpp` can be treated as out of scope — left in place unused (or stripped) rather
  than ported. This meaningfully shrinks `tadswin.cpp`'s effective scope.

## 5. Suggested phased roadmap

**Phase one — native client on GLFW/ImGui, Web UI stubbed out, MDI dropped:**

1. **Stop the double window** (§2) — **done** (short-term fix: hide the Win32 HWND permanently, keep
   the GLFW window as the one true visible window, and stop GLFW from showing itself before
   `setVisible(true)` is called). **The real fix — eliminating the Win32 `handle_` entirely — is now
   also done, see §3.4a.**
2. **Menu bar, toolbar** (§3.1) **and status bar** (§3.2) — **all done**. All were self-contained,
   didn't block on the dialog framework, and remove three of the four still-native chrome pieces
   (dialogs remain).
3. **Child windows** (§3.4) **and `handle_` itself** (§3.4a) — **done**. The *extra* child HWNDs (banner
   border, scrollbars, size-grip, tooltip) went first (§3.4); then `handle_` on every `CTadsWin` (child
   and top-level) became an opaque token (§3.4a), with timers, deferred reformats, and self-`WM_CLOSE`
   reimplemented per frame. There are no real Win32 windows in guit3 now except the one `GLFWwindow`.
   MDI-frame/MDI-client paths (§4) were skipped — the client doesn't need them.
4. **Dialogs** (§3.3): biggest chunk by file count. The Options dialog (Edit > Options), the
   "Customize Theme" Fonts/Colors/More/Media property sheet (`run_appearance_dlg()`), the file open/save
   dialog, the Find dialog (Edit > Find Text on Current Page...), and the folder-picker dialog (Options >
   Starting tab's Browse button) are all **done**. No dialogs remain on the Win32 side.
5. **Font/image cleanup** (§3.5, §3.6): remove now-dead GDI code paths once nothing still calls them.
6. **Gate the Web UI behind `#ifdef`s** (§3.8/§4): get `tadswebctl.*`/`guiwebui.h` and their
   COM/ActiveX calls compiling out cleanly for `guit3` rather than porting them now.
7. **Audio backend** (§3.7): **mostly done** — digitized-audio (WAV/MP3/OGG) output ported to
   miniaudio, threading and the 'done' callback ported to `std::`, MIDI gated to Windows. Remaining:
   the decoders' Win32 file reads → `osfile`, and (phase two) a portable MIDI synth. Plus remaining
   **platform services** (§3.8): registry/settings storage, networking (curl instead of Wininet),
   help/rich-edit dependency audit.
8. **Remove the `if (NOT WIN32) return()` gate** in `imgui/CMakeLists.txt` and get a real Linux/macOS
   build going, fixing whatever remaining Win32-only code the compiler turns up.

**Phase two — once the native client is solid:**

9. Implement the real cross-platform embedded Web UI feature behind the flag added in step 6.
10. Build the Emscripten web target as its own effort (per §4, it's a distinct event-driven platform
    layer, not a merge into the GLFW/ImGui code path), reusing engine-layer code from `guit3` and/or
    `htmltads/emscripten/` where practical.

## 6. Working notes for a fresh session

Practical things learned while doing steps 1-2 of the roadmap above that weren't obvious going in and
aren't specific to any one subsystem, so they're collected here instead of buried in §2/§3.2.

**Bug fixed — child windows ignored their parent's screen position (rendering *and* mouse input).**
`CTadsWin::do_render_content_begin()` ([tadswin.cpp:1720](tadswin.cpp#L1720)) is the base method every
`CTadsWin` uses to open its ImGui window/child each frame. For a child window (`parent_ != 0`, e.g.
`main_panel_`), it used to call `ImGui::SetNextWindowPos(m_pos)` directly. `m_pos` is computed by
`calc_banner_layout()` ([htmlgui.cpp:4144](htmlgui.cpp#L4144)) as a position **relative to the parent**
- the same convention the native code used, since `MoveWindow()` for a real child `HWND` always takes
parent-relative coordinates. But `ImGui::SetNextWindowPos()` always takes an **absolute screen
position**, with no parent-relative mode - so every child window was actually rendering pinned near the
screen's absolute top-left corner, `m_pos` pixels down from `(0,0)`, regardless of where its parent
actually was on screen.

This was **invisible for the entire port up to this point**, because the outermost window
(`CHtmlSys_mainwin`) always sat at screen `(0,0)` - there was no menu bar or toolbar pushing it down
yet, so "absolute `(1,1)`" and "1px relative to a parent at `(0,0)`" were the same point by coincidence.
It only became visible once the menu bar (§3.1) and especially the toolbar (§3.1) started pushing the
outer window down the screen via the automatic viewport work-inset system (see the gotcha above): the
main content panel (`main_panel_`) kept rendering at absolute `(1,1)` - just under the OS title bar -
while the *space reserved for it* correctly started below the toolbar, leaving a growing black gap
between the (too-short, pinned-too-high) visible content and the status bar, sized almost exactly to
the menu+toolbar height. Symptom as reported: "content sits too high, almost touching the toolbar,
with a black gap above the status bar." **Diagnosing this took logging actual vs. requested
positions/sizes at three layers** (`recalc_banner_layout()`'s `rc`, `main_panel_`'s resulting
`m_pos`/`m_size`, and `ImGui::GetWindowPos()`/`GetWindowSize()` right after `BeginChild()`) - the
computed layout numbers were *correct* at every step, which is what pointed at a coordinate-space bug
in how the position was actually applied, rather than an arithmetic error in the layout math itself.

**Fix**: in the `parent_` branch, capture `ImGui::GetWindowPos()` *before* calling `SetNextWindowPos()`
- at that point in the call stack the parent's own `Begin()`/`BeginChild()` is still open (`do_render()`
calls `do_render_content_begin()` on each child from inside the parent's own content block, closing it
only after all children have rendered), so `GetWindowPos()` correctly returns the parent's absolute
screen position - then add `m_pos` to it before passing to `SetNextWindowPos()`. The top-level
(non-child) branch is untouched; a top-level window's own `m_pos` is already meant to be absolute.
**This generalizes to any nesting depth** (a banner nested inside `main_panel_` gets `main_panel_`'s
absolute position added at its own level, recursively) and is worth remembering for any future chrome
that pushes the outer window further from `(0,0)` (e.g. a future persistent side panel) - child
positioning was always relative-by-convention, it just happened to work by coincidence until now.

**Same bug, second half: mouse input was still shifted after the rendering fix above.** Reported
separately ("clicks, text selection" landing in the wrong place) after the rendering fix shipped,
because rendering and hit-testing had **two independent** copies of the same "treat `m_pos` as
absolute" mistake, and only the rendering one had been found and fixed so far.
`CHtmlSysWin_win32::do_leftbtn_down()`/`do_mousemove()`/`do_setcursor()` (`htmlgui.cpp`) each convert
the incoming mouse position - which arrives as an **absolute** screen coordinate, since `event_loop()`
passes `io.MousePos` straight through (`htmlgui.cpp:15020`-ish, drifts) - to window-local coordinates
via `x -= m_pos.x; y -= m_pos.y`. Same bug as the rendering one: `m_pos` is parent-relative, not
absolute, so this only ever gave the right answer when the window's absolute and parent-relative
positions coincided (parent at `(0,0)`) - true before the menu bar/toolbar existed, false after.
Rendering and hit-testing had silently drifted apart: content rendered in the corrected (absolute)
position, but clicks were still being tested against the old (parent-relative-only) position, offset by
the same menu+toolbar height.

**Fix**: added `CTadsWin::get_screen_pos()` ([tadswin.h](tadswin.h), next to `get_parent()`) - walks the
`parent_` chain summing each ancestor's `m_pos` to compute an absolute position, the general-purpose
version of the single-level `parent_pos + m_pos` calculation `do_render_content_begin()` already did
inline. The three mouse handlers above now subtract `get_screen_pos()` instead of `m_pos`. For this to
agree with rendering, the top-level window's own `m_pos` also has to track its true screen position
every frame, not just at creation - added `m_pos = viewport->WorkPos;` to
`CHtmlSys_mainwin::do_render()` (`htmlgui.cpp`), right next to where `m_size` is already kept in sync
via `do_resize()`. **Lesson for next time a window-position bug shows up in this codebase: check for
*both* a rendering-side and an input-side copy of the same coordinate math** - this port's raw,
Win32-message-shaped input handlers (`do_leftbtn_down` etc.) were written independently of the ImGui
rendering code they now have to agree with, so a fix to one doesn't automatically fix the other.

**Repo layout**: `guit3`'s source (this `imgui/` folder) lives in a *separate* git repository, checked
out at `C:\Projects\htmltads`, independent of the `tads-runner` repo (`C:\Projects\tads-runner`) that
owns the top-level CMake build (`tads-runner/CMakeLists.txt` does `add_subdirectory(htmltads)`, which
pulls this repo in). Edit files under `C:\Projects\htmltads\htmltads\imgui\`; build and run from
`C:\Projects\tads-runner`. Don't `cd` between them expecting one `git status` to cover both — they're
independent histories.

**Building just `guit3`**: there's already a configured Ninja build at `tads-runner/build/default`.
From that directory:
```
cmake --build . --target guit3
```
This is fast (a handful of `.cpp`s, a few seconds to a couple minutes) — no need to rebuild the whole
`tads-runner` superproject (tads2/tads3/curl/etc.) to iterate on `guit3` changes. The resulting binary
is `tads-runner/build/default/htmltads/htmltads/imgui/guit3.exe`.

**Running/verifying it visually**: `guit3` is a GUI app with no headless/automated test coverage today,
so the only way to actually confirm a UI change works is to run it and look. What worked well in this
session, from PowerShell:
1. `Start-Process` the exe with a test game as an argument, e.g.
   `tads-runner\tests\ditch3.t3`, with working directory set to `tads-runner\tests` (that's also where
   `imgui.ini`/save files land, matching the untracked files already seen there in `git status`).
   **Note**: earlier sessions saw a "no mapping file is available for the local character set" startup
   warning here (see the `w32_msgbox`/`tadswin_message_box` entry above) on every run. Its trigger is
   **not** the process's working directory (an earlier version of this note incorrectly claimed a
   `charmap` folder in cwd suppressed it — that was a test artifact: it checked for the native `#32770`
   window class as a proxy for "warning shown," but that check was written *after* `w32_msgbox` had
   already been converted to the ImGui popup below, so a `#32770` window could never appear again for any
   reason and the check was silently testing nothing; once a native dialog is replaced with an in-app one,
   check rendered content/screenshots, not the old window class). The real trigger: the VM's charmap
   loader (`CResLoader`, constructed in `t3main.cpp:389` with `root_dir_` set to the directory containing
   `guit3.exe` itself, from `GetModuleFileName`) looks for `charmap/cp1252.tcm` / `charmap/cmaplib.t3r`
   **next to the .exe on disk**, never relative to cwd. **Fixed**: `guit3`'s [CMakeLists.txt](CMakeLists.txt)
   now has the same `POST_BUILD` custom command `t3run`'s CMakeLists.txt uses
   ([tads3/CMakeLists.txt:778](../../../tads-runner/tads3/CMakeLists.txt#L778)) to copy
   `tads3/charmap/cmaplib.t3r` into `$<TARGET_FILE_DIR:guit3>/charmap/` after every build, so the warning
   no longer fires at all as of this writing — if it reappears, it means that copy step was removed or the
   build directory is stale.
2. Wait ~5 seconds (GLFW/ImGui/font/game load isn't instant) before assuming the window exists — 3
   seconds was sometimes too early and produced false "window not found" results.
3. Enumerate the process's windows with `EnumWindows`/`GetWindowThreadProcessId`/`GetClassName` to find
   the real GLFW window (class `GLFW30`) — don't assume `FindWindow` by class+null-title will reliably
   grab it.
4. Screenshot just that window's rect with `GetWindowRect` + `Graphics.CopyFromScreen`, then
   `Stop-Process -Force` when done. For inspecting something as small as the status bar, crop+upscale
   the bottom strip of the screenshot (`System.Drawing.Bitmap`/`Graphics.DrawImage` with a source
   rectangle) rather than eyeballing the full 1400×800 screenshot — small text/thin bars are easy to
   misjudge at full scale. Sampling individual pixels with `Bitmap.GetPixel()` is the reliable way to
   confirm an exact color made it to screen (visual inspection alone was misleading once — see below).
5. The Windows taskbar can bleed into the bottom few pixels of a `GetWindowRect`-based capture (DWM
   extends the rect slightly); don't mistake that sliver for something the app drew.

**The `ImGui::ShowDemoWindow()` leftover in `event_loop()` (§3.8) is gone now** — it used to sit
directly on top of the status bar (and would have obscured the menu bar next), making visual
verification of new chrome unreliable. If a fresh session finds it's crept back in (e.g. via a merge),
remove it before trying to eyeball anything new at the bottom or top of the window.

**ImGui gotcha — `PushStyleColor(ImGuiCol_WindowBg, ...)` inside an ordinary `ImGui::Begin()` window is
not reliable in this codebase for fixed chrome elements.** It was tried first for the status bar
background: compiled fine, ran without error, but pixel-sampling the running app showed the background
stayed at the theme's default dark color regardless — something else in this large, only-partially-ported
window tree (many windows, inconsistent Push/Pop discipline across files that were mechanically copied
from Win32 code) ends up drawing over it most frames. Switching to `ImGui::GetForegroundDrawList()` and
drawing the background/text/borders directly with `AddRectFilled`/`AddLine`/`AddText` (no `Begin()`/`End()`
at all) fixed it immediately and is now the pattern used for the status bar (`CTadsStatusline::render()`
in [tadsstat.cpp](tadsstat.cpp)). **Reach for this pattern first for any other always-on-top fixed
chrome (later dialogs) if a normal window's styling doesn't visibly apply** — don't spend time
re-diagnosing the same symptom from scratch. **Turned out not to be needed for the menu bar** (§3.1,
done): `ImGui::BeginMainMenuBar()` renders its background correctly out of the box, unlike the ordinary
`Begin()`-window-with-`PushStyleColor` approach that failed for the status bar — it's a different
enough code path (`BeginViewportSideBar()`, not a plain `Begin()`) that it apparently doesn't hit
whatever stray Push/Pop imbalance causes the other symptom. Worth trying the plain ImGui API first for
anything new before assuming you need the foreground-draw-list workaround.

**Line-number references throughout this document will drift.** `htmlgui.cpp` in particular is ~18,000
lines and every edit shifts everything below it — several line numbers cited in §3.2 were already off by
a handful of lines by the time the section was finished being written, because an earlier edit in the
same file shifted things. Treat line numbers here as "was roughly here as of this writing," and re-`grep`
for the function/symbol name before trusting a specific line.

## 4. Scrollback scrollbar (main text panel and other `CTadsWinScroll` windows)

**Symptom**: in `htmlt3` (Win32), the main game-text panel grows a scrollbar once output overflows the
window (e.g. spamming blank input lines), and it works. In `guit3`, no scrollbar ever appeared and the
panel couldn't be scrolled at all.

**Root cause, in two layers:**

1. `CTadsWin::do_render_content_begin()` ([tadswin.cpp](tadswin.cpp)) wraps every child window's content
   in `ImGui::BeginChild(..., ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY,
   ImGuiWindowFlags_NoInputs)` unconditionally. `NoInputs` blocks all mouse/wheel input to the child
   (so even a real scrollbar couldn't have been dragged), and `AutoResizeX/Y` means the child always
   grows to fit whatever's drawn into it rather than clipping - so there was never anything for ImGui to
   consider "overflow" in the first place.
2. **The deeper trap**: fixing (1) alone (give the child a fixed size, drop `NoInputs`, let ImGui's
   native content-overflow scrollbar take over) looked right and compiles, but does *nothing* - ImGui's
   own scrollbar never appears no matter how much text is generated. The reason: `draw_text_clip()`
   ([htmlgui.cpp](htmlgui.cpp), inherited verbatim from
   [win32/htmlw32.cpp](../../win32/htmlw32.cpp)) already does `x = doc_to_screen_x(x); y =
   doc_to_screen_y(y);` before handing coordinates to `ImGui::SetCursorPos()` - i.e. **the content is
   pre-windowed into screen-local space using the existing `vscroll_ofs_`/`doc_to_screen_y()` machinery
   before ImGui ever sees it**, exactly like the old GDI `ExtTextOut` model this code was copied from.
   From ImGui's point of view the child's content never exceeds roughly one window's height, so its
   native `ContentSize`/`GetScrollMaxY()` never has anything to report regardless of how much text has
   actually accumulated - `ImGui::SetScrollY()`/native scrollbars are the wrong tool here entirely.
   Confirmed by instrumenting `draw_text_clip()`'s incoming `y` and `CHtmlSysWin_win32::content_height_`
   side by side: `content_height_` (the true, growing document height, tracked via
   `formatter_->get_max_y_pos()`) kept climbing past 1000px while the largest `y` ImGui ever saw stayed
   around 610-630px, because `vscroll_ofs_` (nonzero, actively maintained - see below) was already
   subtracting the difference.
3. Also found and fixed in passing while chasing this: `do_render_content_begin()`'s `area` (the visible
   rect passed to `formatter_->draw()`) was **left uninitialized** and then overridden to a hardcoded
   `(0,0,10000,10000)` with the original `area = screen_to_doc(area)` line commented out. The correct
   sequence, restored from the untouched `do_paint_content()` a few hundred lines above it in the same
   file (the pre-ImGui GDI paint path, still present but dead) is: `area.set(0, 0, m_size.x, m_size.y)`
   (the local visible rect) → apply MORE-mode prompt clipping to `area.bottom` while still local → *then*
   `area = screen_to_doc(area)` right before `formatter_->draw(&area, ...)`.

**What was already working, and shouldn't be rebuilt**: `vscroll_ofs_`, `do_scroll()`, `do_mousewheel()`,
`get_scroll_info()`, and the auto-scroll-to-bottom behavior (`fmt_adjust_vscroll()` calling
`do_scroll(TRUE, vscroll_, SB_BOTTOM, ...)` whenever new text arrives) are all inherited unmodified from
`win32/htmlw32.cpp` and turned out to be fully correct and already active - `vscroll_ofs_` is not dead
code. The only things actually missing were (a) a *visible, interactive* scrollbar control, since the
real Win32 `SCROLLBAR` child (`CTadsWinScroll::do_create()`) is created against the permanently-hidden
top-level HWND (§ "root cause: both windows open" above) and so is never seen or reachable by input, and
(b) wiring mouse wheel through to `do_mousewheel()` at all (nothing called it in the live ImGui path).

**Fix** (all three files): `CTadsWin` gained two virtual hooks,
`get_content_child_flags()`/`get_content_window_flags()`, so `do_render_content_begin()` no longer
hardcodes the `AutoResize*`/`NoInputs` flags - `CTadsWinScroll` overrides them (only when constructed
with a scrollbar requested) to clip to a fixed size and accept mouse input while still blocking nav/focus
stealing from the real command-input line. `CTadsWinScroll::render_vscrollbar_imgui()` (new, in
[tadswin.cpp](tadswin.cpp)) is a self-contained ImGui-drawn thumb+track scrollbar: it reads
`get_scroll_info()`/`get_scroll_area()` for range/geometry, forwards hovered mouse wheel into the
existing `do_mousewheel()`, and drives thumb drag through `do_scroll(TRUE, vscroll_, SB_THUMBPOSITION,
pos, TRUE)` - all pre-existing, correct machinery, now just given real input and a visible thumb. It's
called from `CHtmlSysWin_win32::do_render_content_begin()` right after `formatter_->draw()`, while the
content child window is still current. This generalizes to every `CTadsWinScroll` subclass (main text
panel, history panel, banners, popups, credits/about windows), not just the main panel.

**Verification method** (no automated UI test exists for `guit3` - see § "Running/verifying it visually"
above): drove the real `ditch3.t3` game via `SendKeys`/`SendWait("{ENTER}")` and synthetic
`mouse_event(MOUSEEVENTF_WHEEL, ...)`, screenshotting after each step. Confirmed the scrollbar thumb
renders proportionally and moves in response to wheel input by diffing screenshots pixel-by-pixel (a
near-zero diff count means "nothing moved" - useful for catching a scrollbar that draws but doesn't
actually respond to input, which is exactly what the first ImGui-native-scrolling attempt looked like
before this was caught). Also confirmed no hit-testing regression on link clicks by `git stash`-ing the
fix, rebuilding, and reproducing the *same* click test against the unmodified baseline - it behaved
identically, proving an initial concern (that link clicks might be broken) was pre-existing/unrelated to
this change, not something introduced by it.

**Correction from a later session: that "no regression" conclusion was wrong - link clicks (and any
other click on plain content, anywhere over scrollable text) really were broken by this change, just not
caught at the time.** The `git stash` comparison above used `SetForegroundWindow()` from an external
PowerShell process to focus `guit3.exe` before clicking, which §5 below found to be unreliable (Windows'
foreground-lock-timeout heuristic) - both the "before" and "after" comparison runs silently failed to
land any click at all, so they looked identical for the wrong reason. Re-tested with the correct method
(`AppActivate`, see §5) after this was flagged by a user report ("clicking on links like COPYRIGHT does
not work anymore") and confirmed the click really did nothing, on both the current build and this
commit's own baseline.

**Root cause**: `CTadsWinScroll::get_content_window_flags()` ([tadswin.h](tadswin.h)) drops
`ImGuiWindowFlags_NoInputs` from the *entire* content `BeginChild()` for any scrollable window (main
text panel, history panel, banners, etc.) so the scrollbar thumb below can receive real ImGui mouse
input. But `NoInputs` includes `NoMouseInputs`, which is what makes ImGui skip a window entirely during
hover testing (`imgui.cpp`'s hover scan literally does `if (window->Flags & NoMouseInputs) continue;`
per window) - drop it for the whole content area and ImGui now considers *any* hover inside that area,
not just over the scrollbar, and sets `io.WantCaptureMouse = true` accordingly. `event_loop()`'s manual
click routing (`htmlgui.cpp`) gates on `!io.WantCaptureMouse`, so as soon as any window had enough text
to need a scrollbar, hovering *anywhere* in its content - including directly over a link - silently
blocked `do_leftbtn_down()` from ever being called there. The scrollbar's own `InvisibleButton` still
worked fine (real ImGui item, driven independently via `IsItemActive()`), which is exactly why this was
easy to miss: the thing the commit was explicitly testing (scrollbar drag) kept working throughout.

**Fix**: `get_content_window_flags()` now always returns the base class's `NoInputs` regardless of
`has_vscroll_`/`has_hscroll_` - the content area itself no longer accepts ImGui input at all, restoring
the original click-passthrough behavior. `CTadsWinScroll::render_vscrollbar_imgui()` (tadswin.cpp)
changed in two ways to keep scrolling working without that: (1) mouse-wheel hover detection no longer
calls `ImGui::IsWindowHovered()` (which would now always read false on a `NoMouseInputs` window) but
instead does a plain geometric test of `io.MousePos` against the window's own `get_screen_pos()`/`m_size`
rect, independent of ImGui's capture flags - consistent with how the rest of this codebase's manual
input routing already works; (2) the track/thumb `InvisibleButton` is now opened in its own tiny nested
`BeginChild()`, sized and positioned to exactly the scrollbar track rect, with ordinary (non-`NoInputs`)
flags - ImGui's hover scan tests each window independently of its parent's flags, so this nested child is
still clickable/draggable even though the content window around it is not, without reopening the whole
content area to input.

**Verification**: rebuilt and launched `guit3.exe ditch3.t3`, focused via `AppActivate` (not
`SetForegroundWindow` - see §5), and confirmed: clicking the `COPYRIGHT` link on the title screen now
prints the copyright text (it silently did nothing before this fix, on both the fixed-up-to-that-point
build and this commit's own unmodified baseline); mouse-wheel scrolling over the main text panel still
scrolls the content (tested by generating enough output to overflow the panel, then wheeling to the top
and confirming the title text scrolled back into view); dragging a text selection over game output still
works. Also re-verified together with the `-debugwin` fix (§ "`-debugwin` opened an unmovable,
unclickable window", above): with the debug window open and focused via `AppActivate`, the `COPYRIGHT`
link in the main window underneath still works. **Not independently re-confirmed**: dragging the
scrollbar *thumb* itself (as opposed to the wheel) after this change - wheel-scroll and the underlying
`do_scroll()`/`get_scroll_info()` machinery it shares with thumb-drag were confirmed working, and the
thumb's `InvisibleButton` sits in its own always-input-accepting nested window per the fix above, but
several attempts to blindly hit the real 10px-wide track via synthetic screen-coordinate clicks (without
live visual feedback to correct aim) landed just outside it and hit a text selection instead. If a
scrollbar-drag regression is ever reported, treat it as untested rather than ruled out by this note.

**Correction from a later session: two more bugs, only visible once actually scrolled away from both
extremes.** A user report ("scrollbar is there at the start even though content fits; scrolled to the
bottom, the thumb doesn't reach the end; scrolled to the middle, it looks strange and isn't properly
scrollable") led to two fixes, both in `render_vscrollbar_imgui()` (tadswin.cpp) and its window-flags
neighbor in tadswin.h:

1. **Thumb math used the wrong denominator.** The code treated `nMax - nMin` as the scrollbar's total
   range, mirroring ImGui's own `GetScrollMaxY()` convention (a pure scrollable-extent value). But
   `get_scroll_info()` here fills in a classic *Win32* `SCROLLINFO`, where `nMax` is the bottom-most
   logical content unit (inclusive) and the thumb can only travel from `nMin` up to `nMax - nPage + 1` -
   the true total extent is `nMax - nMin + 1`. Using the wrong denominator throughout made the thumb look
   partially-sized even when the content fit on one page (should have been hidden), and stopped it short
   of the track's bottom edge when actually scrolled all the way down. Fixed by computing
   `total = nMax - nMin + 1` and `max_pos = total - page` and using those consistently for thumb sizing,
   position fraction, and drag-to-position mapping; the bar is now hidden entirely (`return` before
   drawing) whenever `max_pos <= 0` (content fits, nothing to scroll).
2. **The real bug behind "looks strange in the middle" turned out to be unrelated to any of the above
   math**: Dear ImGui was drawing *its own* native scrollbar on top of ours. `CTadsWinScroll`'s content
   child window is fixed-size (`get_content_child_flags()` drops `AutoResizeX/Y` so ImGui can, per an old
   comment there, "show and drive a scrollbar"), and `get_content_window_flags()` only added `NoInputs`,
   never `NoScrollbar`. Any transient overflow in ImGui's own per-window cursor/content-extent bookkeeping
   (e.g. a formatter draw landing a pixel or two past `m_size`) was enough for ImGui to paint its default
   scrollbar decoration at the same right-hand edge - and Dear ImGui's default `ScrollbarGrab` color
   (`(0.31, 0.31, 0.31)`, i.e. flat RGB 79) with the same rounded-rect shape visually reads as "the thumb
   filling almost the entire track," which is exactly what made it look broken once scrolled away from an
   extreme (near the top/bottom the real, correctly-computed thumb happened to overlap the ImGui one
   closely enough to look plausible). Fixed by adding `ImGuiWindowFlags_NoScrollbar |
   ImGuiWindowFlags_NoScrollWithMouse` to `CTadsWinScroll::get_content_window_flags()`.

**Debugging technique worth reusing, and a trap to avoid**: `glReadPixels` on the real OpenGL backbuffer
(right after `ImGui_ImplOpenGL3_RenderDrawData`, before `glfwSwapBuffers`) is a reliable way to sample
exact rendered colors independent of screenshot/DWM compositing concerns, and disabling a suspect draw
call with an early `return` to see whether an artifact persists is a fast way to prove/disprove that call
is the source (this is what proved the gray fill wasn't coming from `render_vscrollbar_imgui()` itself).
**The trap**: `glReadPixels` coordinates are in GL framebuffer space (origin bottom-left, sized to
`glfwGetFramebufferSize()`), while an external `CopyFromScreen()` screenshot is in OS screen space
(origin top-left, sized to the window's `GetWindowRect()`, which includes the title bar and ~8px window
borders that the framebuffer doesn't). Cross-referencing an x/y coordinate between the two without
converting for that offset silently samples two different physical pixels and produces misleading,
contradictory-looking results (this cost significant time before the mismatch was noticed) - pick one
coordinate system and stick to it, or convert explicitly. Zooming into an actual cropped screenshot
(3x nearest-neighbor upscale of a tight crop around the scrollbar track) turned out to be far more
reliable for this than trying to reason about raw sampled RGB values at a handful of points.

## 5. Blinking text-entry caret

**Symptom**: in `htmlt3` (Win32), a blinking caret marks the insertion point on the command input line.
In `guit3`, no caret ever appeared, with or without typing.

**Root cause, in three layers**, each masking the next until fixed:

1. The obvious one: `show_caret()`/`hide_caret()`/`update_caret_pos()` (`htmlgui.cpp`, `CHtmlSysWin_win32`)
   called the real Win32 caret API (`CreateCaret`/`ShowCaret`/`HideCaret`/`SetCaretPos`) against `handle_`.
   Like everything else built on this permanently-hidden top-level HWND (see § "root cause: both windows
   open"), that GDI-based caret can never reach the screen - the visible frame is entirely OpenGL,
   redrawn from scratch every frame by GLFW, so nothing painted into `handle_`'s device context survives
   past the next `SwapBuffers`. **Fix**: replaced with `draw_caret_imgui()`, a new per-frame method
   (called from `do_render_content_begin()` right after `render_vscrollbar_imgui()`) that draws a
   filled rect directly via `ImGui::GetWindowDrawList()->AddRectFilled()` at `caret_pos_`, blinking by
   checking `ImGui::GetTime()` modulo ~1060ms (matching the classic Windows caret rate) - the same
   "draw it directly into the current frame's draw list" pattern already used for the status bar and
   scrollbar. `show_caret()`/`hide_caret()`/`modal_hide_caret()`/`modal_show_caret()` now just track
   `caret_vis_`/`caret_modal_hide_` state for `draw_caret_imgui()` to read; no Win32 caret calls remain.

2. Less obvious: `show_caret()` only turned `caret_vis_` on if `GetFocus() == handle_` - i.e. if this
   window currently has the logical Win32 input focus. Instrumenting this (temporarily, via an
   `ImGui::GetForegroundDrawList()->AddText()` overlay showing live state, plus a call counter in
   `do_setfocus()`) showed `do_setfocus()` *does* fire (so `SetFocus()` did something), but
   `GetFocus() == handle_` reads false immediately afterwards anyway - Win32's focus tracking for a
   window behind a permanently-hidden top-level ancestor is unreliable, not just its GDI painting.
   Also found in passing: `is_in_foreground()` had the same disease (compared `GetForegroundWindow()`
   against `handle_`, which can never match since the real foreground window is the separate GLFW
   window) - fixed by asking Dear ImGui instead (`!ImGui::GetIO().AppFocusLost`, which Dear ImGui already
   tracks via the GLFW backend's window-focus callback). **Fix**: dropped the `GetFocus() == handle_`
   requirement from `show_caret()` entirely - `CHtmlSysWin_win32_Input::update_caret_pos()` already
   independently parks `caret_pos_` off-screen (`(-100,-100)`, which `draw_caret_imgui()` treats as
   "hidden") whenever this window shouldn't actually be accepting input, so `show_caret()` only needs
   `caret_enabled_`. The real-app-focus check moved to `draw_caret_imgui()` itself (via the fixed
   `is_in_foreground()`), since a one-shot gate in `show_caret()` can lose a startup race (it can run
   before the GLFW window's first OS focus event arrives) with no way to retry, whereas a per-frame draw
   check just tries again next frame.

3. The one that actually explains "still nothing, even with both of the above fixed": **something was
   calling `hide_caret()` right back** after every successful `show_caret()`. Traced with an event-trace
   log (a capped string, appended to on every `show_caret()`/`hide_caret()`/`do_setfocus()`/
   `do_killfocus()` call, rendered on-screen each frame) - the trace always ended in a `KF+`/`HC!` pair
   (a `WM_KILLFOCUS` hiding the caret) with nothing after it, even though `set_caret_size()` (called from
   `get_input_begin()`) had already shown it moments earlier. Two independent sources of this, both now
   removed:
   - `CHtmlSys_mainwin::do_ncactivate(flag)` used to `show_caret()`/`hide_caret()` the main panel in
     step with `WM_NCACTIVATE`. Since the frame's `WM_NCACTIVATE` state bounces independently of real
     app focus (same hidden-top-level-window disease as above - Windows' internal activation bookkeeping
     for a hidden frame doesn't track whether the *actual* visible GLFW window is focused), this
     immediately re-hid the caret after almost every `show_caret()`, including the one from
     `do_ncactivate(TRUE)` itself's own dance. **Fix**: removed the caret toggling from
     `do_ncactivate()` entirely; the default `CHtmlSys_framewin::do_ncactivate()` behavior is untouched.
   - `do_setfocus()`/`do_killfocus()` (`WM_SETFOCUS`/`WM_KILLFOCUS`) themselves also directly called
     `show_caret()`/`hide_caret()`, and per point 2 above these events fire in an unreliable ping-pong
     for this window, with the *last* one during startup routinely being a spurious `WM_KILLFOCUS`.
     **Fix**: removed the caret calls from both handlers too - they still do their other jobs
     (`inval_sel_range()`, `notify_parent_focus()`, link-highlight cleanup). `set_caret_size()` (called
     once from `get_input_begin()` when input actually starts) is now the only thing that needs to call
     `show_caret()`, and it does so directly and unconditionally, independent of any Win32 focus message.

**Separately, and required regardless of the above**: `get_input_begin()` never called `take_focus()` at
the start of an input session (unlike `update_input_display()`, which calls it on every keystroke once
input is under way) - so even once the caret could render, the very first prompt after the window was
created had no caret until the player typed something. Added a `take_focus()` call at the top of
`get_input_begin()`, before `set_caret_size()` runs.

**Net effect**: `caret_vis_`/`show_caret()`/`hide_caret()` no longer depend on any Win32 focus message at
all for this window - only `caret_enabled_` (set once per window at construction) and
`update_caret_pos()`'s own semantic checks (`cmdtag_ != 0`, `cmdbuf_->is_caret_visible()`,
`owner_->is_active_page()`) gate whether a caret is logically shown, and `draw_caret_imgui()` alone
decides whether it's actually *drawn* this frame, based on `caret_pos_`, `more_mode_`,
`caret_modal_hide_`, and real app focus via `is_in_foreground()`.

**Verification method**: launched `guit3.exe` from PowerShell (`Start-Process` with a test game, per
"Running/verifying it visually" above), then used `(New-Object -ComObject WScript.Shell).AppActivate($pid)`
to give it real focus - `SetForegroundWindow()` called from an unrelated PowerShell process was
unreliable for this (Windows' foreground-lock-timeout heuristic), while `AppActivate` (used internally by
`SendKeys`) worked consistently. Captured a burst of cropped screenshots (~250ms apart) of the prompt-line
area and confirmed the caret bar toggles on/off across frames (proving it blinks, not just that it's
present in one static screenshot) and follows the cursor position after typing text via
`[System.Windows.Forms.SendKeys]::SendWait(...)`. **Debugging technique worth reusing**: when a
render-side fix compiles and appears correct but still produces no visible effect, temporarily draw the
relevant internal state as on-screen text via `ImGui::GetForegroundDrawList()->AddText()` (with a static
call-counter or a capped event-trace string for anything event-driven) rather than guessing - this is
what actually found layers 2 and 3 above, both of which were invisible from reading the code alone since
the Win32 focus-message ping-pong they depend on isn't something you can spot by inspection.

**File > Open New Game and File > Exit's confirmation prompts - converted from native MessageBox() to
ImGui popups.** Reported as "when pressing Open New Game or Exit in the File menu, there are still
native win32 popups." Two flows, both in `htmlgui.cpp`:

- **File > Open New Game** (`ID_FILE_LOADGAME` → `load_new_game()`) and the recent-games menu entries
  (`load_recent_game()`) both used to call `query_end_game()`, which blocked on a native `MessageBox()`
  ("Starting a new game will quit the current game without saving...") whenever a game was already
  running, before showing the (still-native, out of scope here - see below) `GetOpenFileName()` file
  picker.
- **File > Exit** (`ID_FILE_EXIT`) posts `WM_CLOSE` to `handle_`; `do_close()`'s `HTML_PREF_CLOSE_PROMPT`
  case (the "Quitting" preference tab's "Prompt before closing window and exiting" option) used to block
  on a native `MessageBox()` ("You are about to quit the game without saving...") before actually
  closing.

**Why these couldn't just call `tadswin_message_box()`** (the existing ImGui `MessageBox()` replacement,
see the "Startup `MessageBox`" entry above): both fire synchronously from inside code that runs *outside*
any ImGui frame - `do_close()` runs from the WM_CLOSE handler, dispatched during `glfwPollEvents()` at
the top of `event_loop()`, before that frame's `ImGui::NewFrame()`; `load_new_game()`/`load_recent_game()`
run from a menu click's `do_command()`, which *is* mid-frame but nested inside `render_menu_bar()`'s
`BeginMainMenuBar()`. Calling `tadswin_message_box()` (which runs its own nested
`glfwPollEvents`/`ImGui::NewFrame`/`Render` loop) from either spot would either have no frame to nest
into or would nest one frame inside another and hit an ImGui assert - exactly the problem the "Scope
note" in the earlier `MessageBox` entry predicted and deferred.

**Fix - deferred confirmation, matching the pattern `render_options_dialog()`/`render_context_menu()`
already established**: each flow now sets a pending-confirmation flag and returns immediately instead of
blocking; a new per-frame render function (called from `CHtmlSys_mainwin::do_render()`, same root
ID-stack depth as the other two) shows an ImGui `BeginPopupModal` and acts on the result once the player
clicks a button:
- `load_new_game()`/`load_recent_game()` check `main_panel_->get_eof_flag()` directly now (this is
  exactly what `query_end_game()` used to check before deciding whether to prompt at all) and, if a game
  is running, set `pending_new_game_confirm_` (plus `pending_new_game_is_recent_`/
  `pending_new_game_recent_idx_` to remember which of the two callers to resume) instead of calling
  `query_end_game()`, which is now gone. `render_new_game_confirm()` shows the modal and, on Yes, calls
  whichever of the new `do_load_new_game_prompt()`/`do_load_recent_game()` helpers (the rest of the old
  `load_new_game()`/`load_recent_game()` bodies, split out) applies.
- `do_close()`'s `HTML_PREF_CLOSE_PROMPT` case sets `pending_quit_confirm_` and returns `FALSE` (don't
  close yet) instead of blocking. `render_quit_confirm()` shows the modal and, on Yes, sets
  `quit_confirmed_` and re-posts `WM_CLOSE` - `do_close()` checks `quit_confirmed_` at the top of the
  `HTML_PREF_CLOSE_PROMPT` case and, if set, skips straight to `goto do_close_window` instead of
  prompting again, so the second pass through actually closes.

Both modals share a small `render_yesno_confirm_popup()` static helper (message text, separator, two
centered buttons) styled to match `tadswin_message_box()`'s look, and both use an `..._opened_` bool to
call `ImGui::OpenPopup()` only once per pending request (mirroring `tadswin_message_box()`'s
`popup_opened` guard) rather than every frame. Enter/Escape both activate the second button (No) rather
than the first, deliberately - both source `MessageBox()` calls used `MB_DEFBUTTON2`, and an accidental
stray keystroke shouldn't be able to end a game or quit the app.

**Update: the `GetOpenFileName()` file-picker dialog shown after this confirmation is now ImGui-native
too** - see the "File open/save dialog" entry near the end of §3.3. The paragraph below describes
testing from when it was still deliberately left native; the file-picker behavior it describes (a
separate real OS window) no longer applies, but the confirmation-popup behavior it describes is
unchanged.

**Verified** end-to-end by launching `guit3.exe tests/ditch3.t3`, starting the game, and driving the UI
via simulated mouse clicks (`SetCursorPos`/`mouse_event`) plus screenshots of the real GLFW window
(`EnumWindows` to find it, `GetWindowRect`+`CopyFromScreen` to capture it - same recipe as elsewhere in
this section). Confirmed: (1) File > Open New Game while the game is running shows the ImGui "TADS"
popup (not a native `#32770` window - the game text visibly stays part of the same captured window
underneath it) with Yes/No; clicking Yes brings up the native "Choose a game to load" file dialog as a
separate real OS window (confirming the file picker is still native, as intended), and Cancel returns
cleanly with no leftover popup. (2) With the "Quitting" preference (Edit > Options > Quitting tab) left
at its default "Send QUIT command to game", File > Exit correctly falls through to the game's own
`quit`-command confirmation instead (unchanged, unrelated code path) - this default meant the very first
Exit tests looked like nothing had changed until the close-action preference was switched to "Prompt
before closing window and exiting" and re-tested. (3) With that preference set to Prompt, File > Exit
shows the new ImGui "TADS" popup ("You are about to quit the game without saving. Do you really want to
quit?"); clicking Yes closes the popup and the process actually exits (confirmed via `Get-Process`
failing to find the PID moments later) - the deferred re-post-`WM_CLOSE` round trip does reach real
termination, not just a UI-level dismiss. **Gotcha hit while testing**: the File menu's "Quit Game" item
sits directly above "Exit" (separated only by the recent-games list) and sends `quit` straight to the
game exactly like Exit's `HTML_PREF_CLOSE_CMD` default does - it's easy to misclick one for the other
when driving the menu by fixed pixel offsets, and both produce the *same* visible "Do you really want to
quit?" game-text prompt, so a wrong click can look like a successful test of the wrong thing. Also worth
noting: the main window's saved screen position (`imgui.ini`) persists across separate launches, so a
window-relative click offset computed for one run's `GetWindowRect()` silently stops matching after
relaunching the app - always re-query `GetWindowRect()` per process rather than reusing a coordinate
captured from an earlier run.
