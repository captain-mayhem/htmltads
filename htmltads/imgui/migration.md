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

Progress so far (25 commits): GLFW/ImGui window creation, keyboard input, FreeType-based font loading
and metrics, text display, child-window handling, coloring, resizing, link coloring, positioning,
character-encoding fixes, image rendering via GL textures, text selection, mouse hover, and text
highlighting. This is genuinely useful progress, but it's all in the *game text window* rendering path;
the application chrome (menu, status bar, dialogs, window creation itself) is still 100% Win32.

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

### 3.1 Menu bar
No ImGui menu code exists at all (`grep` for `ImGui::BeginMenuBar`/`BeginMainMenuBar`/`MenuItem` in
`imgui/*.cpp` returns nothing). The menu is entirely native: `LoadMenu`/`SetMenu`
([htmlgui.cpp:755](htmlgui.cpp#L755), [:10806](htmlgui.cpp#L10806), [:16094](htmlgui.cpp#L16094)),
built from a Win32 `.rc` resource (`htmlt3.rc`, loaded via `htmlres.h` resource IDs), plus
`CTadsSyswin::syswin_set_win_menu()` ([tadswin.cpp:3404](tadswin.cpp#L3404)) which just calls
`SetMenu(win_->get_handle(), menu)` on the (soon to be removed) HWND. Owner-drawn menu items and
`iconmenu.cpp` (icon-annotated menu items via `SetMenuItemInfo`/`SetMenuInfo`) will also need an
ImGui-native equivalent (custom `MenuItem` rendering with an icon texture).

**Plan**: define the menu structure data-driven (command ID → label/shortcut/icon/submenu), keep it
independent of the `.rc` file, and render it with `ImGui::BeginMainMenuBar()` /
`ImGui::BeginMenu()`/`ImGui::MenuItem()` inside the existing `event_loop()`. Route `MenuItem` clicks
through the same command-dispatch path currently triggered by `WM_COMMAND`.

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
`tadsdlg2.cpp`'s `CreateWindow("SysTreeView32", ...)`). None of this has been started.

**Plan**: this is the largest remaining chunk of work by file count. Recommend introducing an
ImGui-native modal/dialog base class mirroring the current `CTadsDialog` API surface (so call sites
mostly stay the same), then porting dialogs one at a time in order of how often they're hit during
normal play (probably: preferences → find/replace → folder picker → the rest), each rebuilt as a plain
data structure of controls drawn with `ImGui::Begin(..., ImGuiWindowFlags_Modal)`/checkboxes/combos/
input fields instead of a `.rc` template.

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
2. **Menu bar** (§3.1) and **status bar** (§3.2 — status bar done) — both are self-contained, don't
   block on the dialog framework, and remove two of the four still-native chrome pieces.
3. **Child windows** (§3.4): scrollbars/banners/size-grip — needed so the top-level HWND can actually
   be deleted, not just hidden. Skip MDI-frame/MDI-client child-window paths entirely (§4) — the client
   doesn't need them.
4. **Dialog framework** (§3.3): biggest chunk; do this after chrome is ImGui-native so dialogs can be
   simple ImGui modals over an all-ImGui main window instead of native popups over a mixed window.
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
in [tadsstat.cpp](tadsstat.cpp)). **Reach for this pattern first for any other always-on-top fixed chrome
(the menu bar in §3.1 next, later dialogs) if a normal window's styling doesn't visibly apply** — don't
spend time re-diagnosing the same symptom from scratch.

**Line-number references throughout this document will drift.** `htmlgui.cpp` in particular is ~18,000
lines and every edit shifts everything below it — several line numbers cited in §3.2 were already off by
a handful of lines by the time the section was finished being written, because an earlier edit in the
same file shifted things. Treat line numbers here as "was roughly here as of this writing," and re-`grep`
for the function/symbol name before trusting a specific line.
