# guit3 Migration Plan — from Win32 htmlt3 to cross-platform Dear ImGui

## 1. Where things live

- `htmltads/htmltads/win32/` — the original Win32 `htmlt3` application (untouched, kept as reference).
- `htmltads/htmltads/imgui/` — the **guit3** rewrite. It started as a copy of the `win32` sources
  (`git log` shows the first commit is literally "Copied platform specific headers for htmltads into
  new folder") and is being converted file-by-file to GLFW + Dear ImGui + OpenGL3, with FreeType for
  font rendering.
- `htmltads/htmltads/imgui/imgui/` — vendored Dear ImGui + backends (`imgui_impl_glfw`,
  `imgui_impl_opengl3`) plus its own unused `main.cpp` demo (not part of the `guit3` target).
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
ImGui-native pieces too: the menu bar, toolbar, and status bar (§3.1, §3.2) are all done, and the first
dialog - Options (§3.3) - is done too. Window creation itself, the remaining chrome
(banners/scrollbars), and the rest of the dialogs (find/replace, folder picker, Customize Theme) are
still 100% Win32.

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
[w32main.cpp:486-563](w32main.cpp#L486-L563)) creates a `CHtmlSys_dbglogwin`, a second top-level
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
`w32main.cpp:552`); `CTadsSyswin::syswin_create_system_window()`'s GLFW overload
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
through `w32_msgbox()` in [w32tr.cpp:181](w32tr.cpp#L181), which called `MessageBox()` directly.

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
`w32tr.cpp:181`). The many other `MessageBox()` call sites — `htmlgui.cpp` (quit/save-overwrite
confirmations, `foldsel2.cpp`, `htmlpref.cpp`, `w32fndlg.cpp` — see the `grep -i MessageBox` results —
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
| `foldsel.h` / `foldsel2.cpp` | 11 / 10 | Custom folder-picker dialog. Has its own `WinMain` guarded by `#ifdef BUILD_TEST_PROG` (dead code in the `guit3` build) but the real dialog logic is still Win32. |
| `w32fndlg.h` / `.cpp` | 6 / 10 | Find/Replace dialog (`FINDREPLACE`) — **not started**. |
| `tadscbtn.h`, `w32webui.h`, `w32snd.h`, `tadsstat.h`, `htmlpref.h` | 6, 5, 4, 4, 4 | Custom button control, web-UI glue, sound glue, status line, prefs header — all **not started**. |

Everything else (tadswav/tadsvorb/tadsmidi/tadssnd/tadsreg/tadsimg/tadsjpeg/tadspng/tadscar/tadscsnd/
tadstab/tadscom) has only 1-3 stray references, mostly just `#include <windows.h>` or a type alias —
low `HWND` density but not necessarily low effort (audio APIs in particular are all Win32 multimedia
calls, see §4).

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
"`-debugwin` still showed a real, blank second OS window" entry further down. The debug-log window's own
right-click popup (`IDR_DEBUGLOG_POPUP`, loaded via the same `load_context_popup()`) and the status bar's
right-click popup (`IDR_STATUSBAR_POPUP`, `statusline_popup_`) are structurally identical to §3.1a's
right-click menu but still unported.

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
- `get_handle()` is kept (returning `0`/null) purely so `w32webui.h` — the not-yet-ported, phase-two
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

### 3.3 Dialogs (preferences, find/replace, folder picker, generic app dialogs)
All dialog infrastructure (`tadsdlg.cpp`/`tadsdlg2.cpp`, `htmlpref.cpp`, `foldsel2.cpp`,
`w32fndlg.cpp`) is built on `CreateDialogParam`/`DialogBoxParam` with resource-defined (`.rc`) layouts,
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

**What's still native, deliberately.** A few sub-flows reachable from the Options dialog remain
Win32 for now: "Customize Theme..." (Appearance tab; opens `run_appearance_dlg()`'s own
Fonts/Colors/More/Media property sheet - a comparably large second dialog, out of scope for this pass),
the Starting tab's folder-browse button (`CTadsDialogFolderSel2::run_select_folder()`), and the Game
Chest tab's two file-browse buttons (`GetOpenFileName()`). The "New Theme" name-entry prompt, which
*was* a small custom native dialog (`CTadsDialogNewProfile`, `DLG_NEW_PROFILE`), was reimplemented as a
small ImGui popup instead of kept native, since its only dependency on Win32 was a real `HWND` combo box
handle passed in purely for a duplicate-name check - easier to redo as a plain loop over the ImGui
dialog's own already-in-memory profile list than to keep threading a native control handle through.
Deleting/resetting a theme reuses plain `MessageBox()` confirmation prompts, same as the original.

These remaining pieces are safe to leave native, and were verified (not just assumed) to still work
correctly when triggered from an ImGui button's click handler: `PropertySheet()`, `GetOpenFileName()`,
`DialogBoxParam()` (which is what both `CTadsDialogFolderSel2` and the old `CTadsDialogNewProfile` were
built on) and `MessageBox()` are all *modal, self-pumping* Win32 APIs - each one blocks the calling
thread and internally runs its own `GetMessage`/`DispatchMessage` loop for the duration of the call. That
loop is entirely independent of `guit3`'s own (nonexistent - see §2) main message pump, unlike the
now-fixed WM_COMMAND-routed *non-modal* child controls (banners, scrollbars, the old toolbar/menu) that
motivated this whole port. Confirmed empirically too: `GetOpenFileName()` was already in active,
working use elsewhere (File > Open) before this change, and `File > Open`/File-safety-style native
common dialogs have worked throughout the port. Practical implication for future dialog work: a modal
system/common dialog invoked from an ImGui callback is safe to leave native and defer; a custom
non-modal Win32 child control is not, and needs the ImGui treatment before it'll do anything at all.

**Verification**: built `guit3` and drove it via the PowerShell screenshot recipe in §6 - clicked
Edit > Options, confirmed the Appearance tab (theme combo showing "Multimedia", Delete/description
correctly disabled since it's a standard theme, Reset to Defaults correctly enabled), switched to the
Keyboard tab and clicked a radio button, switched away and back to confirm the change stuck, checked
File Safety and Network Safety tabs both show sane non-default-looking real values (game-folder-only /
local-only, i.e. genuinely read from the preferences object, not just placeholder UI), and clicked
Close to confirm the popup dismisses cleanly without disturbing the main window underneath (command
input line still live immediately after).

**Not yet ported**: find/replace (`w32fndlg.cpp`), the folder-picker dialog itself
(`CTadsDialogFolderSel2`/`foldsel2.cpp`, still only *called* from ImGui code, not rewritten), and
`run_appearance_dlg()`'s Fonts/Colors/More/Media "Customize Theme" property sheet. The general
`CTadsDialog`-mirroring base class this section used to recommend turned out not to be necessary for
the Options dialog - whether it's worth building depends on whether the remaining dialogs turn out to
share enough structure to benefit from one; worth revisiting once find/replace or Customize Theme is
tackled rather than building it speculatively now.

### 3.4 Child "windows" (banners, scrollbars, tooltips, size-grip)
`CTadsWin` treats banners, scrollbars, and the resize grip as real child `HWND`s
(`tadswin.cpp:2535,2550,2573,2633`, `htmlgui.cpp:3736` for banners, `htmlgui.cpp:5360` for tooltips).
These all need an HWND-owning parent, which is exactly what's forcing the Win32 top-level frame in §2
to keep existing.

**Plan**: convert scrollbars and the size-grip to `ImGui::SliderFloat`-style custom-drawn widgets (or
use `ImGui`'s built-in child-window scrollbars where the layout allows it), and banners to nested ImGui
child regions instead of nested `HWND`s. This is a prerequisite for actually deleting the stray Win32
top-level window rather than just hiding it.

### 3.5 Fonts
Real progress here: `tadsfont.cpp` already uses `imgui/misc/freetype/imgui_freetype.h` for glyph
rendering (cross-platform). However it still has ~9 references to GDI (`HDC` etc.), likely leftover
metrics/enumeration code (e.g. font-family enumeration, `GetTextMetrics`) that hasn't been replaced by
a FreeType/OS-agnostic equivalent yet, plus `w32font.cpp`/`w32font.h` (2 refs) which may be entirely
dead now that `tadsfont.cpp` exists — worth checking if `w32font.cpp` is still called from anywhere
before deleting.

### 3.6 Images
Also good progress: `tadsimg.cpp` uploads to a GL texture and renders with `ImGui::Image()`
([tadsimg.cpp:167](tadsimg.cpp#L167)). It still has ~11 `HDC`/DIB references though (old GDI decode
path likely coexisting with the new GL path), and `w32img.cpp` (a separate, still fully Win32 file) is
still compiled into `guit3` — needs auditing for what still calls into it vs. what's dead.

### 3.7 Sound / MIDI
Untouched. `tadssnd.cpp`, `tadsmidi.cpp`, `tadswav.cpp` all call directly into Windows Multimedia
(`waveOut*`, `midiOut*`) and/or DirectSound (`dxguid.lib` is linked). This is one of the larger
"actually platform-specific" subsystems (unlike menus/dialogs, which are UI-toolkit-specific and have a
straightforward ImGui replacement, audio needs a real cross-platform audio backend — e.g. something
like miniaudio/SDL_audio/OpenAL — chosen and wired in). Vorbis/MP3 decoding (`tadsvorb.cpp`, the
`win32/mpegamp/*` sources already linked into `guit3`) are already portable; only the output layer is
Windows-locked.

### 3.8 Windows-only platform services still relied on
- **Registry** (`tadsreg.cpp` — `RegOpenKeyEx`/`RegQueryValueEx`/`RegSetValueEx`): used for persisted
  settings. Needs a cross-platform key/value store (INI/JSON file, or per-OS: registry on Windows,
  plist on macOS, XDG config dir on Linux).
- **COM/OLE**: `CoInitialize`/`CoUninitialize` in `w32main.cpp` ([w32main.cpp:817](w32main.cpp#L817),
  [:883](w32main.cpp#L883)), plus the ActiveX web-control embedding in `tadswebctl.h`/`w32webui.h`. Per
  §4, the Web UI feature must survive but is phase-two work — for phase one, gate
  `tadswebctl.*`/`w32webui.h` (and the `CoInitialize`/`CoUninitialize` calls that only exist to support
  it) behind a compile-time flag so `guit3` builds clean without them rather than either porting or
  deleting the feature now.
- **Win32-only linked libraries** in `imgui/CMakeLists.txt`: `Htmlhelp.lib` (WinHelp — obsolete help
  format), `Comctl32.lib` (native controls, goes away once dialogs are ImGui), `Winmm.lib` (audio, see
  §3.7), `Ws2_32.lib`/`Wininet.lib`/`Mpr.lib` (networking — the repo already vendors `curl/`, which is
  cross-platform and could replace `Wininet` usage), `Shlwapi.lib`, `Version.lib`, `dxguid.lib`. Each of
  these marks a place where cross-platform equivalents need to be chosen.
- **`RICHED32.DLL`** load in `w32main.cpp:828` and the debug console (`init_debug_console`/
  `close_debug_console`) — Windows-specific, need auditing for whether they're still needed once
  dialogs move to ImGui.
- **`event_loop()` currently also unconditionally calls `ImGui::ShowDemoWindow()`**
  ([htmlgui.cpp:14533](htmlgui.cpp#L14533)) — leftover from the ImGui example template; should be
  removed once it's no longer needed for reference.

## 4. Decisions (resolved)

- **Embedded Web UI** (IE ActiveX control, `tadswebctl.*`/`w32webui.h`): needs to survive long-term,
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
   `setVisible(true)` is called). The real fix (eliminating the Win32 frame entirely) still depends on
   §3.4.
2. **Menu bar, toolbar** (§3.1) **and status bar** (§3.2) — **all done**. All were self-contained,
   didn't block on the dialog framework, and remove three of the four still-native chrome pieces
   (dialogs remain).
3. **Child windows** (§3.4): scrollbars/banners/size-grip — needed so the top-level HWND can actually
   be deleted, not just hidden. Skip MDI-frame/MDI-client child-window paths entirely (§4) — the client
   doesn't need them.
4. **Dialogs** (§3.3): biggest chunk by file count. The Options dialog (Edit > Options) is **done** -
   ImGui modal with a tab bar, no separate framework needed. Still remaining: find/replace
   (`w32fndlg.cpp`), the folder-picker dialog (`foldsel2.cpp`, only called from ImGui code so far, not
   itself rewritten), and the "Customize Theme" Fonts/Colors/More/Media property sheet
   (`run_appearance_dlg()`).
5. **Font/image cleanup** (§3.5, §3.6): remove now-dead GDI code paths once nothing still calls them.
6. **Gate the Web UI behind `#ifdef`s** (§3.8/§4): get `tadswebctl.*`/`w32webui.h` and their
   COM/ActiveX calls compiling out cleanly for `guit3` rather than porting them now.
7. **Audio backend** and remaining **platform services** (§3.7, §3.8): registry/settings storage,
   networking (curl instead of Wininet), help/rich-edit dependency audit.
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
