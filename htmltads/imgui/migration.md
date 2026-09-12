# guit3 Migration Plan — from Win32 htmlt3 to cross-platform Dear ImGui

## 1. Where things live

- `htmltads/htmltads/win32/` — the original Win32 `htmlt3` application (untouched, kept as reference).
- `htmltads/htmltads/imgui/` — the **guit3** rewrite. It started as a copy of the `win32` sources and is
  being converted file-by-file to GLFW + Dear ImGui + OpenGL3, with FreeType for font rendering.
- `htmltads/imgui/` — vendored Dear ImGui + backends (`imgui_impl_glfw`, `imgui_impl_opengl3`) plus its
  own unused `main.cpp` demo. Lives top-level alongside the other 3rdparty libs (`glfw`, `freetype`,
  `miniaudio`, …), *not* nested under the app directory; `guit3` includes it as `<imgui/imgui.h>` because
  its CMakeLists puts `htmltads/` on the include path.
- `htmltads/htmltads/emscripten/` — a **separate**, older, parallel web-port effort. Not part of `guit3`;
  out of scope (see §4).
- Build target: `htmltads/htmltads/imgui/CMakeLists.txt` defines `guit3`. It still hard-gates to Windows
  (`if (NOT WIN32) return()`), and two gates above it do the same (§5.1). Removing all three is the goal.

**Status in one paragraph.** The *rendering and interaction* port is essentially complete: GLFW/ImGui
window, keyboard/mouse input, FreeType fonts and metrics, text layout and drawing, colors, links, images
(GL textures for PNG/JPEG/MNG), selection, hover, scrollback scrollbar, blinking caret. All application
chrome is ImGui-native: menu bar, toolbar, status bar, all context menus (§3.1–3.2), and all seven dialogs
— Options, Customize Theme, Manage Themes, file open/save, Find, folder picker, License (§3.3). There are
**no real Win32 windows in guit3 at all** any more — not the frame, not banners/scrollbars/tooltips, not
even `handle_` (§3.4,
§3.4a); the only OS window is the one `GLFWwindow`. Digitized audio runs on miniaudio with `std::`
threading (§3.7).

What remains is not UI work — it is the **platform layer underneath it**: resources, settings storage,
clipboard, cursors, shell integration, system colors, font enumeration, file I/O, character encoding, and
the build plumbing. §5 is the plan for that.

## 2. History — the "both windows open" problem, and how it ended

`CTadsWin::create_system_window()` used to create **two** windows per top-level window: a real Win32 `HWND`
(`handle_`, via `CreateWindowEx`) and a `GLFWwindow` (`m_window`). Both were shown, so two top-level windows
appeared. The `HWND` was functionally dead — there is **no `GetMessage`/`DispatchMessage` pump written
anywhere in guit3** — but it was still created because every child (banners, scrollbars, tooltips, status
bar, menu, dialogs) needed a real parent `HWND`.

Resolution, in order: (a) short-term — `setVisible()` branches on `parent_ == nullptr` and never shows any
top-level `handle_`, and the GLFW overload sets `glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE)` before
`glfwCreateWindow` (GLFW shows new windows by default, which was the more visible half of the symptom);
(b) the extra child `HWND`s were removed (§3.4); (c) `handle_` itself became an opaque token (§3.4a). Done.

**Lessons that outlived this section:**

- Branch on *"is this top-level"* (`parent_ == nullptr`), not on *"does this own a GLFW context"*
  (`m_window != 0`). A second top-level window (`CHtmlSys_dbglogwin`) has no GLFW window of its own —
  `syswin_create_system_window()`'s GLFW overload deliberately refuses to create a second real window — so
  the `m_window` test silently fell through to the "show a real HWND" branch and put a blank native window
  on screen.
- A secondary top-level `CTadsWin` renders through `CTadsWin::do_render_content_begin()`'s parentless
  branch as an ImGui `Begin()`/`End()` inside the one real window. That branch must **not** use
  `ImGuiWindowFlags_NoInputs` or `ImGuiCond_Always` positioning — those are `CHtmlSys_mainwin`-specific
  (it routes mouse input by hand and needs ImGui never to intercept). It uses `ImGuiCond_FirstUseEver`
  and reads `GetWindowPos()`/`GetWindowSize()` back into `m_pos`/`m_size` after `Begin()`.
- `event_loop()`'s manual mouse routing must be told which window the pointer is over. It rect-tests
  `dbgwin_->m_pos`/`m_size` and routes uncaptured clicks/hovers there instead of `this`.
- **`GetWindowPos()`'s meaning depends on where the chrome was reserved.** The main window's menu bar goes
  through `BeginViewportSideBar()`, which shrinks `viewport->WorkPos` — so `GetWindowPos()` there already
  means "below the menu bar". The debug window draws its menu bar with `ImGuiWindowFlags_MenuBar` *inside
  its own* `Begin()`, so `GetWindowPos()` means "above the menu bar", and a child anchored at `(0,0)`
  renders one menu-bar-height too high and gets clipped. Anchor such children at `ImGui::GetCursorPos()`
  instead. (This produced a phantom "solid band across the bottom" that looked exactly like a sizing bug.)
- **Diagnosing "part of my child fills, part doesn't": confirm size *and* position independently.** Paint
  the child's full nominal rect a throwaway solid color and diff it against
  `GetWindowDrawList()->GetClipRectMin()/Max()`. If the paint and the clip rect agree with each other but
  disagree with the child's claimed `Pos`/`Size`, the bug is *where* it starts, not how big it is.
- `ImGui::BeginMenuBar()` must be called directly inside the `Begin()` it belongs to, not from a nested
  child — hence `CHtmlSys_dbglogwin::do_render_content_begin()` calls `render_menu_bar()` itself.
- **Startup `MessageBox` → `tadswin_message_box()`** ([tadswin.cpp](tadswin.cpp)/[tadswin.h](tadswin.h)):
  same signature and blocking contract as `MessageBox()` (`MB_OK`/`MB_OKCANCEL`/`MB_YESNO`,
  `IDOK`/`IDCANCEL`/`IDYES`/`IDNO`) but renders an ImGui `BeginPopupModal`, running its own local
  `glfwPollEvents`/`NewFrame`/`Render`/`glfwSwapBuffers` loop on the caller's `GLFWwindow*`. Used by
  `w32_msgbox()` ([guitr.cpp](guitr.cpp)), the VM's error-display hook. Falls back to real `MessageBox()`
  if there is no window yet.
  **Scope trap:** this only works for callers with *no* ImGui frame in progress. Anything firing from
  inside `event_loop()`'s per-frame handling must use the deferred pending-flag pattern instead (§3.3),
  or it nests `NewFrame()` inside `NewFrame()` and asserts.
- The warning that popup was showing ("no mapping file is available for the local character set") was
  fixed at the source: `guit3`'s [CMakeLists.txt](CMakeLists.txt) now has the same `POST_BUILD` charmap
  copy `t3run` uses. The VM's `CResLoader` looks for `charmap/` **next to the .exe**, never relative to
  cwd. If the warning reappears, that copy step was removed or the build dir is stale.

## 3. Subsystem inventory — what's ported

### 3.1 Menu bar, toolbar, context menus — done

`CHtmlSys_mainwin::render_menu_bar()` / `render_toolbar()` / `render_context_menu()` /
`render_statusbar_context_menu()` ([htmlgui.cpp](htmlgui.cpp), declared in [htmlgui.h](htmlgui.h)) replace
the native menu (`IDR_MAIN_MENU`), the `CreateToolbarEx()` control, the game-text right-click popup
(`IDR_EDIT_POPUP_MENU`) and the status-bar popup (`IDR_STATUSBAR_POPUP`).
`CHtmlSys_dbglogwin::render_menu_bar()` does the same for `IDR_DEBUGWIN_MENU`.

**The core technique, used everywhere:** items call `do_command(0, id, 0)` and read enabled/checked state
from `check_command(&check_cmd_info(id))` — the exact virtuals the native `WM_COMMAND`/`WM_INITMENUPOPUP`
handlers called. No application logic was duplicated, just re-entered from a new call site. Dynamic content
(recent games, Themes profile list) is rebuilt from source data every frame instead of mutating a cached
`HMENU`; Game Chest is gated on the runtime `is_game_chest_present()` rather than the `.rc`'s `#ifdef`.

The native `LoadMenu`/`SetMenu`/`create_toolbar()` code is left in place as harmless dead code (the
`SetMenu` calls themselves were deleted to avoid leaking the `HMENU`).

**Gotchas worth keeping:**

- **Don't hand-reserve space for `BeginMainMenuBar()`/`BeginViewportSideBar()`.** They add their height
  into `viewport->BuildWorkInsetMin.y`, which automatically shrinks `GetMainViewport()->WorkPos`/`WorkSize`
  on the next frame. `do_render()` re-reads those every frame, so the space is reserved once, at the outer
  window level. Adding it again in `recalc_banner_layout()` double-reserves it and leaves a large gap.
  **Rule: anything built on `BeginViewportSideBar()` reserves its own space; anything drawn via the
  foreground draw list (the status bar) still needs manual reservation.**
- **This ImGui build does not parse `&` mnemonics.** Hardcoded labels are written without `&`; the three
  Themes labels that come from `LoadString()` (`IDS_MANAGE_PROFILES`, `IDS_SET_DEF_PROFILE`,
  `IDS_CUSTOMIZE_THEME`) go through a `strip_mnemonic()` lambda. Watch for this in any future ImGui text
  sourced from a `.rc` string table.
- **Keyboard shortcuts are display-only** — `MenuItem()`'s shortcut parameter is cosmetic — **was true until
  §5.4/L.** `CHtmlSys_mainwin::do_accel_keys()` now dispatches them for real each frame, reading the
  `IDR_ACCEL_WIN`/`IDR_ACCEL_EMACS` resources in portable form rather than through `CTadsAccelerator`
  (which turned out to be dead in guit3 regardless - only the out-of-scope debugger ever instantiated it).
- **`OpenPopup()` and `BeginPopup()` must be at matching ID-stack depths** — ImGui hashes string IDs against
  the current window/child ID. `do_rightbtn_up()` fires from `event_loop()`'s routing block at root level,
  so `render_context_menu()` must also run at root level: it's called from `do_render()` *after*
  `CTadsWin::do_render()` returns. Calling `BeginPopup()` from inside a `do_render_content_begin()` pair
  would silently never match.
- **Claim a click on button-*down*, not just button-up.** `CHtmlSysWin_win32::do_leftbtn_down()` grabs
  mouse capture for essentially any click handed to it, so by button-up time `event_loop()` routes to the
  captured child and never to your own `do_rightbtn_up()` override. `CHtmlSys_mainwin::do_rightbtn_down()`
  rect-tests via `over_statusbar()` and returns `TRUE` *without* recursing, so no child can steal it.
- **`CTadsWin::do_rightbtn_down()` needed the same recursive child dispatch `do_leftbtn_down()` has** — the
  base version was a `{ return FALSE; }` stub, so right-clicks never reached leaf windows.
- **The foreground draw list always paints last.** A popup opened at the click point *inside* the status
  bar had its nearest rows painted over every frame — content rendered fine, then got covered. Fix:
  `SetNextWindowPos()` with pivot `(0, 1)` anchored at `(mouse_x, status_bar_top)` so it grows upward.
  **Reuse this diagnosis for any "some rows mysteriously don't appear" report near foreground-drawn chrome
  (status bar, caret, scrollbar): check screen overlap before assuming a sizing/constraint bug.**
- **Toolbar icons**: `win32/runtbar.bmp` (`IDB_TERP_TOOLBAR`, 304×15, 4bpp indexed, nineteen 16×15 frames)
  is loaded once by `load_toolbar_texture()` into a single GL atlas — `LoadImage(..., LR_CREATEDIBSECTION)`
  + `GetDIBits()` to expand to 32bpp, then manual BGRA→RGBA that also converts the bitmap's top-left-pixel
  color key into a real alpha channel (GL has no color-key equivalent). Each button samples a `1/19` UV
  slice via `ImGui::ImageButton()`. Use **`GL_NEAREST`** — the icons are packed edge-to-edge with no
  padding and linear filtering visibly bleeds neighbours. *(As of M2/B (§5.4/B) the resource+GDI+conversion
  half is behind `os_load_toolbar_rgba()` in [guios_w32.cpp](guios_w32.cpp); `load_toolbar_texture()` keeps
  only the GL upload.)*
- **`ID_THEMES_DROPDOWN` is a native split button, not a plain dropdown button.** The original toolbar
  (`TBSTYLE_BUTTON | TBSTYLE_DROPDOWN`, win32/htmlw32.cpp) has two independent hit regions sharing one slot:
  clicking the icon sends `WM_COMMAND`/`ID_THEMES_DROPDOWN` straight through (opens Customize Theme, same as
  `do_command()`'s `ID_APPEARANCE_OPTIONS` case), while a separate small arrow region sends `TBN_DROPDOWN`
  (shows the theme-list popup). `render_toolbar()` used to treat the whole slot as one `ImageButton()` that
  always opened the popup, losing the "click the icon to customize" half. Fixed by drawing the arrow as its
  own adjacent button (`ImGui::SameLine(0, 0)` + a fixed-width `ImGui::Button()`, with a hand-drawn triangle
  via `AddTriangleFilled()` since there's no separate arrow glyph in the icon strip) — the icon keeps calling
  `do_command()` on click, the arrow calls `OpenPopup()`. **The triangle needs a fixed dark color**
  (`IM_COL32(32,32,32,255)`), not `ImGuiCol_Text` — same near-white-on-light-grey contrast problem as the
  menu label bullet below, caught by screenshotting rather than just compiling.
- **Chrome color**: menu bar and toolbar each push `ImGuiCol_WindowBg` (+ `ImGuiCol_MenuBarBg`) to the
  status bar's grey `IM_COL32(212,212,212,255)`. The base style is `StyleColorsDark()`, whose light text
  is unreadable on that, so a `menu(label)` helper pushes `ImGuiCol_Text` black *just around the top-level
  label* and pops before the dropdown items draw — the dropdowns keep the dark theme.
- **`ImageButton()` always paints a Button-colored frame**, even at rest. `render_toolbar()` pushes
  `ImGuiCol_Button` transparent and hovered/active to greys.
- **Toolbar height** = `button_height + style.WindowPadding.y * 2` (ImGui only pads above the row, so an
  arbitrary fudge left the buttons overflowing the bottom).
- **Not ported, deliberately**: `iconmenu.cpp`'s owner-drawn menu icons (dead; follow the toolbar's
  GL-texture pattern if ever wanted). `IDR_DEBUGLOG_POPUP` is unreachable in guit3 — its
  `load_context_popup()` call is gated on `debugger_ifc_ != 0`, and `guimain.cpp` always constructs the
  debug window with a null one.

### 3.2 Status bar — done

`CTadsStatusline` ([tadsstat.h](tadsstat.h)/[tadsstat.cpp](tadsstat.cpp)) no longer owns a native control.
`CTadsStatusSource`/`CTadsStatusPart` (the "ask each registered source for a message" protocol) are
unchanged — only the backend was swapped.

- Holds `part_edges_`/`part_texts_` (mirroring `SB_SETPARTS`/`SB_SETTEXT`) instead of an `HWND`;
  `set_parts()`/`set_part_text()` replace the `SendMessage` calls, including two external call sites that
  poked the control directly (`adjust_statusbar_layout()`, `do_timer()`).
- `notify_parent_resize()` keeps its fixed/proportional layout algorithm, now taking the width as a
  parameter instead of `GetClientRect`-ing a handle.
- `render(x, y, width)` draws the bar bottom-anchored to `GetMainViewport()`'s work area, with a top border
  line, per-part clipping and separators — deliberately mimicking the Win32 look. Fixed light grey
  `IM_COL32(212,212,212,255)` with black text, independent of the dark theme.
- `get_height()` replaces the `GetClientRect(statusline_->get_handle())` that `recalc_banner_layout()`
  already used to reserve space.
- `owner_draw()`/`WM_DRAWITEM` was dropped — dead even in the original. `get_handle()` is kept returning
  null purely so the not-yet-ported Web UI code (§4) keeps compiling.

**Gotcha — `PushStyleColor(ImGuiCol_WindowBg, …)` inside an ordinary `Begin()` window is unreliable in this
codebase for fixed chrome.** It compiled and ran but the grey never reached the screen; something in this
large, partially-ported window tree (many windows, inconsistent Push/Pop discipline across mechanically
copied files) draws over a normal window's background most frames. Drawing straight into
`ImGui::GetForegroundDrawList()` (`AddRectFilled`/`AddLine`/`AddText`, no `Begin()` at all) fixed it
immediately. **Try the plain ImGui API first** — `BeginMainMenuBar()` renders its background correctly out
of the box, so this isn't universal — but reach for the foreground list as soon as styling doesn't visibly
apply.

### 3.2a Banner subwindow clicks — leaf windows must bounds-check

**Symptom**: clicking the `west` link in a room/status banner did nothing.

**Cause**: `CTadsWin::do_leftbtn_down()`'s recursive dispatch offers the click to every visible subwindow in
creation order, stopping at the first `TRUE`. `main_panel_` is created before any game banner and
`CHtmlSysWin_win32::do_leftbtn_down()` returned `TRUE` for essentially every click (it only declined the
vertical-scrollbar strip), so it consumed clicks belonging to banners behind it in the list.

**Fix**: `CHtmlSysWin_win32::pt_in_screen_rect(x, y)` ([htmlgui.h](htmlgui.h)) — a geometric test against
this subwindow's own `get_screen_pos()`/`m_size`. `do_leftbtn_down()` and `do_setcursor()` return `FALSE`
up front when the point isn't inside, so dispatch continues to the sibling that contains it.
`do_rightbtn_down()` forwards to `do_leftbtn_down()`. `do_mousemove()`/`do_leftbtn_up()` are deliberately
untouched — `event_loop()` sends those straight to the capture holder, since a selection drag that leaves
the subwindow must keep receiving them.

### 3.2b `get_focus_subwin()` — Edit > Copy/Cut/Delete/SelectAll were permanently disabled

**Symptom**: found while click-testing §5.4/L's new `Ctrl+C` accelerator — drag-selecting text and pressing
`Ctrl+C` did nothing, and (once checked) `Edit > Copy` from the menu was greyed out too, no matter how much
text was selected.

**Cause**: `CHtmlSys_mainwin::check_command()` forwards `ID_EDIT_CUT`/`COPY`/`DELETE`/`SELECTALL` to
`get_focus_subwin()` - whichever of `main_panel_`/`hist_panel_`/a banner currently has "the input focus" -
and calls *that* window's `check_command()`/`do_command()`, since `can_copy()` etc. test the selection on a
specific window, not the app as a whole. `get_focus_subwin()` found this by comparing `GetFocus()` against
each candidate's `get_handle()`. Both halves of that comparison are dead: every `CTadsWin::handle_` is a
synthetic, never-registered token (§3.4a), so `GetFocus()` never equals one, and `take_focus()` - the
routine that's supposed to claim focus in the first place, called from `do_leftbtn_down()` among others -
had a default implementation of `if (GetFocus() != handle_) SetFocus(handle_)`, itself one of §3.4a's
documented dead no-ops. So `get_focus_subwin()` always returned null, `check_command()` fell through to
`TADSCMD_UNKNOWN`, and every one of these four commands read as permanently disabled - true of the menu
item exactly as much as of the (then-new) accelerator, since both dispatch through the identical
`do_command()`/`check_command()` pair. This had apparently never been click-tested end-to-end before.

**Fix**: a portable substitute, same shape as the existing `CTadsApp::m_mouse_capture_win`/
`setMouseCapture()`/`getMouseCapture()` (added earlier in the port for the analogous broken-mouse-capture
problem): `CTadsApp::m_logical_focus_win`/`setLogicalFocus()`/`getLogicalFocus()` ([tadsapp.h](tadsapp.h)).
`CTadsWin::take_focus()`'s default body now calls `CTadsApp::get_app()->setLogicalFocus(this)` instead of
the dead `SetFocus()` call; `get_focus_subwin()` ([htmlgui.cpp](htmlgui.cpp)) compares
`CTadsApp::get_app()->getLogicalFocus()` against `main_panel_`/`hist_panel_`/each banner by pointer instead
of `GetFocus()`/`IsChild()` against `handle_`. No `IsChild()`-equivalent descendant check was needed:
`take_focus()` is only ever called directly on one of those three kinds of object, never on some deeper
child. Unlike mouse capture (cleared back to null on button-up), logical focus deliberately stays put once
claimed - `Ctrl+C` has to still work after the mouse button that finished the selection is released.

**Verified**: clean build + link, `0 warnings`; interactive `ditch3.t3` test - set the clipboard to a
sentinel string, drag-selected a line of title-screen text (`SetCursorPos`+`mouse_event`, real mouse-drag,
not synthetic keyboard), confirmed the selection highlighted on screen, sent `Ctrl+C` via `SendInput()`,
and read the clipboard back: it now held exactly the selected text, replacing the sentinel.

### 3.3 Dialogs — done, all seven

All seven app dialogs are ImGui-native. The general `CTadsDialog`-mirroring base class this section once
recommended turned out to be unnecessary — each dialog follows the same small shape directly.

| Dialog | Implementation | Replaces |
|---|---|---|
| Options (Edit > Options) | `CHtmlPreferences::open_options_dialog()`/`render_options_dialog()` ([htmlpref.cpp](htmlpref.cpp)) | native property sheet, 8 `CHtmlDialog*PropPage` classes |
| Customize Theme | `open_customize_theme_dialog()`/`render_customize_theme_dialog()` (same file) | `run_appearance_dlg()`, 4 property pages |
| Manage Themes (Themes > Add/Delete Themes...) | `open_manage_themes_dialog()`/`render_manage_themes_dialog()` (same file) | `run_profiles_dlg()`, `CHtmlDialogAppearance` (`DLG_APPEARANCE`), `CTadsDialogNewProfile` (`DLG_NEW_PROFILE`) |
| File open/save | `CTadsFileDialog` ([tadsfiledlg.h](tadsfiledlg.h)/[.cpp](tadsfiledlg.cpp)) | every live `GetOpenFileName()` call site |
| Find | `CTadsFindDialog` ([tadsfinddlg.h](tadsfinddlg.h)/[.cpp](tadsfinddlg.cpp)) | `CTadsDialogFind` (`guifndlg.cpp`) |
| Folder picker | `CTadsFolderDialog` ([tadsfolderdlg.h](tadsfolderdlg.h)/[.cpp](tadsfolderdlg.cpp)) | `CTadsDialogFolderSel2` (`foldsel2.cpp`) |
| License (Help > About HTML TADS > License) | `CTadsLicenseDlg` ([tadslicensedlg.h](tadslicensedlg.h)/[.cpp](tadslicensedlg.cpp)) | `LicenseDlg : CTadsDialog` (inline in `htmlgui.cpp`, `DLG_LICENSE` resource) |

**Manage Themes was the last real Win32 dialog in the port** — `Themes > "Add/Delete Themes..."`
(`ID_MANAGE_PROFILES`) used to call `CHtmlPreferences::run_profiles_dlg()`, a real Win32 `PropertySheet()`
holding the single native `CHtmlDialogAppearance` page (`DLG_APPEARANCE`). That page's whole content — the
theme picker, description field, New.../Delete buttons (with the separate `CTadsDialogNewProfile` name-entry
modal, `DLG_NEW_PROFILE`), "Customize Theme..." and "Reset to Defaults" — was **already** reproduced by
`opt_render_appearance_tab()`, which backs the Options dialog's Appearance tab. So the port is just a second,
standalone framing of that same helper: `render_manage_themes_dialog()` is a `BeginPopupModal("Manage Themes")`
whose body is one `opt_render_appearance_tab()` call plus a Close button, drawn from
`CHtmlSys_mainwin::do_render()` right next to `render_options_dialog()`/`render_customize_theme_dialog()`.
`open_manage_themes_dialog()` snapshots the same `opt_*` working state `open_options_dialog()` does (via
`opt_refresh_profile_list()`/`opt_on_profile_change()`); the two dialogs share that state safely because they
can never be open at once (the Themes menu is unreachable while a modal popup is up). The `ID_MANAGE_PROFILES`
`do_command()` case now calls `open_manage_themes_dialog()` instead of `run_profiles_dlg()`.

- **"Customize Theme..." from inside Manage Themes** behaves exactly as it does from the Options dialog's
  Appearance tab: the button calls `open_customize_theme_dialog()` (deferred flag) and
  `render_customize_theme_dialog()` runs at root level from `do_render()`, so per the "nested modals" trap
  below the Manage Themes popup is evicted rather than stacked under. The native property sheet stacked a
  second sheet on top and returned to the first on close; guit3 drops you back to the game instead. This is
  a pre-existing quirk of the shared Options→Customize path, not new here, and the two flows are deliberately
  interchangeable ("Manage Themes" really is just "Customize Theme" reached from another menu item). If it
  ever needs true stacking, apply the `CTadsFolderDialog::render()` precedent (call
  `render_customize_theme_dialog()` from *inside* the `BeginPopupModal("Manage Themes")` block and guard the
  root-level call with `if (!mt_dlg_open_)`).
- The superseded native `run_profiles_dlg()` / `CHtmlDialogAppearance` / `CTadsDialogNewProfile` are left
  compiled but unused, same as every other replaced dialog. **This does now make `tadscbtn.cpp`
  (`CColorBtnPropPage`) genuinely dead** — its only live consumer was `CHtmlDialogColor`/`CHtmlDialogFonts`
  reached through `CHtmlDialogAppearance`'s "Customize..." button (the §5.3 correction note about it being
  reachable is now stale). It can be dropped in a future cleanup pass.

The superseded native code (`tadsdlg.cpp`, `tadsdlg2.cpp`, `foldsel2.cpp`, `guifndlg.cpp`, the property-page
classes) is left compiled but unused. `guifndlg.cpp`'s `CTadsDialogFindReplace`/`CTadsDialogFindRegex`
belong to the out-of-scope Workbench debugger and were never ported.

**The two call patterns — pick by "is there an ImGui frame in progress?"**

- **`open()` / `render()` (deferred).** `open()` just records a pending flag and a completion callback and
  never touches ImGui state, so it is safe from a click handler however deeply nested. `render()` — called
  once per frame from `CHtmlSys_mainwin::do_render()` — calls `OpenPopup()`/`BeginPopupModal()` and invokes
  the callback when the dialog closes.
- **`open_blocking()`.** For callers with no frame to defer into: `get_game_name_cb()` (runs before
  `event_loop()` starts) and `askfile_hook()` (called synchronously from deep inside VM command
  processing). Self-pumps its own `glfwPollEvents`/`NewFrame`/`Render` loop, sharing the same per-frame draw
  routine via a local lambda that flips a `bool done`. Falls back to the native common dialog if there's no
  GLFW window. It takes an optional `render_background` (`std::function<void()>`) — without it the loop only
  draws the dialog and `glClear()`s everything else, so a mid-game Restore Position dialog appeared over a
  flat fill instead of the running game. `askfile_hook()` passes a lambda calling `win->do_render()` (plus
  the debug window's, matching `event_loop()`); that lambda draws the dialog itself via `do_render()`'s
  normal popup sequence, so `draw_frame()` must **not** also be called or the dialog draws twice.

**Nested modals: the one real trap.** A dialog opened from inside an already-open modal must have its
`render()` called from **inside the parent's still-open `BeginPopupModal` block**, not from the root-level
popup list. Dear ImGui decides a popup's nesting level from `g.BeginPopupStack.Size` **at the moment
`OpenPopup()` is called**. From the root list, the parent's `BeginPopupModal`/`EndPopup` has already
completed for that frame, so the stack size is 0 and ImGui opens the child as a fresh *level-0* popup —
which truncates `OpenPopupStack` and silently **evicts the parent** instead of stacking on it. It still
*looks* nested (the dimmed parent behind a freshly-opened popup is just the previous frame's pixels), but
closing the child takes the parent down with it. `CTadsFolderDialog::render()` is therefore called from
inside `render_options_dialog()`; the reasoning is recorded in [tadsfolderdlg.h](tadsfolderdlg.h) so it
isn't "simplified" back. This only works because that dialog has exactly one known call site — a dialog
needed at multiple depths needs `render()` called from whichever context it was opened under.
**`CTadsFileDialog`'s Game Chest-tab call site has the same latent bug, never yet click-tested.**
**Never conclude a nested popup works because it compiled and a screenshot looked right — click its Cancel
button and confirm the parent survives, or trace `g.BeginPopupStack`/`g.OpenPopupStack`.**

**Design decisions carried through all of them:**

- **No "Apply" staging.** Every control writes straight through to its `CHtmlPreferences` setter on change.
  This matches the original's real persistence model: the setters only mutate the in-memory property list,
  and `CHtmlPreferences::save()` (the actual registry write) runs from the main window's destructor and on
  profile switch — the native Apply button never persisted anything either.
- Modal, self-pumping Win32 APIs (`DialogBoxParam`, `PropertySheet`, `GetOpenFileName`) run their own
  internal message loop and are **safe to call from an ImGui click handler** — verified, not assumed.
  A custom **non-modal** Win32 child control is not, and does nothing at all until ported. That distinction
  is the whole reason this port was needed.

**Reusable ImGui-dialog lessons:**

- **`io.WantTextInput` must gate character forwarding.** `event_loop()` fed `io.InputQueueCharacters`
  straight to `do_char()` unconditionally, so typing into any ImGui text field *also* typed into the game's
  command line. Fixed by wrapping the `do_char()` loop and the Enter-key one in `if (!io.WantTextInput)`.
  This is a **general** gap, not Find-dialog-specific — re-check it's still in place before trusting
  keyboard input in any new dialog, and consider whether `io.WantCaptureMouse` deserves the same treatment.
- **`io.InputQueueCharacters` (GLFW's char callback) never delivers control characters** - only printable
  text, unlike raw Win32 `WM_CHAR`, which did send a real `0x08`/`0x0D`/etc. for Backspace/Enter/etc. That's
  why Enter needed its own `IsKeyPressed(ImGuiKey_Enter) -> do_char('\r', 0)` case above the char loop -
  and why Backspace silently didn't work at the command prompt until it got the same treatment
  (`IsKeyPressed(ImGuiKey_Backspace) -> do_char('\b', 0)`): `do_char()`'s `case 8:` backspace handling
  was already there and correct, just never reached. **Any other control key `do_char()` switches on
  (Tab, Escape, arrow-key history recall, ...) needs the same manual `IsKeyPressed()` case** if it isn't
  already routed some other way (arrow keys/Escape/Tab may be — re-verify per key before assuming one
  "just works" here.)
- **A fresh ImGui popup does not grab keyboard focus** the way a native dialog does. Without an explicit
  `SetKeyboardFocusHere()` on the frame it opens (guarded by a `just_opened` flag), focus stays on the
  game's command line — and then `io.WantTextInput` never goes true and the fix above never engages.
- **An `AlwaysAutoResize` popup visibly grows for a frame or two.** Its size comes from `ContentSizeIdeal`,
  which reflects the *previous* frame's content — near zero for a window that didn't exist last frame. The
  naive fix (capture the size on the opening frame and pin it) is *stable but wrong*, locking in the
  under-measured value and clipping widgets. **Use settle-then-pin**: keep auto-fitting and re-capturing for
  a handful of frames (`FindDlgState::settle_frames`, currently 4), then pin to the last captured size.
  `tadswin_message_box()` and the Save-mode overwrite confirmation still use plain `AlwaysAutoResize` and
  likely show the same brief pop.
- **`RadioButton()` labels do not wrap.** The native one-sentence labels bled past the popup's edge. Shorten
  each to a phrase and put the rest in an indented `TextWrapped()` below it.
- **`ColorEdit3()` needs `ImGuiColorEditFlags_NoInputs`** or the swatch expands into inline R/G/B fields
  that get crushed against whatever's next on the line. `HTML_color_t`↔`ImVec4` reuses
  `HTML_color_to_ImVec4()`/`ImVec4_to_HTML_color()` in [tadswin.h](tadswin.h).
- **An ImGui text field showing a row of `?` on first open means uninitialized memory** — MSVC's debug-heap
  `0xCD` fill has no glyph. That's how `find_text_[0]` never being zeroed was caught.
- **A synchronous virtual interface has to become callback-based** to defer to a later frame.
  `get_find_text()` went from returning `const char *` to taking a
  `std::function<void(const char*, int, int, int, int)>`. `do_find()` now does its
  `execute_find()`/`find_not_found()` work inside the lambda, wrapped in `AddRef()`/`Release()` since the
  callback can fire on a later frame than the request (same precedent as `wait_for_new_game()`).
- **Font enumeration for the Font tab.** `EnumFontFamiliesEx()` used to fill a combo box `HWND` directly.
  `CHtmlPreferences::cust_refresh_font_lists()` enumerates into fixed-size name arrays once when the dialog
  opens, de-duplicating (a face is reported once per style/script). The family filters
  (`cust_font_select_serif/sans/script/typewriter`) are verbatim ports of the original `lfPitchAndFamily`
  logic. `CUST_MAX_FONTS`/`CUST_FONT_NAME_LEN` are `public:` because the file-scope enum callback needs them.
- **`CTadsFileDialog::open()` takes a single `initial_path`** (a directory, a full path to a
  maybe-nonexistent file, a bare filename, or empty) and does the dir/name split once internally via
  `GetFileAttributesA()` then `strrchr('\\')`. Call sites used to inline that logic each. It parses the same
  Win32 `OPENFILENAME::lpstrFilter` multi-string format (`"Desc\0*.ext\0\0"`) callers already had.
- **`os_askfile()` needed a runtime hook, not a compile-time branch.** It lives in
  `../../tads-runner/tads2/msdos/oswin.c` — one static lib (`tr32h`) built **once** and linked by four
  executables (`guit3` plus three still-native Win32 targets), so an `#ifdef IMGUI` inside it wouldn't work.
  Added `oss_set_askfile_hook()`/`os_askfile_hook_t` to `oswin.h`/`oswin.c`, mirroring the existing
  `oss_set_open_file_dir()` shape: `os_askfile()` still builds the filter, initial directory and default
  filename exactly as before, but calls the hook instead of `GetOpenFileName()` when one is registered.
  Unregistered, it's byte-for-byte the old behavior. `guit3` registers `askfile_hook()` in
  [guimain.cpp](guimain.cpp). **The hook signature uses only plain C types** (`const char *filter`, not
  `OPENFILENAME *`) — `oswin.h` has a standing house rule to stay buildable without `<windows.h>`.
  **This is the pattern to copy for any other shared-`oswin.c` behavior guit3 needs to override.**
- **Confirmation prompts (File > Open New Game, File > Exit) use the same deferred pattern**, because both
  fire outside a usable ImGui frame (`do_close()` runs from `glfwPollEvents()` before `NewFrame()`;
  `load_new_game()` runs mid-frame but nested inside `BeginMainMenuBar()`). Each sets a pending flag and
  returns; `render_new_game_confirm()`/`render_quit_confirm()` show the modal and act on the result.
  `do_close()` sets `pending_quit_confirm_` and returns `FALSE`; on Yes, `quit_confirmed_` is set and the
  close re-requested, and `do_close()` skips the prompt on the second pass. Both share
  `render_yesno_confirm_popup()` and use an `..._opened_` bool so `OpenPopup()` is called once per request.
  Enter/Escape activate **No**, matching the originals' `MB_DEFBUTTON2`.

### 3.3a Help > About HTML TADS crashed — missing-resource path had no null check

**Symptom**: Help > About HTML TADS crashed the whole app instantly (`STATUS_STACK_BUFFER_OVERRUN` /
`0xc0000409`, a `/GS` failure inside `ucrtbase.dll`'s `fopen`).

**Cause**: the About box's background image is loaded via the private `exe:about.jpg` resource scheme (see
`CHtmlSys_abouttadswin::build_contents()`, [htmlgui.cpp](htmlgui.cpp)) — normally an EXRS chunk that
`htmlt3`/`htmltdb3` bind into their `.exe` at link time via `maketrx32` (`make_t3r()`/`make_trx()` in the
top-level [CMakeLists.txt](../CMakeLists.txt)). `guit3`'s build never did this, so `guit3.exe` has no EXRS
chunk and no `about.jpg` anywhere. `CHtmlResCache::find_or_create()` ([htmlrc.cpp](../htmlrc.cpp), shared by
every port) still called the image loader (`CHtmlSysImageJpeg::create_jpeg()`) with a null filename when the
resource search came up empty instead of just failing the load like the rest of the function already assumes
— the loader passed that straight to `fopen()`, and the CRT's `/GS` cookie check turned the null-pointer
access into a hard process-kill rather than a normal crash/exception. Fixed with a null check before the
call in `find_or_create()`; this is shared code, so it protects every port from any future missing-resource
case, not just this one.

**Fallback added**: `CHtmlSys_mainwin::load_exe_resources()` now falls back to opening a stand-alone
`<exe-basename>.t3r` file next to the `.exe` (`os_remext()`/`os_addext()` on the exe path) when it can't find
an embedded EXRS chunk, reading it exactly like the embedded case (same `T3-image`/`MRES` header parsing) —
it only needed `exe_fname_` to end up holding whichever file the resources actually came from.
[imgui/CMakeLists.txt](CMakeLists.txt) builds `guit3.t3r` next to `guit3.exe` via the same `make_t3r()` call
`htmlt3` uses, but deliberately skips `make_trx()` (no `maketrx32` embedding step) and installs the `.t3r`
alongside the `.exe` instead.

**Also not centered, and no border**: once it stopped crashing, the dialog was positioned wrong and had no
visible outline - two more legacy-Win32 leftovers in the same class, worth recording together since the
underlying mechanics are non-obvious:

- *Centering*: `CHtmlSys_abouttadswin::run_dlg()` is never a real top-level window - `create_system_window()`
  adds it as a child in `parent`'s `m_children` list, and `CTadsWin::do_render_content_begin()` renders it as
  a `BeginChild()` panel positioned relative to `parent`'s own on-screen position. The inherited Win32 code
  centered against `GetWindowRect(GetDesktopWindow(), ...)` - meaningless once position is parent-relative
  instead of absolute. Fixed by centering against `parent->get_win_size()` instead (a new small public
  accessor on `CTadsWin` - `m_size` itself is `protected`, inaccessible through a plain `CTadsWin*` from a
  sibling class).
- *Border*: the natural fix, adding `ImGuiChildFlags_Borders` to `get_content_child_flags()`, has an
  unexplained side effect: with it set, the dialog's HTML header text (the version string and
  Credits/License/Close links, positioned only a few pixels from the top edge) stops rendering entirely -
  reproduced with `ImGuiStyleVar_WindowPadding` pushed to zero (ruling out the flag's padding side effect) and
  with `ImGuiChildFlags_AutoResizeX/Y` both on and off (ruling out an auto-fit interaction). Root cause not
  tracked down. A hand-drawn `AddRect()` on our own `GetWindowDrawList()` avoids that flag entirely but hits a
  *different* problem: it renders, but gets silently painted over by `html_subwin_`'s opaque background-image
  child (drawn later in the same `do_render()` child loop) - reproduced adding the rect in both
  `do_render_content_begin()` (before children render) and `do_render_content_end()` (after). The fix that
  actually works: draw on `ImGui::GetForegroundDrawList()` instead, in `do_render_content_end()` - the
  foreground list composites after every window regardless of clip rects or submission order within the
  frame, so it can't be painted over or clipped by our own or our children's content. **If a future dialog in
  this port needs a border, reach for this technique first** rather than re-deriving it.

**Also the text/link layout was wrong** compared to `htmlt3` (run it side by side to compare - `cmake --build
--preset default --target htmlt3`, then `Help > About HTML TADS`, or send it `WM_COMMAND`/`ID_HELP_ABOUT`
(40251) directly since it's a real HWND and doesn't need real menu navigation): the version/credits line
wrapped across several lines and stacked top-left instead of laying out along one right-aligned line at the
bottom, and the Credits/License/Close links stacked one-per-line instead of running in a single row.

**Cause**: `get_disp_width()`/`get_disp_height()` (`CHtmlSysWin_win32::disp_width_`/`disp_height_` -
everything layout-sensitive in the formatter reads these: `width=100%` tables, `<tab align=right>`, ...) are
established by `do_resize()`, which a banner/game subwindow always gets via `calc_banner_layout()`. This
dialog's own `html_subwin_` isn't part of that banner tree, though - `CHtmlSys_top_win::do_create()` creates
it directly and calls `create_system_window()`, which only sets `m_pos`/`m_size`, with no WM_SIZE-equivalent
to establish `disp_width_`/`disp_height_` the way a real child HWND creation used to. They were left holding
whatever they were on construction (never explicitly initialized) when `build_contents()`'s `do_reformat()`
call ran the very first, synchronous formatting pass against them.

**Fix**: `do_create()` now calls a new `CHtmlSysWin_win32::establish_disp_size()` right after creating
`html_subwin_`, before formatting anything. It can't just call `do_resize()` directly two ways: that's
`protected` (a plain `CTadsWin*`/sibling class can't reach it - same reason `get_win_size()` above exists),
and `do_resize()` also schedules a *deferred* reformat-after-resize (`onresize_pending_`) whenever the width
passed in differs from `fmt_width_` - always true here, since `fmt_width_` starts at 0 before anything has
ever been formatted. That's redundant with (and, worse, runs a frame after, so actively fights) the caller's
own upcoming synchronous `do_reformat()` - reproduced as a fully blank dialog (background image, title and
body text all gone, only the links independently re-rendered by that second pass surviving) next frame if
left in place. `establish_disp_size()` calls `do_resize()` then immediately clears `onresize_pending_` to
suppress it.

**A red herring worth recording**: chasing that second problem burned real time on a false lead - the
background image and title/body text really did vanish in one build, but it turned out to be nothing to do
with the fix at all: an earlier cleanup pass had deleted `guit3.t3r` from the `tests/` scratch deployment
folder (see §3.3a) and it never got copied back in. Confirmed the same way as the disappearing-border
investigation above - temporary `fprintf()`-to-a-log-file instrumentation traced it to
`CHtmlResCacheObject::get_image()` returning null for a `bg_image_` that was otherwise set (i.e. the resource
lookup itself was failing, not anything downstream) - worth checking `guit3.t3r` sits next to `guit3.exe`
*before* going looking for a rendering bug when content vanishes like this.

**Also asserted on open, in a debug ImGui build only**: `ImGui::ErrorCheckNewFrameSanityChecks()` -
`"Forgot to call Render() or EndFrame() at the end of the previous frame?"` - reported against
`build/vs`'s `Debug` config (`cmake --build build/vs --target guit3 --config Debug`); the `default`
preset's `Release` build never showed it, because `/DNDEBUG` compiles out the plain `assert()` Dear ImGui's
`IM_ASSERT` maps to here. **Always sanity-check a reported assert/crash against a Debug build of the same
target before concluding a fix works** - Release testing throughout this dialog's earlier fixes above never
would have caught this.

**Cause**: `ID_HELP_ABOUT`'s command handler called `CHtmlSys_abouttadswin::run_dlg()` directly, synchronously,
from `do_command()` - itself called from `render_menu_bar()`, from `do_render()`, from *inside* an
already-open `ImGui::NewFrame()`/`Render()` bracket for the current frame. `run_dlg()` pumps its dialog by
calling `event_loop()` again (the same function, reentered) - which calls `ImGui::NewFrame()` a second time
before the outer frame's matching `Render()`/`EndFrame()` ever ran. This is the exact same class of bug
already identified and fixed for File > Open New Game / File > Exit (see the confirmation-prompt bullet in
§3.3) - just a different, not-yet-covered call site hitting it.

**Fix**: `CHtmlSys_mainwin::queue_deferred_dialog(std::function<void()>)` queues a callback instead of
running it immediately; `event_loop()` drains the queue itself, once per iteration, right after
`glfwPollEvents()` and before that iteration's `NewFrame()` - i.e. at a point between two logical frames, at
*every* nesting depth, since the same drain code runs whether it's the outermost `event_loop()` or one
already nested inside a dialog's own (so opening Credits from a link click *inside* the About dialog - itself
mid-frame relative to the About dialog's own nested loop - queues onto and drains from that inner loop, not
the outer one). `ID_HELP_ABOUT` and `CHtmlSys_abouttadswin::show_credits_dlg()` both went through this queue.
Verified against the `Debug` build specifically, per the note above.

**Found while verifying this, fixed in §3.3b below**: at the time, none of the About dialog's own links
(Credits, License, Close) responded to clicks in either build config - `event_loop()`'s mouse routing had no
notion of a currently-open modal `CHtmlSys_top_win` dialog. Turned out to be a symptom of the same root cause
§3.3b fixes (the dialog wasn't a real floating window yet), not a separate bug - see there.

### 3.3b About HTML TADS had no title bar/close button, and Credits rendered garbled on top of it

**Symptom**: following up on §3.3a's fixes, the About dialog still didn't look like a real dialog - no title
bar, no close ("X") button, just the bordered content panel. Once added (below), a second symptom appeared:
opening Credits from within About rendered both dialogs' HTML content garbled together in the same
overlapping rectangle, with only one title bar between them.

**Cause and fix, title bar**: `CHtmlSys_abouttadswin::run_dlg()` had been rendering as a `BeginChild()` panel
nested inside the main window (`create_system_window(parent, ...)` - `parent` non-null), the same as any
banner - fine for a border (§3.3a) but a `BeginChild()` can never have Dear ImGui's own title bar or close
button, only a real `ImGui::Begin()` window can. `CTadsWin::do_render_content_begin()` already had exactly
that "floating overlay window" code path (used by the debug log window, `dbgwin_`) for a null `parent_` - the
fix was to actually route through it: `run_dlg()` now passes a null parent, and two new virtuals,
`get_titlebar_open_flag()`/`on_titlebar_close()` (default: no close button, matching `dbgwin_`'s existing
behavior), let `CHtmlSys_abouttadswin` opt in a close button wired to the same `closing_` flag the "Close"
link already sets. Being a real `Begin()` window also draws its own border and title bar natively, so the
hand-drawn `GetForegroundDrawList()` border from §3.3a became dead weight - removed.

A floating window isn't automatically part of anything, though - it needed two more pieces of plumbing that
`dbgwin_` gets for free from being a single, fixed, mainwin_-known slot: `CHtmlSys_mainwin::push_active_dialog()`/
`pop_active_dialog()` (an actual stack, since these dialogs are created and destroyed dynamically and nest -
Credits opened from within an already-open About) so `event_loop()` knows to `do_render()` it each frame, and
an extension to the same `mouse_over_dbgwin`-style rect test in `event_loop()`'s mouse routing so clicks
reach the dialog's content instead of falling through - and, symmetrically, get swallowed instead of leaking
through to the game underneath when they land elsewhere while a dialog is open (approximating
`modal_dlg_pre()`'s original intent of disabling the windows behind a modal dialog - inert now that none of
these are real HWNDs to disable). **This is what actually fixed the "links don't respond to clicks" gap
flagged in §3.3a** - it was never a separate bug, just downstream of the dialog not being a real window yet.

**Cause and fix, Credits garbled**: `CHtmlSys_creditswin` doesn't override `get_dlg_title()`, so it returns
the exact same `"About HTML TADS"` string as the class it inherits from - harmless for two distinct native
HWNDs (which is what both were when this code was written), but Dear ImGui uses a window's title as its
*identity*, not just its label - so with both dialogs now real `Begin()` windows, an open About and an open
Credits were literally the same ImGui window, `Begin()`'d twice in the same frame. Fixed by building the
title `run_dlg()` actually passes to `create_system_window()` as `"<display text>##dlg<this pointer>"` - the
`##`-suffix is a Dear ImGui idiom for "make the ID unique without changing the displayed label," and the
object's own address is a convenient always-distinct-per-instance token since nothing else reliably
differs between the two classes.

**Also had to fix while making Credits a real window**: centering. `run_dlg()` had been centering each dialog
against `parent->get_win_size()` (`parent` being whichever `CTadsWin` opened it - mainwin_ for About, but the
About window itself for Credits). That's fine while `BeginChild()`-clipped, but once real, a 500-tall Credits
centered inside a 279-tall About box computes a negative top - Credits' title bar ended up rendered behind
About's rather than in a sensible place on screen. Centering against `mainwin_->get_win_size()` unconditionally
instead (matching the original Win32 code, which centered every one of these dialogs against the *desktop*
regardless of which one was being opened or from where) fixed it - both now land centered on the app window,
which is this app's closest equivalent to "the screen" now that there's no real desktop to ask.

### 3.3c Game text was missing its first several pixels/characters on every line

**Symptom**: reported after §3.3b's fixes, but unrelated to them - reproduced from a clean `imgui.ini`, so not
accumulated test-session state either. The main game panel's text was consistently missing its first
character (sometimes more) on every line, with a sliver of unrelated content bleeding in at the opposite
edge - visible from the very first frame of a fresh launch, not something that developed over time.

**Cause**: `CHtmlSysWin_win32::get_scroll_info()`'s horizontal-scrollbar branch divides the visible width
(`rc.right`, from `CTadsWinScroll::get_scroll_area()`) by `get_hscroll_units()` (16) to compute `SCROLLINFO::nPage`,
an `unsigned` field. `get_scroll_area()` starts from `get_client_rect()`, which just echoes `m_size` - and for
the main panel (part of the banner layout tree), `m_size` is only ever set once `calc_banner_layout()` has
run for it at least once. `adjust_scrollbar_ranges()` can run *before* that first pass, though (right after
creation), with `m_size` still `{0,0}` - `get_client_rect()` then returns a zero-width rect, and subtracting
the vertical scrollbar's width (`GetSystemMetrics(SM_CXVSCROLL)`, ~17px) from it goes negative. Dividing that
negative `rc.right` by 16 and storing the (negative) result into `nPage` - `unsigned` - wraps it to just under
`UINT_MAX`, which then feeds bogus values through the rest of the scrollbar-range math (`win_set_scroll_info()`'s
clamping, `do_scroll()`), landing the panel's `hscroll_ofs_` on `1` (one scroll *unit*, i.e. 16px) instead of
`0`. One bad frame is enough: `get_scroll_info()`'s visibility check (`... || hscroll_ofs_ != 0`) then keeps
the (invisible - nothing ever draws a horizontal scrollbar for this panel) scrolled state latched for the
rest of the session, silently shifting every subsequent frame's content left by 16px.

**Fix**: `CTadsWinScroll::get_scroll_area()` now clamps its result to never go negative in either dimension -
matching the real Win32 `GetClientRect()` it stands in for, which can never report a negative width or height
either. Found via the same `fprintf()`-to-a-log-file technique as always (§ below): logging
`get_scroll_info()`'s inputs/outputs for the main panel showed `rc.right=-17` and `nPage` as a huge
`unsigned` value on the very first frames, immediately pointing at `get_scroll_area()`.

### 3.3d The License dialog (Help > About HTML TADS > License) never opened

**Symptom**: clicking the "License" link inside the About HTML TADS box did nothing - no window, no error.
Unlike the plain-broken-link symptom §3.3a/§3.3b fixed for About/Credits (which turned out to be downstream
of About not being a real window yet), the License link's handler, `CHtmlSys_abouttadswin::show_license_dlg()`,
was never touched by that fix: it drove a completely different, still-native code path -
`LicenseDlg : CTadsDialog` (defined inline in `htmlgui.cpp` just above it) calling
`CTadsDialog::run_modal(DLG_LICENSE, ...)`, a real Win32 `DialogBoxParam()` native modal. guit3 has no
message loop to pump a native dialog's `WM_INITDIALOG`/`WM_COMMAND` messages (see §5.4/N), so the call
silently did nothing observable - not a crash, just a dialog that never appeared.

**Fix**: ported to `CTadsLicenseDlg` ([tadslicensedlg.h](tadslicensedlg.h)/[.cpp](tadslicensedlg.cpp)),
following the same `open()`/`render()` deferred pattern as `CTadsFindDialog` (§3.3's table) rather than the
About/Credits "real floating `Begin()` window" pattern - License doesn't need HTML layout or nested-dialog
support, just a read-only scrollable text box and an OK button, the same shape as the other `open()`/`render()`
dialogs. `render()` is called from the same root-level spot in `CHtmlSys_mainwin::do_render()` as
`CTadsFindDialog::render()`/`CTadsFileDialog::render()`, **not** from inside About's window block - and that
turns out to matter: About is a plain `ImGui::Begin()` floating window (§3.3b), not a `BeginPopupModal()`, so
it never enters `g.BeginPopupStack` at all. The "nested modals" trap described in §3.3 (a child `OpenPopup()`
called from outside its parent's still-open `BeginPopupModal` block evicts the parent) only applies between
two real popups - opening a genuine popup from a link click inside a plain `Begin()` window doesn't trigger
it, confirmed by the two staying open simultaneously in testing (License stacked on top of About, About still
present and interactive behind it).

The license text itself comes from the `IDX_LICENSE_TEXT` `TEXTFILE` resource
(`../notes3/license.txt`, embedded via the shared `../win32/htmlt3.rc`). As of the M2/B pass (§5.4/B, §5.5)
`load_license_text()` fetches it through `os_load_license_text()` rather than calling `FindResource` inline;
`tadslicensedlg.cpp` no longer includes `<windows.h>`. The Win32 backend in [guios_w32.cpp](guios_w32.cpp)
is the same `FindResource`/`LoadResource`/`SizeofResource` lookup the old dialog did - a portable backend
supplying an embedded byte array is M3 work. One improvement over the original, kept in the backend: the
byte count comes from `SizeofResource()` rather than assuming the resource bytes are null-terminated (the old
`EM_REPLACESEL` call handed the raw resource pointer to `SendMessage` as if it were a C string, which
happened to work but wasn't guaranteed by the `TEXTFILE` resource format).

### 3.4 Child "windows" (banner border, scrollbars, size-grip, tooltip) — removed

All four extra child controls nested below each window's `handle_` were either purely decorative (never
painted — no message pump) or, for the scrollbars, decorative *plus* incidentally used as external
`SetScrollInfo`/`GetScrollInfo` storage.

- **Banner border** (`border_handle_`, a `"TADS.BannerBorder"` `WS_CHILD` window) →
  `draw_banner_border_imgui()` (`htmlgui.cpp`), called per frame from `do_render_content_begin()`.
  `calc_banner_layout()` already computed a correct `border_rc` every time layout changed; it's now just
  stored, converted to absolute screen coordinates via **`get_parent()->get_screen_pos()`** — *not* this
  window's own position, since `border_rc_` lives in the parent's coordinate space (unlike
  `draw_caret_imgui()`, which draws inside its own window) — and filled via
  `GetForegroundDrawList()->AddRectFilled()` with `IM_COL32(64,64,64,255)` (approximating
  `COLOR_3DDKSHADOW`). The class registration and `border_proc()` were removed.
- **Scrollbars** (`vscroll_`/`hscroll_`) → `vscroll_info_`/`hscroll_info_` (plain `SCROLLINFO` members on
  `CTadsWinScroll`) plus `win_get_scroll_info()`/`win_set_scroll_info()` inline helpers
  ([tadswin.h](tadswin.h)). These **replicate `SetScrollInfo`'s documented `nPos` clamping**
  (`[nMin, max(nMin, nMax-nPage+1)]`), because `do_scroll()` depends on it and re-reads immediately after
  writing specifically to get the clamped value. `vscroll_`/`hscroll_` are still `HWND`-typed and non-null
  but are now opaque tokens (`(HWND)this` / `(HWND)((char*)this+1)`, distinct and non-null), so every
  `hwnd == vscroll_` comparison still works. `vscroll_is_visible()`/`hscroll_is_visible()` and
  `maybe_drag_scroll()` now read `vscroll_vis_`/`hscroll_vis_` directly instead of `IsWindowVisible`; the
  `IsWindowEnabled` half was dropped (nothing ever called `EnableWindow` on these).
- **Size-grip / corner gray box** (`sizebox_`/`graybox_`) — pure decoration, removed outright.
  `set_has_sizebox()` now only tracks the flag for whenever real corner-grip resize gets built.
- **Tooltip** (`tooltip_`, `TTF_SUBCLASS`-ed onto `handle_`) — `TTF_SUBCLASS` only works by intercepting
  real messages dispatched to `handle_`'s window proc, which never happened, so it could never have shown.
  Left permanently `0`; the scattered `SendMessage(tooltip_, …)` calls are harmless no-ops.

`adjust_scrollbar_positions()` is now an empty no-op, kept because several callers would otherwise need
updating for no gain.

### 3.4a `handle_` itself is gone — every `CTadsWin` uses an opaque token

`CTadsWin::create_system_window()` no longer calls `CreateWindowEx` for **any** window. `handle_` is
`reinterpret_cast<HWND>(this)` — opaque, non-null, unique — so every `handle_ != 0` / `hwnd == handle_`
comparison in the still-partly-ported tree keeps working, while every Win32 call that actually touches a
window was replaced. The class registration, `s_winproc` and `common_msg_handler` are dead but left in
place. MDI (`client_handle_`, `CreateWindowEx("MDICLIENT")`) is untouched — Workbench-only (§4).

- **This had to be all-or-nothing.** Tokenising only the two top-level windows breaks immediately: children
  are created with `WS_CHILD` and `parent = mainwin->get_handle()`, and `WS_CHILD` + an invalid parent makes
  `CreateWindowEx` *fail* — the game-text panels get no window and the content area renders solid black.
  Child code also calls `GetClientRect(handle_)` on live layout paths.
- **`do_create()` is now called explicitly** by `create_system_window()` (it used to run via `WM_CREATE`), at
  the same point in the sequence — for a top-level window, before its GLFW `m_window` is created.
  `do_destroy()` likewise: `guimain.cpp`'s `DestroyWindow(...)` calls became `->destroy_now()`.
  `CHtmlSys_mainwin::do_destroy()` still ends in `PostQuitMessage(0)`, which `glfwPollEvents()` picks up as
  `WM_QUIT` and turns into `glfwWindowShouldClose` — still the real termination path.
- **The GLFW incidental pump is the thing this change actually had to replace.** `glfwPollEvents()` on Win32
  runs `while (PeekMessage(&msg, NULL, …)) { TranslateMessage; DispatchMessage; }` — it pumps the *whole
  thread queue*, not just GLFW's windows. So with a real hidden `handle_`, `WM_TIMER` from
  `SetTimer(handle_, …)`, the `HTMLM_REFORMAT`/`HTMLM_ONRESIZE`/`HTMLM_RELOAD_GC` self-posts and
  `PostMessage(handle_, WM_CLOSE)` *were* being delivered. ("There is no message pump" is true of
  `GetMessage`/`DispatchMessage` written in our code, but misses this.) Each was reimplemented:
  - **Timers** → `win_set_timer(id, ms)` / `win_kill_timer(id)` / `tick_timers(now)`, a per-window
    active-timer list. `event_loop()` calls `tick_timers_tree(glfwGetTime()*1000)` once per frame over the
    whole tree; a due timer fires `do_timer(id)` and reschedules (`WM_TIMER` is periodic). Every
    `SetTimer`/`KillTimer` call site converted: input timeout, real-time event callback, `os_setTimer`,
    bg-image animation, temp-link display, drag-scroll auto-scroll, elapsed-time idle timer.
  - **Self-posted messages** → per-window pending flags (`reformat_pending_`/`reformat_flags_`,
    `onresize_pending_`/`onresize_width_`, `reload_gc_pending_`), drained once per frame by
    `run_pending_deferred()` (virtual). `run_pending_deferred_all()` walks main/history/banner panels plus
    the debug log's panel and is called from `event_loop()` **before** `ImGui::NewFrame()` (it's formatting
    work, not drawing).
  - **`PostMessage(handle_, WM_CLOSE)`** → `CTadsWin::request_close()` (`if (do_close()) do_destroy()`).
  - **`SendMessage(handle_, WM_COMMAND, cmd, 0)`** → `do_command(0, cmd, 0)`.
- **Geometry/DC replacements** (mechanical): `GetClientRect(handle_)` → `get_client_rect()`
  (`{0,0,m_size.x,m_size.y}` — our client area *is* our current size, kept fresh by
  `calc_banner_layout()`/`do_render()`); `ScreenToClient`/`ClientToScreen` →
  `screen_to_client()`/`client_to_screen()` (offset by `get_screen_pos()`, the same space `io.MousePos`
  uses); `GetDC(handle_)` → `GetDC(NULL)`; `OpenClipboard(handle_)` → `OpenClipboard(NULL)`;
  `show_normal()`/`bring_owner_to_front()` → `glfwRestoreWindow`/`glfwFocusWindow`/
  `glfwGetWindowAttrib(GLFW_ICONIFIED)`.
- **Dead no-ops left in place**: `InvalidateRect`, `UpdateWindow`, `BeginPaint`/`EndPaint`, `ScrollWindow`,
  `SetFocus(handle_)`, `GetWindow(handle_, GW_CHILD)`, `SendMessage(WM_PALETTECHANGED)`, the native
  `TrackPopupMenu` paths, `CreateToolbarEx(handle_)` (returns NULL, all guards skip).
- **About / Credits / License / in-game popup windows** (`CHtmlSys_aboutgamewin`, `CHtmlSys_abouttadswin`,
  `CHtmlSys_creditswin`, `CHtmlSysWin_win32_Popup`, in the 18k–19k line range of `htmlgui.cpp`) were
  `CTadsWin` subclasses that never ImGui-ported and didn't render in guit3. About and Credits are now real
  floating ImGui windows (§3.3a/§3.3b) and License is now a real ImGui popup (§3.3d); `CHtmlSys_aboutgamewin`
  and `CHtmlSysWin_win32_Popup` are still outstanding (§5.4).

### 3.5 Fonts

`tadsfont.cpp` already used `imgui/misc/freetype/imgui_freetype.h` for glyph rendering. What changed:

- **Removed dead code**: a `pixperinch`/`ptsize` pair computed and immediately discarded, and an orphan
  `logfont->lf.lfFaceName;` expression-statement. Also fixed `m_font` being left uninitialized when
  `GetFontData()` fails (the destructor read garbage).
- **DPI queries** → `CTadsFont::get_screen_dpi()` = `96.0f *
  ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())`, the same content-scale API
  `syswin_create_system_window()` already uses. Replaced `GetDC(GetDesktopWindow())` +
  `GetDeviceCaps(LOGPIXELSX/Y)` in `calc_lfHeight()`/`calc_pointsize()` *and* the duplicate in
  `CHtmlSysWin_win32::get_pix_per_inch()` (which feeds the wider HTML layout engine's point↔pixel math).
- **`CHtmlSysFont_win32` is very much alive** — it's the concrete font class used throughout `htmlgui.cpp`
  and derives from `CTadsFont`. Its `get_win_font_metrics()` GDI query was redundant: FreeType already
  computes the same metrics while loading. Replaced with `get_baked()`
  ([guifont.h](guifont.h)/[.cpp](guifont.cpp)), calling `m_font->GetFontBaked(-logfont_.lf.lfHeight)` and
  reading `ImFontBaked::Ascent`/`Descent`. `descender_height` is `-Descent` (FreeType's is negative;
  `TEXTMETRIC::tmDescent` was a positive magnitude). Fixed-pitch is inferred by comparing
  `GetCharAdvance('i')` against `GetCharAdvance('M')`.
- **Font enumeration is behind a platform hook**: `os_font_family_is_present(const char *name, size_t len)`
  (declared in [tadsfont.h](tadsfont.h)) — one implementation per OS/GUI backend.
  `CTadsFont::font_is_present()` is a one-line forwarder, so its call sites (Wingdings bullets;
  `font-face` fallback-list resolution) are untouched. The `EnumFontFamiliesEx()` implementation moved
  verbatim into `guifont.cpp` with its helpers in an anonymous namespace. A Linux/macOS port adds its own
  file (`fcfont.cpp`, `ctfont.cpp`) implementing the same signature.
- **Font *name* → font file *bytes* is behind a platform hook**: `os_font_data_for_name(name, weight,
  italic, charset, &size)` (declared in [tadsfont.h](tadsfont.h)) — the constructor's
  `CreateFontIndirect()` + `GetFontData()` trick that resolves a font name to its actual TTF/OTF bytes so
  FreeType has something to parse. **FreeType has no system font matching capability** — this is inherently
  OS integration, not rendering, the same underlying problem as `os_font_family_is_present()` — so it moved
  verbatim into [guifont.cpp](guifont.cpp) next to the enumeration one (§5.4/G, done). A Linux/macOS port
  implements the same signature (`FcFontMatch` then read the matched file; CoreText's font URL).

**Crashes and layout bugs fixed here, all worth remembering:**

1. **`GetFontData()` returns `GDI_ERROR` (`0xFFFFFFFF`) for fonts with no scalable outline** — e.g. the
   literal face name `"System"`. Stored into a `size_t` it became ~4GB; `MemAlloc()` was asked for that,
   the second `GetFontData()` also failed and coincidentally compared equal, so the "success" branch ran
   and handed a truncated `-1` to `AddFontFromMemoryTTF`. Fix: check `size != GDI_ERROR` after the probe
   call and leave `m_font` null.
2. **`ImGui::PushFont(nullptr)` does not mean "use the default font"** — it means "keep `g.Font`", and
   asserts if that's also null. `g.Font` *is* null during the first HTML layout pass at startup, before any
   `NewFrame()`. Fix: `CTadsFont::select()` and `get_baked()` fall back to `GetIO().Fonts->Fonts[0]` — the
   `AddFontDefault()` font added in `do_create()` right after context creation, so always valid.
3. **The caret didn't rescale when the Command Font's size changed mid-session.** `caret_pos_` was refreshed
   on every reformat, but `caret_ht_`/`caret_ascent_` were set only by `set_caret_size()`, called once from
   `get_input_begin()`. Fix: `adjust_for_reformat()` now also calls `set_caret_size(formatter_->get_font())`
   (null-guarded) before recomputing the position. Safe for every window type — with an unfinished input tag
   that's the same command font; without one, the caret is never shown anyway.
4. **Em-dashes ran text together.** `measure_text()` converts ANSI→UTF-8 for `CalcTextSize()`, but passed
   `len` (the *original* byte length) as the end offset into the *converted* buffer, truncating the
   measurement for any character that expands under UTF-8. `draw_text()` correctly used `utf8size`. Since
   `measure_text()` drives line-wrapping, the undershoot let too much text be placed. Fix: use `utf8size`.
5. **Text ran under the vertical scrollbar** — three rounds, two of them real-but-insufficient fixes:
   - *False start 1*: fixing (4) removed a coincidental gap that had been clearing the scrollbar.
   - *False start 2*: `render_vscrollbar_imgui()` drew the track at `[rc.right - track_w, rc.right]`, but
     `get_scroll_area()` already subtracts `SM_CXVSCROLL` so `rc.right` **is** the text area's right
     boundary — the track was inside the text area. Moved to `[rc.right, rc.right + track_w]`. Real bug,
     still fixed, not the cause.
   - **Actual cause**: the line-break decision doesn't go through `measure_text()` at all.
     `CHtmlDispText::find_line_break()` (`htmldisp.cpp`, shared cross-platform formatter code) calls
     `win->get_max_chars_in_width()`, which had never been touched by the FreeType migration — it still
     measured via GDI's `GetTextExtentExPoint()` against the `CreateFontIndirect()` font, **a different
     rasterizer than the FreeType glyphs actually drawn**. It reported that more characters fit than
     FreeType would draw. Fix: rewrote it to convert to UTF-16 and accumulate
     `ImGui::GetFont()->GetFontBaked(ImGui::GetFontSize())->GetCharAdvance()` per character. GDI is still
     used to *select* the font, not to measure.
   - Also fixed in passing: `calc_banner_layout()` now sizes the window *before* calling `do_resize()`,
     since `do_resize()` reads the current size (it was seeing pre-resize geometry).

### 3.5a DPI / display scaling

**Model: the monitor content scale is applied exactly once, and `CTadsFont` owns it.**
`CTadsFont::get_dpi_scale()` ([tadsfont.cpp](tadsfont.cpp)) is the single source of truth — the primary
monitor's GLFW content scale (1.0 at 100%, 2.0 at 200%, …), null-monitor-guarded to 1.0.
`get_screen_dpi()` is `96 * get_dpi_scale()` and feeds both the font point↔pixel math (`calc_lfHeight()`,
`calc_pointsize()`) and the wider HTML layout engine via `CHtmlSysWin_win32::get_pix_per_inch()`. So every
`CHtmlSysFont_win32` is rasterised at its DPI-scaled pixel size, and the formatter lays the page out in the
same DPI-scaled coordinate space. The GLFW window is created `size * content_scale` and
`ImGuiStyle::ScaleAllSizes(content_scale)` is called once in `syswin_create_system_window()`
([tadswin.cpp](tadswin.cpp)).

**Trap that bit us: do NOT also set `style.FontScaleDpi`.** The ImGui GLFW example sets
`style.FontScaleDpi = main_scale`, because it adds its fonts at nominal point sizes and relies on that for
the DPI step. `CTadsFont` does the opposite — it bakes the scale into `FontSizeBase` itself — so
`FontScaleDpi` on top multiplied it in a *second* time: on a 200%-scaled display all game text came out at
`2.0² = 4×` and no longer matched the formatter's own metrics. Removed; left at its 1.0 default.

**Chrome that ImGui draws itself must scale its own fixed sizes** — `ScaleAllSizes()` only covers
style-driven spacing, not fonts or explicit `ImVec2` dimensions, and with `FontScaleDpi` gone nothing else
touched them:
- The `AddFontDefault()` font in `do_create()` — the face used by the menu bar, every dropdown menu, the
  dialogs, tooltips and the status line — is now requested at `13px * get_dpi_scale()` via an
  `ImFontConfig`. (It's ProggyClean; upscaled it looks a little chunky. A real system-UI face is §5.4.)
- `render_toolbar()`'s icons are blitted at `TOOLBAR_ICON_WIDTH/HEIGHT * get_dpi_scale()` (the UVs into the
  16×15 icon sheet stay in texel space), and its hand-written `SameLine`/divider offsets scale too.
- Images: the **shared formatter** owns image scaling now. `CHtmlDisp::set_image_scale(double)`
  ([htmldisp.h](../htmldisp.h)) holds a global factor, default **1.0** (what htmlt3 / htmltdb3 / the
  emscripten port rely on - `scale_image_dim()` short-circuits, so those builds are byte-for-byte
  unaffected); guit3's `do_create()` sets it to `CTadsFont::get_dpi_scale()`. `htmldisp.cpp` multiplies
  every on-screen image dimension by it - `CHtmlDispImg` (intrinsic size, explicit `WIDTH=`/`HEIGHT=`, and
  the aspect-ratio fill-in, all uniformly at the end of the ctor), `CHtmlDispListitemBullet`, and
  `CHtmlDispHR`'s image height - and `htmlfmt.cpp`'s `CHtmlFmtMapZone::compute_coord()` /
  `CHtmlFmtMapZoneCircle` scale image-map pixel coordinates and radii to match. This is the guit3 stand-in
  for htmlt3 getting a uniform magnification from the OS scaling its whole (DPI-unaware) window bitmap, and
  it covers the explicit-pixel-size case that a platform-only `get_width()` hack could not.
  - `CHtmlSysImage*_win32::get_width()/get_height()` ([guiimg.h](guiimg.h)) therefore report the image's
    **raw intrinsic pixels** - an earlier version scaled them here, which double-scaled once the formatter
    change went in.
  - `CTadsImage::draw()`'s CLIP/TILE branches still multiply the texture's native size by
    `CTadsImage::disp_scale()` (== the same content scale) so the fill matches the formatter-scaled
    destination rect; STRETCH just fills the rect. The window-background tiling in `htmlgui.cpp`
    (`draw_dc_image`, `inval_html_bg_image`) computes its tile step with `CHtmlDisp::scale_image_dim()` for
    the same reason - it draws the background itself, bypassing the formatter.
- The ported dialogs' hard-coded pixel constants — every `SetNextWindowSize`, fixed `Button` width,
  positive/`-N`-inset `SetNextItemWidth`, `BeginChild` bottom-reservation and literal `SameLine` offset in
  `htmlpref.cpp` (Options, Customize Theme), `tadsfiledlg.cpp`, `tadsfinddlg.cpp`, `tadsfolderdlg.cpp` and
  the two `render_yesno_confirm_popup` / `CTadsWin::message_box` popups — are multiplied by
  `CTadsFont::get_dpi_scale()` (via a file-local `uisc()` in `htmlpref.cpp`, a local `const float s`
  elsewhere). `-1` "stretch to edge" `SetNextItemWidth` sentinels are left alone. The lean standalone dialog
  `.cpp`s pull in `tadsfont.h` for this — it only needs `<imgui/imgui.h>` + `<windows.h>`, both already
  included there.
- The **About HTML TADS / Credits** dialogs (`CHtmlSys_abouttadswin` / `CHtmlSys_creditswin`, ported to real
  floating ImGui windows in the "Migrated about dialog" commit) take their box size from the virtual
  `get_dlg_size()`, which returns a design size in 100%-scaling pixels (478×279, 450×500). Their bodies are
  HTML laid out with the content scale baked in, so `CHtmlSys_abouttadswin::run_dlg()` now multiplies the
  `get_dlg_size()` result by `CTadsFont::get_dpi_scale()` before centring/creating the window — one spot,
  covering both classes. (The "About this game" box, `CHtmlSys_aboutgamewin`, is still on the legacy
  `MoveWindow(handle_)` path — a no-op in guit3 — and isn't covered here. The License dialog was a native
  resource dialog too, but has since been ported to a plain popup, not a scaled `get_dlg_size()` window — see
  §3.3d. See §5.4 for `CHtmlSys_aboutgamewin`.)
  - *Unrelated pre-existing bug fixed in passing:* `CHtmlSys_top_win::do_create()` subtracted
    `SM_CXVSCROLL`/`SM_CYHSCROLL` from the sub-window rect for a scrolling panel (in practice only Credits,
    `panel_has_vscroll() == TRUE`). `CHtmlSysWin_win32` reserves its own scrollbar strip internally now
    (`CTadsWinScroll::get_scroll_area()`; `render_vscrollbar_imgui()` draws the track inside its own client
    area — §3.5), so this shrank the panel a second time and left an `SM_CXVSCROLL`-wide band of the
    dialog's own black background down the right edge of the Credits box. `do_create()` now sizes the
    sub-window the same way `do_resize()` always has — `get_client_rect()` + `adjust_subwin_rect()`.

**Nothing image-related is left unscaled** now that the formatter carries the factor (see the images bullet
above). If a future subclass sizes an image straight from `CHtmlSysImage::get_width()`/`get_height()`, run
it through `CHtmlDisp::scale_image_dim()`.

### 3.6 Images

**GL-texture rendering: done for all types.** Texture upload is centralized in `CTadsImage::create_texture()`
([tadsimg.cpp](tadsimg.cpp)), called from all three loaders: `CTadsImage::create_pix_dword_aligned()` (PNG),
`CTadsJpeg::create_pix_dword_aligned()` (JPEG) and `CHtmlSysImageMng_win32::notify_mng_update()` (MNG,
re-uploaded every animation frame — previously only JPEG uploaded a texture, so PNG/MNG decoded fine but
drew nothing). It converts the DIB-style `pix_` buffer (bottom-up, DWORD-aligned rows, BGR/premultiplied
BGRA) into a top-down straight-alpha RGBA image and uploads as `GL_RGBA` — **the Microsoft OpenGL 1.1
headers don't reliably expose `GL_BGRA`, and ImGui's blend wants straight alpha**, so the conversion is done
in software (same as the toolbar-icon loader).

`CTadsImage::draw()` is GL/ImGui-only now: the GDI blit path (`CreateCompatibleDC`/`StretchBlt`/`AlphaBlend`)
and the 1bpp AND-mask path (`draw_mask()`, `CTadsPng::create_mask()`) are deleted — the mask only existed as
a fallback for Windows versions without `AlphaBlend`, moot under GL. `draw()` also now actually implements
all three `htmlimg_draw_mode_t` modes (a doubled `switch` meant CLIP/TILE silently degraded to STRETCH).

Verified against a test game with a paletted PNG, a JPEG photo, a 32-bit RGBA PNG with partial transparency,
a 24-bit RGB PNG and a stretched image.

Remaining (see §5.4): `alloc_dib()` still allocates `pix_` via `CreateDIBSection`; `get_alphablend_proc()`/
`is_alpha_supported()` still gate whether decoders keep an alpha channel at all; `guiimg.cpp` is Win32-named
but now only does `doc_to_screen()` plus a call into the `CTadsImage` base.

### 3.7 Sound / MIDI

**Digitized audio (WAV/MP3/OGG) output ported to miniaudio; threading and the 'done' callback ported to the
C++ standard library; MIDI gated to Windows.** The DirectSound streaming buffer, the
`CreateEvent`/`CreateThread`/`CRITICAL_SECTION` plumbing and the `HTMLM_SOUND_DONE` window message are gone
from the digitized path.

**The seam.** The engine only knows `CHtmlSysSoundWav`/`Mpeg`/`Ogg`/`Midi` from `htmlsys.h` and their
factories in [guisnd.cpp](guisnd.cpp). Below that: all three digitized types are thin shims over
`CHtmlSysSoundDigitized_win32` → `CTadsCompressedAudio` ([tadscsnd.cpp](tadscsnd.cpp)), which owned the
entire platform surface. The three decoders (`CWavW32`, `CMpegAmpW32`, `CVorbisW32`) only implement
`do_decoding()` and call four protected methods: `open_playback_buffer(freq, bits, channels)`,
`write_playback_buffer(buf, bytes)`, `close_playback_buffer()`, `halt_playback_buffer()`. The decoders
themselves are already portable (`win32/mpegamp/*` is vendored C++, `libvorbis`/`libogg` are vendored
cross-platform libs).

What was done:

1. **Vendored `miniaudio`** ([`htmltads/miniaudio/`](../../miniaudio/)) — single public-domain header pinned
   at 0.11.22 plus a one-line implementation TU. **The `.c` is compiled directly into the `guit3` target**,
   not as a separate `add_subdirectory` lib: an earlier standalone-lib attempt tripped the Visual Studio
   generator (`guit3.vcxproj` picked up the `miniaudio.lib` dependency before `miniaudio.vcxproj` existed in
   the loaded `.sln` → `LNK1104`). Playback only (`MA_NO_DECODING`/`ENCODING`/`RESOURCE_MANAGER`/`ENGINE`),
   since the TADS decoders feed raw PCM.
2. **`CTadsAudioDevice`** ([tadsaudiodev.h](tadsaudiodev.h)/[.cpp](tadsaudiodev.cpp)) — backend-neutral
   open/write/halt/drain/close/set_volume, shaped like the old DirectSound streaming buffer so the decoders
   didn't change. `ma_device` playback + a one-second `ma_pcm_rb` ring buffer; the decoder thread is the lone
   producer and miniaudio's thread the lone consumer, so streaming is lock-free (the device handle is
   mutex-guarded only against a concurrent `set_volume` from a fader thread). Playback auto-starts once a
   quarter of the ring is primed, mirroring the old "start after two chunks" heuristic.
3. **`CTadsCompressedAudio` reworked** — `<dsound.h>`, `IDirectSound*`, `WAVEFORMATEX` and all the DS
   lock/cursor bookkeeping gone; `std::thread`; `refcnt_` is `std::atomic`; `CRITICAL_SECTION` → `std::mutex`.
4. **`CTadsAudioPlayer`/`CTadsAudioVolumeControl`/`CTadsAudioFader` ported**
   ([tadssnd.h](tadssnd.h)/[.cpp](tadssnd.cpp)) — a small `tads_event` (mutex + condition variable,
   manual-reset) modelled on the Win32 events it replaces; `GetTickCount()` →
   `std::chrono::steady_clock`; the `WaitForMultipleObjects` coordination became
   `wait_for_playback_start()`/`wait_stop()`/`stop_signaled()`.
5. **The last `HWND` in the digitized path is gone.** `send_done_message()` enqueues the completed player on
   a thread-safe queue (`tads_audio_post_done_callback`); `event_loop()` calls
   `tads_audio_run_done_callbacks()` once per frame after `glfwPollEvents()`. This also **fixes a latent
   bug**: with no message pump the old `HTMLM_SOUND_DONE` case never fired, so sound-resource queues (looping
   background tracks advancing to the next clip) were silently broken in guit3.
6. **MIDI gated to `#ifdef _WIN32`** — `tadsmidi.cpp`'s whole body and `guisnd.cpp`'s MIDI section; the
   `#else` gives a stub `create_midi()` returning null (a game requesting MIDI plays silent). Windows
   behavior unchanged. The dead `#ifdef HAVE_DXMUSIC` block still references removed members but
   `HAVE_DXMUSIC` is defined nowhere.

Builds and runs clean. **Actual audio playback is not yet runtime-verified** — neither test game has any
sound, so a sound-bearing `.t3`/`.gam` needs sourcing to exercise the WAV/OGG/MP3 path by ear.

Deferred: decoder file I/O (§5.4), the vestigial DirectSound probe (§5.4), a portable MIDI synth (phase two —
TinySoundFont + a bundled GM soundfont through `CTadsAudioDevice`), and the volume curve
(`update_level()` applies a `log10` curve and hands 0..10000 to `set_volume`, which maps *linearly* to
miniaudio's 0..1; the old code applied a second dB mapping on top — revisit once there's a test game).

### 3.8 Scrollback scrollbar

**Symptom**: in `htmlt3` the main text panel grows a working scrollbar on overflow; in `guit3` none ever
appeared and the panel couldn't scroll.

**Two layers:**

1. `do_render_content_begin()` wrapped every child's content in `BeginChild(…, AutoResizeX | AutoResizeY,
   NoInputs)`. `NoInputs` blocks all mouse/wheel input; `AutoResize` means the child always grows to fit, so
   there was never any overflow.
2. **The deeper trap**: fixing (1) and letting ImGui's native content-overflow scrollbar take over does
   *nothing*. `draw_text_clip()` already does `x = doc_to_screen_x(x); y = doc_to_screen_y(y);` before
   `SetCursorPos()` — **the content is pre-windowed into screen-local space via `vscroll_ofs_` before ImGui
   ever sees it**, exactly like the GDI `ExtTextOut` model it was copied from. ImGui's `ContentSize` never
   exceeds one window height regardless of how much text accumulated. `SetScrollY()`/native scrollbars are
   the wrong tool entirely. (Confirmed by instrumenting: `content_height_` climbed past 1000px while the
   largest `y` ImGui saw stayed ~610-630px.)
3. Also found here: `area` (the visible rect passed to `formatter_->draw()`) was **uninitialized** and then
   overridden to a hardcoded `(0,0,10000,10000)` with `area = screen_to_doc(area)` commented out. Correct
   sequence, restored from the dead-but-untouched `do_paint_content()`: `area.set(0, 0, m_size.x, m_size.y)`
   → apply MORE-mode clipping to `area.bottom` while still local → **then** `area = screen_to_doc(area)`.

**Already working, don't rebuild**: `vscroll_ofs_`, `do_scroll()`, `do_mousewheel()`, `get_scroll_info()`, and
auto-scroll-to-bottom (`fmt_adjust_vscroll()`) are inherited unmodified and fully correct. Only a *visible,
interactive* scrollbar and wheel wiring were missing.

**Fix**: `CTadsWin` gained `get_content_child_flags()`/`get_content_window_flags()` virtuals so
`do_render_content_begin()` no longer hardcodes flags. `CTadsWinScroll::render_vscrollbar_imgui()`
([tadswin.cpp](tadswin.cpp)) is a self-contained thumb+track drawn from
`get_scroll_info()`/`get_scroll_area()`, forwarding wheel into `do_mousewheel()` and thumb drag into
`do_scroll(TRUE, vscroll_, SB_THUMBPOSITION, pos, TRUE)`. Called from
`CHtmlSysWin_win32::do_render_content_begin()` right after `formatter_->draw()`. Generalizes to every
`CTadsWinScroll` subclass.

**Three follow-up bugs, each a reusable lesson:**

- **`NoInputs` includes `NoMouseInputs`, which is what makes ImGui skip a window during hover testing.**
  Dropping it for the whole content area (so the thumb could receive input) made ImGui set
  `io.WantCaptureMouse` for *any* hover in that area — and `event_loop()`'s manual routing gates on
  `!io.WantCaptureMouse`, so link clicks anywhere over scrollable text silently stopped working. The
  scrollbar's own `InvisibleButton` kept working (it's a real ImGui item driven by `IsItemActive()`), which
  is exactly why this was missed. **Fix**: `get_content_window_flags()` always returns `NoInputs`; the wheel
  hover test became a plain geometric test of `io.MousePos` against `get_screen_pos()`/`m_size` (not
  `IsWindowHovered()`, which now always reads false); and the track/thumb `InvisibleButton` moved into its
  own tiny nested `BeginChild()` sized to the track rect with ordinary flags — **ImGui's hover scan tests
  each window independently of its parent's flags**, so the nested child stays clickable inside a
  `NoMouseInputs` parent.
- **`SCROLLINFO`'s `nMax` is inclusive.** The code treated `nMax - nMin` as the total range (ImGui's
  `GetScrollMaxY()` convention), but the Win32 thumb travels from `nMin` to `nMax - nPage + 1` and the true
  extent is `nMax - nMin + 1`. The wrong denominator made the thumb partially-sized even when content fit
  (it should be hidden) and stopped it short of the bottom. Now: `total = nMax - nMin + 1`,
  `max_pos = total - page`, used consistently for sizing, position fraction and drag mapping; the bar is
  hidden entirely when `max_pos <= 0`.
- **Dear ImGui was drawing its own scrollbar on top of ours.** The content child is fixed-size but
  `get_content_window_flags()` never added `NoScrollbar`. Any transient overflow in ImGui's own
  content-extent bookkeeping was enough for it to paint its default scrollbar at the same right edge, and
  its default `ScrollbarGrab` grey with the same rounded shape reads as "the thumb fills the whole track" —
  which is what "looks strange in the middle" meant. Fixed by adding
  `ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse`.

**Debugging technique + trap.** `glReadPixels` on the real GL backbuffer (right after
`ImGui_ImplOpenGL3_RenderDrawData`, before `glfwSwapBuffers`) samples exact rendered colors independent of
screenshot/DWM concerns, and disabling a suspect draw call with an early `return` quickly proves whether it's
the source. **The trap**: `glReadPixels` is in GL framebuffer space (origin bottom-left, sized to
`glfwGetFramebufferSize()`), while `CopyFromScreen()` is in OS screen space (origin top-left, sized to
`GetWindowRect()`, which includes title bar and ~8px borders). Cross-referencing coordinates between them
without converting silently samples two different pixels and produces contradictory results. Pick one space
and stick to it — or just zoom into a cropped screenshot (3× nearest-neighbour), which was more reliable here
than reasoning about sampled RGB values.

### 3.9 Blinking text-entry caret

**Symptom**: no caret ever appeared, with or without typing. **Three layers, each masking the next:**

1. `show_caret()`/`hide_caret()`/`update_caret_pos()` called the real Win32 caret API against `handle_`.
   Nothing painted into a GDI device context survives the next `SwapBuffers` of an OpenGL frame. **Fix**:
   `draw_caret_imgui()`, called from `do_render_content_begin()` after `render_vscrollbar_imgui()`, draws a
   filled rect via `GetWindowDrawList()->AddRectFilled()` at `caret_pos_`, blinking on `ImGui::GetTime()`
   modulo ~1060ms (the classic Windows rate). The `show_caret()`/`hide_caret()`/`modal_*` methods now only
   track `caret_vis_`/`caret_modal_hide_` state.
2. `show_caret()` only set `caret_vis_` if `GetFocus() == handle_`. **Win32 focus tracking for a window
   behind a hidden top-level ancestor is unreliable, not just its painting** — `do_setfocus()` fires, but
   `GetFocus() == handle_` reads false immediately after. `is_in_foreground()` had the same disease
   (`GetForegroundWindow() == handle_` can never match the GLFW window) — fixed by asking Dear ImGui:
   `!ImGui::GetIO().AppFocusLost`. **Fix**: dropped the `GetFocus()` requirement entirely
   (`update_caret_pos()` already parks `caret_pos_` at `(-100,-100)` when the window shouldn't accept
   input), and moved the real-app-focus check into `draw_caret_imgui()` — a one-shot gate in `show_caret()`
   can lose a startup race with no retry, a per-frame draw check just tries again next frame.
3. **Something was calling `hide_caret()` right back.** Two sources, both removed: `do_ncactivate()` toggled
   the caret in step with `WM_NCACTIVATE`, whose state bounces independently of real app focus for a hidden
   frame; and `do_setfocus()`/`do_killfocus()` called them directly, with the last startup event routinely
   being a spurious `WM_KILLFOCUS`. Both keep their other jobs (`inval_sel_range()`,
   `notify_parent_focus()`, link-highlight cleanup). `set_caret_size()` is now the only caller of
   `show_caret()`, and calls it unconditionally.

Separately, `get_input_begin()` never called `take_focus()` (unlike `update_input_display()`, which does on
every keystroke), so the first prompt after window creation had no caret until the player typed. Added.

**Debugging technique worth reusing**: when a render-side fix compiles and looks correct but produces no
visible effect, **draw the relevant internal state as on-screen text** via
`ImGui::GetForegroundDrawList()->AddText()` — with a static call counter, or a capped event-trace string for
anything event-driven. That's what found layers 2 and 3, neither of which was visible from reading the code.

## 4. Decisions (resolved)

- **Embedded Web UI** (IE ActiveX, `tadswebctl.*`/`guiwebui.h`): must survive long-term, but is explicitly
  **phase two**. For phase one it should just **compile out cleanly** behind a flag — not be ported and not
  be deleted.
- **Emscripten**: `htmltads/emscripten/` is a deliberately separate, feature-reduced web target driven by the
  browser's event/callback model rather than a continuous render loop. **Not** to be merged into `imgui/`.
  Sequential plan: native `guit3` first, then the web target as its own follow-on, reusing engine-layer code
  but keeping its own platform layer.
- **MDI**: not required. `CTadsSyswinMdiFrame`/`CTadsSyswinMdiClient` exist for the Workbench
  editor/debugger, not the game client. `guit3` is scoped to **porting the client only** — leave the MDI
  machinery unused (or strip it), don't port it. Same for the debugger interfaces (`debugger_ifc_` is always
  null in `guit3`) and `guifndlg.cpp`'s Replace/Regex dialogs.

## 5. Making guit3 platform independent — the remaining plan

Everything in §3 was about getting guit3 off Win32 *windows and widgets*. That is done. What's left is the
**platform layer**: resources, settings, clipboard, cursors, shell, system colors, font lookup, file I/O,
character encoding — plus the build plumbing that still says "Windows only" in three places.

### 5.1 The three gates, and what's behind them

| Gate | File | Currently |
|---|---|---|
| 1 | `tads-runner/CMakeLists.txt:26` | `WITH_HTMLTADS` set only `if (WIN32 OR EMSCRIPTEN)` — the whole `htmltads` tree is skipped elsewhere |
| 2 | `htmltads/CMakeLists.txt:19` | `libogg`, `libvorbis` (and `textindex`, `scintilla`) added only `if (WIN32)` — but `guit3` links `Vorbis::vorbisfile` |
| 3 | `htmltads/htmltads/imgui/CMakeLists.txt:2` | `if (NOT WIN32) return()` — `guit3` itself |

Also in `guit3`'s CMakeLists:

- **`../win32/htmlt3.rc` is in the source list.** There is no resource compiler off-Windows. See §5.4/B.
- **Win32-only link libraries**, each marking a dependency to resolve or drop: `Comctl32.lib` (native
  controls — the last property-page dialog classes that needed it are gone as of the M1 revisit, §5.3, so
  this is now a candidate for removal — verify no other `commctrl` user remains first), `Winmm.lib` (MIDI
  only, already `_WIN32`-gated), `Ws2_32.lib`/`Wininet.lib`/`Mpr.lib` (networking — the superproject already
  vendors `curl/`), `Shlwapi.lib` (`PathMatchSpecA`, §5.4/J), `Version.lib`, `dxguid.lib` (only the
  vestigial DirectSound probe, §5.4/I), `Opengl32.lib` (already `if (WIN32)`-guarded).
  (`Htmlhelp.lib` was here too — **dropped** in the M1 revisit along with the `.chm` help path, §5.4/F.)
- **`Tads::tr32h` / `Tads::t3htm` already have non-Windows branches** (`tads2/CMakeLists.txt` builds
  `unix/osunixt.c` + `ostzposix.c` with `UNIX`/`OSANSI`/`LINUX_386` instead of the `msdos/` set). But `guit3`
  calls into the *Windows* half of that layer: `oss_win_static_init_done()`, `oss_win_free_all()`,
  `oss_G_hinstance`, `oss_set_open_file_dir()`, `oss_set_askfile_hook()`, `oss_is_cmd_event_enabled()`.
  Each needs a portable equivalent or a no-op on the Unix side. The `oss_set_askfile_hook()` precedent
  (§3.3) is the right shape: a runtime hook in the shared `os*.c`, not an `#ifdef`, because `tr32h` is one
  static lib linked by four different executables.

### 5.2 Where the Win32 surface actually is now

Win32 token density (`HWND`/`HDC`/`HINSTANCE`/`HANDLE`/`CreateWindow`/`windows.h`/`DWORD`/… occurrences)
across the files `guit3` actually compiles, largest first:

| File | Refs | Lines | Nature of what's left |
|---|---:|---:|---|
| `tadswin.h` | 144 | 2180 | Mostly **types in signatures** (`HWND`, `HMENU`, `LRESULT`, `RECT`, `SCROLLINFO`) — the handles are already opaque tokens (§3.4a). (Dead MDI subclasses removed in M2/A1; base-class MDI virtuals/handlers remain.) |
| `htmlgui.cpp` | 135 | 19402 | The long tail: codepage conversion, the native-`HMENU` builders (now dead but still `LoadString`/`InsertMenuItem`), the still-unported "About this game"/`CHtmlSysWin_win32_Popup` windows, misc `MessageBox`. (Cursors, clipboard, `GetTickCount`, `GetSysColor`, `ShellExecute` behind hooks — §5.4/D-F; live `LoadString` + the toolbar bitmap loader behind hooks — §5.4/B; `HtmlHelp`/`.chm` help gone — §5.4/F.) |
| `tadswin.cpp` | 127 | 3916 | Same as `tadswin.h` plus the dead window-class registration. (MDI subclass implementations + their `register_win_class()` block removed in M2/A1.) |
| `htmlgui.h` | 98 | 4583 | Types in signatures. |
| `htmlpref.cpp` | 80 | ~3800 | Registry (theme profiles), `GetCurrentDirectory`, `EnumFontFamiliesEx`. The ~1600 lines of dead native property-page classes (`CHtmlDialog{FontPp,Fonts,Color,More,Media,Appearance}`, `CTadsDialogNewProfile`, `run_appearance_dlg()`, `run_profiles_dlg()`) were removed in the M1 revisit (§5.3). |
| `tadsdlg.cpp`/`.h`/`tadsdlg2.cpp` | 49/46/20 | 2463 | Dead except three entry points — §5.3. |
| `tadswebctl.h`/`guiwebui.h`/`tadscom.h` | 36/13/22 | — | Web UI, to be gated out — §5.4/O. |
| `tadsapp.cpp`/`.h` | 25/17 | 2000 | `MSG` pump, accelerators, modeless list, MDI — mostly dead — §5.4/L. |
| `tadsreg.cpp` | 21 | 419 | Registry — §5.4/C. |
| `guimain.cpp` | 21 | 956 | Startup/shutdown — §5.4/M. |
| `tadsmidi.cpp` | 20 | 2219 | Already `#ifdef _WIN32`. Phase two. |
| `foldsel2.cpp`/`guifndlg.cpp`/`iconmenu.cpp` | 20/16/12 | 2351 | Dead — §5.3. |
| `tadsimg.cpp`/`.h` | 15/8 | 700 | **Win32-free now (M2/A2/H).** `alloc_dib()` is a plain `os_alloc_huge()`; the `AlphaBlend` dynamic-link gate is gone (alpha always on). Dead `<windows.h>`/`tadsapp.h` includes remain per the A1 convention. |
| `tadsvorb.cpp`/`tadswav.cpp`/`mpegamp_w32.cpp`/`tadscsnd.cpp` | 10/8/9/2 | — | **Win32-file-API-free now (M2/A2/I).** Decoder file I/O is on the portable `osfile` API; the packed WAV struct parse is explicit LE field reads. `getbits.cpp` forked into `imgui/` for the same reason. Dead `<Windows.h>` includes remain (A1 convention). |
| `tadsfont.cpp`/`guifont.cpp`/`tadsfont.h` | 4/6/3 | 493 | The GDI font-bytes lookup and enumeration — §5.4/G. **A2 seam done**: both are behind `os_font_data_for_name()` / `os_font_family_is_present()` in `guifont.cpp`; what's left here is the fontconfig/CoreText backends (M3). |
| `tadsfiledlg.cpp`/`tadsfolderdlg.cpp` | 5/2 | — | `FindFirstFileA`/`PathMatchSpecA` — §5.4/J. |

Two things this table understates:

1. **Types dominate.** A large fraction of the counts in `tadswin.h`/`htmlgui.h`/`tadswin.cpp` are `HWND`,
   `RECT`, `POINT`, `DWORD`, `BOOL`, `LRESULT` in declarations of functions that no longer touch Windows.
   Those are a typedef problem, not a porting problem (§5.4/A1).
2. **The `.rc` resource file is a single point of failure** that no per-file count reflects (§5.4/B).

### 5.3 Cut before you port

Roughly 5,000 lines currently compiled into `guit3` are unreachable. Removing them from the source list is
the cheapest possible progress and shrinks everything downstream (link libs, includes, the type shim).

| File | Lines | Status |
|---|---:|---|
| `tadscbtn.cpp`/`.h` | 231 | ~~"zero references anywhere" - drop now.~~ ~~**Wrong - not dropped.** `CColorBtnPropPage` reachable through the still-native `Manage Profiles > Customize` path.~~ **Done (M1 revisit).** Once `ID_MANAGE_PROFILES` stopped calling `run_profiles_dlg()` (§3.3, Manage Themes port), the whole chain `run_profiles_dlg()` → `CHtmlDialogAppearance` → `run_appearance_dlg()` → `CHtmlDialog{Fonts,Color,More,Media}` → `CColorBtnPropPage` (plus `CHtmlDialogFontPp` and `CTadsDialogNewProfile`) went dead. All ~1600 lines removed from `htmlpref.cpp` and `tadscbtn.cpp`/`.h` deleted outright; `#include "tadscbtn.h"` and the `tadscbtn.cpp` source-list entry removed. Verified with a clean Debug build. |
| `guifndlg.cpp` | 908 | Superseded by `CTadsFindDialog` (§3.3). `CTadsDialogFindReplace`/`FindRegex` are Workbench-only. **Done (M1)** - genuinely zero live references; dropped cleanly. |
| `foldsel2.cpp` | 1011 | Superseded by `CTadsFolderDialog` (§3.3) for the live (ImGui Options/Starting tab) path. **Done (M1)**, but its only remaining consumer, `CHtmlDialogStart`, turned out to live inside a larger dead block: `htmlpref.cpp` also had `CHtmlDialogKeys`/`Safety`/`NetSafety`/`Mem`/`Quit`/`Start`/`GameChest` (the 7 property-page classes used *only* by `run_preferences_dlg()`, itself unreachable - `CHtmlDialogAppearance` was at the time the one page still shared with the then-live `run_profiles_dlg()`, so it alone survived that pass - it and the rest of that chain were removed in the M1 revisit once Manage Themes was ported, §3.3 / the `tadscbtn.cpp` row above) plus `run_preferences_dlg()` itself, all confirmed dead by the same "no call sites outside this block" check and removed together (~1750 lines) so the `foldsel.h` include could actually go. Watch for this pattern elsewhere in `htmlpref.cpp`: a file's "zero references" claim in this doc may only hold once a further, uncounted dead block that references it is cleared out too - verify reachability transitively, not just by grepping the target file's own name. |
| `iconmenu.cpp` | 432 | Owner-drawn menu icons - dead (no real `HMENU` is ever shown). **Done (M1)**, but there were more live-but-inert call sites than this row implied: besides the `do_create()` construction, `create_toolbar()` (already-dead native toolbar code, §3.1) called `add_bitmap()`/`map_commands()`/`map_command()` ×3 every time it ran, and `do_destroy()` deleted the object. All of it executed at startup with no visible effect (no real menu bar ever receives `WM_INITMENUPOPUP`/`WM_DRAWITEM`) - removed all of it, then the file. |
| `tadsdlg.cpp`/`.h`/`tadsdlg2.cpp` | 2463 | Dead **except** `CTadsDialog::modal_dlg_pre()`/`modal_dlg_post()` and `set_filedlg_center_hook()`, still used by `CHtmlSys_abouttadswin`/`CHtmlSys_creditswin::run_dlg()` and (once ported) `CHtmlSys_aboutgamewin` (§5.4/N). `LicenseDlg : CTadsDialog`, the last of the four, is gone now that License is ported (§3.3d). Port `CHtmlSys_aboutgamewin`/`CHtmlSysWin_win32_Popup`, then drop these. |
| `tadsole.cpp` | 430 | `CTadsDataObjText`, used by `htmlgui.cpp:1585`'s `get_data_object()` (OLE drag-and-drop source). Gate with the Web UI/COM flag (§5.4/O) or drop drag-out support off-Windows. |
| `tadswebctl.cpp`/`tadscom.cpp`/`guinogch.cpp` | ~470 | Web UI / COM — gate behind `TADS_WEBUI_ENABLED` (§4, §5.4/O). |
| MDI in `tadswin.h`/`.cpp` | — | `CTadsSyswinMdiFrame`/`MdiClient`/`MdiChild`, `client_handle_`. Workbench-only (§4). **Done (M2/A1).** The three subclasses + their `register_win_class()` registration block removed from `tadswin.h`/`tadswin.cpp`; `CTadsApp::set_mdi_win()`/`mdi_win_` + the `TranslateMDISysAccel()` branch in `process_message()` removed from `tadsapp.h`/`tadsapp.cpp`. The base-class `CTadsWin::mdi{child,frame}_message_handler()` and `mdi*_win_class_name` were left as unreferenced dead native code (base-class MDI plumbing is a broader native-dead-code sweep, not A1). |

### 5.4 The work items

Grouped by subsystem. Each is independently landable on Windows first (nothing here needs a Linux build to
verify) — that's deliberate: **do not flip the gates until most of this is done**, or the first Linux build
produces thousands of errors at once with no way to bisect them.

**A. The portable seam — do this first, everything else plugs into it.**

- **A1. `tadsplat.h`, a type compatibility header.** For non-Windows, define the Win32 types guit3's headers
  still traffic in: `HWND`/`HDC`/`HMENU`/`HACCEL`/`HCURSOR`/`HINSTANCE` (all `void *` — they are already
  opaque tokens, §3.4a), `RECT`/`POINT`/`SIZE`/`SCROLLINFO`/`LOGFONT`/`MSG` (plain structs — the
  `SCROLLINFO` store is ours now, §3.4), `DWORD`/`BOOL`/`LRESULT`/`WPARAM`/`LPARAM`/`COLORREF`, and the
  handful of constants still referenced (`SB_*`, `SW_*`, `MB_*`/`ID*`, `TRUE`/`FALSE`). **Prefer this over
  renaming**: it is ~150 lines against ~26,000 lines of call sites, and it keeps the Windows build
  byte-identical. Strip MDI at the same time (§5.3).
- **A2. `os_*` hook headers per subsystem, one implementation file per platform.** The precedent is already
  set by `os_font_family_is_present()` (§3.5): declare the hook in the neutral header, put the Windows
  implementation in the `gui*`/`*_w32` companion file, and select per-platform files in CMake. Apply the
  same shape to every item below. Introduce the hook *now*, with only the Windows backend behind it, rather
  than deferring the abstraction until a second platform exists.

**B. Resources — the single biggest blocker.** `guit3` compiles `../win32/htmlt3.rc`, which pulls in
`htmlcmn.rc` (1117 lines: 63 `IDS_` string-table entries, 4 bitmaps, 1 cursor, 18 icons, 2 menus, 2
accelerator tables, 26 dialogs) plus `IDX_LICENSE_TEXT`. Live consumers in the ImGui build:

- `LoadString()` — ~28 call sites (23 in `htmlgui.cpp`). → `os_load_string(int id, char *buf, size_t len)`
  backed by a generated `{id, text}` table. Generate it from `htmlcmn.rc`'s `STRINGTABLE` at build time, or
  hand-convert once into a `guires.cpp` and keep the `.rc` authoritative only for Windows.
- `LoadImage(IDB_TERP_TOOLBAR)` + `GetDIBits()` — the toolbar icon atlas (§3.1). → embed `runtbar.bmp` as a
  byte array (or convert to PNG) and feed it through the existing `CTadsImage::create_texture()` path,
  keeping the color-key→alpha conversion.
- `LoadCursor(hand)` — → `glfwCreateStandardCursor(GLFW_HAND_CURSOR)`; see D below.
- `FindResource`/`LoadResource` for `IDX_LICENSE_TEXT` — embed `notes3/license.txt` as a byte array.
- The app icon and version info can stay in the `.rc` for the Windows build only; GLFW takes an icon via
  `glfwSetWindowIcon()` from raw pixels on all platforms.
- Menus, dialogs and accelerator tables in the `.rc` are already dead (all reimplemented in ImGui) — nothing
  to port, they just stop being compiled.

**M2/A2 seam landed (see §5.5).** The three live resource kinds now go through hooks in
[guios.h](guios.h) / [guios_w32.cpp](guios_w32.cpp), Windows backends lifted verbatim, Windows build
byte-identical:

- `os_load_string(int id, char *buf, size_t buflen)` — same `LoadString()` contract (returns chars copied).
  All **live** call sites route through it: `load_res_str()` and ~15 others in `htmlgui.cpp`, the theme-
  description lookup in `htmlpref.cpp`, the game-name prompt in `guimain.cpp`. The ~6 remaining direct
  `LoadString()` calls in `htmlgui.cpp` are inside dead native-`HMENU` builders (`InsertMenuItem` /
  `MENUITEMINFO` blocks — the ImGui menus replaced them, §3.1); `tadswin.cpp`'s native menu-label updater,
  `tadsdlg2.cpp`, and `guimain.cpp`'s COMCTL32-version guard are likewise Windows-only dead/startup code and
  keep their `<windows.h>` `LoadString` per the A1 convention.
- `os_load_toolbar_rgba(int *w, int *h)` — the `LoadImage(LR_CREATEDIBSECTION)` + `GetDIBits()` + BGRA→RGBA
  + color-key→alpha block moved out of `CHtmlSys_mainwin::load_toolbar_texture()`, which keeps only the GL
  upload. Returns a `th_malloc()`'d buffer (was `new[]`).
- `os_load_license_text(size_t *len)` — the `FindResource`/`LoadResource`/`SizeofResource` lookup moved out
  of `tadslicensedlg.cpp`, which is now `<windows.h>`-free.

The `LoadCursor(hand)` item was folded into ImGui's own hand cursor when the hover-shape cursors moved to
`ImGui::SetMouseCursor()` (see D); the app icon / `glfwSetWindowIcon` is left for M3. **What remains for B is
the portable half** — the generated string table and the embedded `runtbar.bmp` / `license.txt` byte
arrays — scheduled under M3.

**C. Settings storage.** `CTadsRegistry` (`tadsreg.cpp`, 419 lines) plus direct `RegEnumKeyEx`/`RegDeleteKey`
in `htmlpref.cpp` (4 sites, theme-profile enumeration and deletion) and `htmlgui.cpp`
(`render_themes_menu_items()` enumerates the `<prefs>\Profiles` key directly, §3.1). → a `CTadsSettings`
interface with the same open/query/set/**enumerate-subkeys**/delete shape, registry-backed on Windows and
file-backed (INI or JSON under `$XDG_CONFIG_HOME` / `~/Library/Preferences`) elsewhere. **Design the
subkey-enumeration primitive into the interface deliberately** — the theme-profile feature depends on it and
a flat key/value file doesn't give it for free.

**D. Services GLFW already provides — cheapest wins, do them early.**

- **Clipboard** (`htmlgui.cpp` ~1315–1503, plus `tadsole.cpp`): `OpenClipboard`/`GlobalAlloc`/`GlobalLock`/
  `SetClipboardData`/`GetClipboardData`/`EmptyClipboard` → `glfwSetClipboardString`/`glfwGetClipboardString`.
  Keep the existing CR/LF normalization; drop the `GlobalAlloc` handle dance entirely.
- **Cursors** (`htmlgui.cpp` ×10 `SetCursor`, `tadswin.cpp` ×2 `LoadCursor`): →
  `glfwCreateStandardCursor(GLFW_ARROW_CURSOR / GLFW_IBEAM_CURSOR / GLFW_HAND_CURSOR)` + `glfwSetCursor`.
  Note the wait cursor is set around long synchronous operations (`htmlgui.cpp:3045`, `:7232`, `:14360`) that
  run *outside* the frame loop — `glfwSetCursor` is still the right call there.
- **`GetTickCount()`** (`hos_gui.cpp:96`'s `os_get_sys_clock_ms`, `htmlgui.cpp` ×3) →
  `std::chrono::steady_clock`, exactly as the audio layer already did (§3.7).

**E. System colors.** `GetSysColor`/`GetSysColorBrush` for `COLOR_HIGHLIGHT`, `COLOR_HIGHLIGHTTEXT`,
`COLOR_WINDOW`, `COLOR_3DSHADOW`, `COLOR_3DHILIGHT`, `COLOR_3DFACE` (`htmlgui.cpp`, ~10 sites) back the
selection highlight and the "Use Windows colors" preference. → `os_get_sys_color(enum)` with the Win32
lookup on Windows and fixed sensible values (or the ImGui style palette) elsewhere. Small and self-contained.

**F. Shell integration.**

- `ShellExecute` — `guitr.cpp:206` (open a URL from the game), `htmlgui.cpp:9749`/`:13854`/`:18625`. →
  `os_open_url()`: `ShellExecute` / `xdg-open` / `open`.
- `HtmlHelp` (`htmlgui.cpp`, `ID_HELP_CONTENTS`) — obsolete bundled `.chm` help. **Dropped (M1 revisit),
  as originally planned** (the earlier "kept, wrapped in `#ifdef _WIN32`" decision was reversed on explicit
  direction once no native dialogs remained). `ID_HELP_CONTENTS` now just does
  `ShellExecute(0, 0, "https://www.tads.org/", ...)` — the same one-liner shape as the adjacent
  `ID_HELP_WWWTADSORG` case — so it folds into the `os_open_url()` work above with no `.chm`, no
  `#include <htmlhelp.h>`, and no `Htmlhelp.lib`. Two loose ends for whoever does `os_open_url()`: route
  this through that hook rather than a bare `ShellExecute`, and deep-link to the actual HTML TADS manual
  page instead of the tads.org site root (confirm the URL against tads.org's doc layout).

**G. Fonts — two OS-integration hooks, not a rendering problem. A2 seam done (see §5.5).**

- `os_font_family_is_present()` — the hook exists (§3.5); it still needs a fontconfig backend (`fcfont.cpp`)
  and a CoreText one (`ctfont.cpp`) — that portable half is M3.
- `os_font_data_for_name(name, weight, italic, charset, &size)` — **done.** Font *name* → font file *bytes*,
  the `CreateFontIndirect()` + `GetFontData()` trick lifted verbatim out of `CTadsFont`'s constructor into
  [guifont.cpp](guifont.cpp) next to the enumeration one; the constructor now calls the hook. FreeType
  cannot do system font matching. Returns an `ImGui::MemAlloc()`'d buffer the caller hands straight to
  `AddFontFromMemoryTTF()`. The reduced signature (name/weight/italic/charset, not the whole `LOGFONT`) is
  safe because every live call site fills in a concrete face name before constructing the font
  (`CHtmlSysWin_win32::get_font()`), so `lfPitchAndFamily` substitution never applies, and height/decoration
  fields don't change which file `GetFontData()` returns. Linux backend: `FcFontMatch` then read the matched
  file. macOS: CoreText's font URL.
- `get_max_chars_in_width()` — **done.** No longer opens a throwaway DC: the old
  `GetDC`/`select_font`/`ReleaseDC` dance only existed to get the ImGui font pushed, so it now calls the new
  DC-free `CTadsFont::push_imgui_font()` (the ImGui half of `select()`, factored out; `select(HDC)` now just
  calls it then `SelectObject`s). The width loop already ran entirely off FreeType-baked glyph advances.
  `measure_text()` still uses GDI (`GetTextMetrics`/`GetTextExtentPoint32`) and keeps its DC — untangling
  that is separate.

**H. Images.** `alloc_dib()`'s `CreateDIBSection` is now *only* an allocator (nothing blits the DIB) → plain
`os_alloc_huge()`. `get_alphablend_proc()`/`is_alpha_supported()` gate whether decoders keep an alpha channel
at all; off Windows this must simply return true, and arguably should on Windows too since GL always blends.
`guiimg.cpp`'s `*_win32` class names are cosmetic — rename opportunistically, not as a task.

**A2 seam done — and it needed no `os_*` hook.** Unlike D–G, item H had nothing to *abstract*: the two Win32
calls just get deleted, because their replacements are already portable or constant.
[tadsimg.cpp](tadsimg.cpp) / [tadsimg.h](tadsimg.h) / [tadsmng.cpp](tadsmng.cpp) / [tadspng.cpp](tadspng.cpp):

- `CTadsImage::alloc_dib()` no longer creates a GDI DIB section against the desktop DC — it is a plain
  `os_alloc_huge()` (already a portable `th_malloc()` alias in [hos_gui.h](hos_gui.h)) of
  `dword_aligned_row_bytes * height_`, `memset` to zero to match `CreateDIBSection`'s zero-fill (an
  interlaced MNG reads the canvas before every pixel is written). The buffer keeps the DIB memory layout
  the decoders' row walkers and `create_texture()` still assume (rows bottom-up, each padded to a 4-byte
  boundary); only the GDI object is gone. The `HBITMAP dibsect_` member and its `DeleteObject()` teardown
  are removed; `delete_image()` `os_free_huge()`s `pix_` instead. The name `alloc_dib()` is kept — the
  *layout* it produces really is a DIB layout, and renaming would only churn three call sites.
- Alpha is now unconditionally supported. `CTadsImage::is_alpha_supported()` returns `TRUE`;
  `create_pix_dword_aligned()` keeps the alpha channel whenever the input has one
  (`in_bytes_per_pixel >= 4`), with no `get_alphablend_proc()` gate. The whole dynamic-link machinery —
  `get_alphablend_proc()`, `alphablend_proc_`, `linked_alphablend_proc_`, the Win98 exclusion, the
  `LoadLibrary("Msimg32.dll")` — is deleted. `CTadsMng::init_mng_canvas()` always asks libmng for a
  `MNG_CANVAS_BGRA8PM` canvas when the image has transparency (the old non-AlphaBlend fallback that
  pre-composited alpha onto a gray `mng_set_bgcolor()` background is gone); `CTadsPng::init_alpha_support()`
  is now an empty stub (it only ever set `HTMLPNG_OPT_NO_ALPHA`, which never applied any more).
  `CTadsImage::disable_alpha_support()` (the `-noalphablend` command-line option) is a no-op stub so the
  option is still accepted and ignored.
- `tadsimg.cpp` no longer calls any Win32 API (its `<windows.h>` / `tadsapp.h` includes are now dead but
  left in place per the A1 "`.cpp` files keep `<windows.h>` until M4" convention). `guiimg.cpp`'s cosmetic
  `*_win32` class names were left alone as the item says.

**Verified**: clean build + link of `guit3`, `0 warnings` on the four touched TUs (plus `htmlgui.cpp`, which
includes `tadsimg.h`), and a fresh smoke-test launch on `ditch3.t3` — the game's `RETURN to DITCH DAY`
cover **JPEG renders with correct colors and no corruption**, which exercises the new `alloc_dib()`
allocator and `create_pix_dword_aligned()` end to end (`create_jpeg()` → `load_from_jpeg()` →
`create_pix_dword_aligned()` → `alloc_dib()` → `create_texture()`); toolbar, menu bar and status-bar
elapsed clock intact, no charmap warning, no crash. PNG/MNG transparency was not exercised (nothing in
`tests/` uses it) but the decoder changes are the same constant-fold of a now-always-true condition.

**I. Audio — the decoders' file I/O is the last real blocker for a non-Windows digitized path.**
`CreateFile`/`ReadFile`/`SetFilePointer`/`CloseHandle` in `tadscsnd.cpp` (the shared `in_file_`),
`tadswav.cpp` (header + data reads), `tadsvorb.cpp` (the `datasource_t` callbacks) and `mpegamp_w32.cpp`
(`get_input()`). → the TADS `osfile` API (`osfoprb`/`osfrb`/`osfseek`/`osfpos`) already available through
`tr32h`. Mechanical but spans five files, and includes packed-struct WAV header parsing
(`PCMWAVEFORMAT`/`WAVEFORMATEX`) that should become explicit little-endian field reads. **Get a
sound-bearing test game first** — nothing in `tests/` has audio, so this is currently unverifiable by ear
(§3.7). Also here: drop `CHtmlSys_mainwin::get_directsound()`'s `LoadLibrary("DSOUND.DLL")` version probe
(nothing uses the returned `IDirectSound*` any more) in favour of a plain "audio available" bool from
miniaudio's `ma_context` init — that removes `dxguid.lib`. **Done (M1).** Added
`CTadsAudioDevice::is_available()` (`tadsaudiodev.h`/`.cpp`) — a cheap `ma_context_init()`/`ma_context_uninit()`
round-trip with no device opened. `get_directsound()`'s *signature* was left untouched (still returns
`IDirectSound *`) rather than plumbed through the 4 declaration sites and 2 call sites that all only ever compare
it against 0 — it now caches a non-null sentinel (`reinterpret_cast<IDirectSound *>(1)`, never dereferenced) for
"available" instead of a real COM pointer, and the destructor's `directsound_->Release()` had to be dropped
accordingly (nothing to release any more). All DirectSound WinAPI calls, the `DSBCAPS_CTRLDEFAULT` macro shim,
and `#include <dsound.h>` are gone from `htmlgui.cpp`; `dxguid.lib` is gone from `CMakeLists.txt`.

**A2 seam done — the digitized-audio path is Win32-file-API-free.** Every `CreateFile` / `ReadFile` /
`SetFilePointer` / `CloseHandle` in the WAV, Ogg and MP3 decoders now goes through the portable TADS
`osfile` API (`osfoprb` / `osfrb` / `osfrbc` / `osfseek` / `osfpos` / `osfcls`, `OSFTBIN`), already linked
via `tr32h`. `CTadsCompressedAudio::in_file_` is an `osfildef *` (was `HANDLE`) and the shared
`do_decoding()` virtual takes `osfildef *fp` (was `HANDLE hfile`); a failed open is now `0` rather than
`INVALID_HANDLE_VALUE`, so `guisnd.cpp`'s post-`create_player()` check compares against `0`.

- **`tadscsnd.cpp`** — the shared `in_file_` open/seek/close (`osfoprb(fname, OSFTBIN)` +
  `osfseek(..., OSFSK_SET)` + `osfcls`). Header now `#include <os.h>` for `osfildef`.
- **`tadswav.cpp` / `tadswav.h`** — `get_track_len_ms()`, `read_header()` and `read_data()` on `osfile`.
  The packed-struct WAV parse is gone: `tads_wav_hdr_info` no longer holds a heap-allocated `WAVEFORMATEX`
  read straight off disk; it holds a plain `tads_wav_format` whose six fields are filled by explicit
  little-endian reads (`osrp2` / `osrp4`) of the 16-byte common "fmt " header. Format-specific bytes after
  that header (the Win32 `cbSize` + extra data) are skipped by the existing seek-to-next-subchunk step —
  nothing consumed them. `WAVE_FORMAT_PCM` → local `TADS_WAVE_FORMAT_PCM`; the dead `get_wavefmtex()`
  accessor became `get_wave_format()`.
- **`tadsvorb.cpp` / `tadsvorb.h`** — the `datasource_t` context and the four `ov_callbacks`
  (`cb_read`/`cb_seek`/`cb_close`/`cb_tell`) stream from an `osfildef *`; `cb_read` uses `osfrbc`'s
  returned count, `cb_seek` resolves every `SEEK_*` to an absolute `osfseek(..., OSFSK_SET)` keeping the
  stream-base offset so an embedded `.ogg` resource still seeks within its own slice. `get_track_len_ms()`
  likewise. The unused 64-bit `SetFilePointer` high-word dance and `INVALID_SET_FILE_POINTER` shim are
  gone.
- **MP3** — `CMpegAmp::in_file` stays declared `HANDLE` in the *shared* `../win32/mpegamp/mpegamp.h`
  (untouched, so the legacy `htmlt3` build is unaffected). Only **`getbits.cpp` was forked into `imgui/`**
  (joining the existing `mpegamp_w32.cpp`/`.h` forks): its two file-touching functions, `get_input()` and
  `dummy_getinfo()`, now call `osfrbc` / `osfseek` on `(osfildef *)in_file`. `mpegamp_w32.h`'s
  `do_decoding()` stores `in_file = (HANDLE)fp` and `mpegamp_w32.cpp`'s `CMpegTimeParser` opens/seeks/closes
  through `osfile` with the same cast. `CMakeLists.txt` now compiles `imgui/getbits.cpp` instead of
  `../win32/mpegamp/getbits.cpp`; the other six shared amp `.cpp`s are unchanged. `osfrbc` can't tell a
  read error from EOF, so `get_input()`'s former `GETHDR_ERR`-vs-`GETHDR_EOF` split collapses to
  `GETHDR_EOF` (both paths stop playback gracefully).

**Verified**: clean build + link of `guit3`, `0 warnings` on the five touched/forked TUs (`getbits.cpp`,
`tadscsnd.cpp`, `tadswav.cpp`, `tadsvorb.cpp`, `mpegamp_w32.cpp`) plus `guisnd.cpp` and `htmlgui.cpp`, and
a smoke-test launch of `ditch3.t3` (renders and runs normally — no audio regression in startup/shutdown
wiring). **The decode paths themselves are still unverified by ear** — nothing in `tests/` plays a sound,
exactly the gap §3.7 / this item's opening note calls out. Sourcing a sound-bearing `.t3`/`.gam` and
playing one WAV, one Ogg and one MP3 through the new code is the remaining confirmation.

**J. File-system browsing in the ImGui dialogs — done (see §5.5).** `tadsfiledlg.cpp` and
`tadsfolderdlg.cpp` used `FindFirstFileA`/`FindNextFileA`/`GetFileAttributesA`/`GetFullPathNameA`/
`GetCurrentDirectoryA`/`PathMatchSpecA`. Like H and I, this needed **no new `os_*` hook** — the TADS OS
layer already has a complete portable filesystem API (`os_open_dir()`/`os_read_dir()`/`os_close_dir()`,
`osfmode()`+`OSFMODE_DIR`, `os_get_abs_filename()`, `os_build_full_path()`, `os_get_path_name()`/
`os_get_root_name()`, `os_is_special_file()`), implemented for Windows in `../../tads-runner/tads2/osnoui.c`
+ `tads2/msdos/osdosnui.c` and for Unix in `tads3/unix/osunix.c`. Routing the two dialogs through it makes
them Win32-free **and** portable in one step (so A2 and M3 collapse for this item). `PathMatchSpecA` — which
has no osifc equivalent — is replaced by a small in-file `wildcard_match()`/`spec_match()` in
`tadsfiledlg.cpp` (`*`/`?`, `;`-separated alternatives, case-insensitive, `*.*`/`*` match everything). All
hard-coded `'\\'` path splitting/joining is gone (the "Up" button is now `os_build_full_path(cur, "..")`).
`tadsfolderdlg.cpp` is fully `<windows.h>`-free; `tadsfiledlg.cpp` keeps `<windows.h>` only for
`open_blocking()`'s `window == 0` native-`GetOpenFileName()` fallback (both call sites document the window
always exists — A1's "`.cpp` stays Windows-only until M4" convention). Related sites also cleaned up:
`htmlpref.cpp`'s `PathAppend` → `os_build_full_path` and its `GetCurrentDirectory` → `os_get_abs_filename(".")`,
`htmlgui.cpp:notify_load_game()`'s `GetFullPathName(..., &root_name)` → `os_get_abs_filename()` +
`os_get_root_name()`. With those, `Shlwapi.lib` is dropped from [CMakeLists.txt](CMakeLists.txt). Still on
raw Win32, deliberately: `htmlgui.cpp:load_new_game()`'s `SetCurrentDirectory()` — there is no portable
`os_*` chdir, so it needs a broader decision (a new hook), not this item.

**K. Character encoding.** `MultiByteToWideChar`/`WideCharToMultiByte` with `CP_ACP`, 10 sites in
`htmlgui.cpp` — `measure_text()`, `draw_text()`, `get_max_chars_in_width()` and the clipboard paths all
convert the engine's local-codepage bytes to UTF-16/UTF-8 for ImGui. The consistent answer is to route this
through the TADS charmap layer the VM already loads (`charmap/cmaplib.t3r`, shipped next to the exe — §2)
rather than adding a second, parallel encoding assumption. Note the existing code already assumes
"wide-char count == source character count" in `get_max_chars_in_width()` (true for single-byte codepages,
false in general) — decide explicitly whether to keep that assumption.

**A2 seam done — Win32 backend; the charmap-backed portable half is M3.** Three hooks in
[guios.h](guios.h) / [guios_w32.cpp](guios_w32.cpp), Windows backends lifted verbatim from the call sites:

- `os_local_to_utf8(codepage, src, srclen, &out_len)` — the
  `MultiByteToWideChar(MB_PRECOMPOSED)` + `WideCharToMultiByte(CP_UTF8)` pair, returning a `th_malloc()`'d
  NUL-terminated buffer plus its byte length. Routes `measure_text()`, `draw_text()` (`draw_text_clip()`)
  and — see below — `do_copy()`. `measure_text()`/`draw_text()` kept measuring/drawing against the
  *converted* buffer's length (`utf8size`), not the source's, so a character that expands under UTF-8
  (`©`, em-dash, …) still isn't truncated.
- `os_local_to_utf16(codepage, src, srclen, &out_cnt)` — the `MultiByteToWideChar` half alone, returning a
  `th_malloc()`'d array of `os_utf16_t` (`unsigned short` — `guios.h` stays `windows.h`-free and can't name
  `wchar_t`, whose width is platform-dependent). Routes `get_max_chars_in_width()`'s glyph-advance loop.
  **The "one array unit == one character" assumption the doc above flags is deliberately kept** — exact for
  the single-/double-byte code pages actually in play (no BMP-external characters, so no surrogate pairs),
  and the portable charmap backend can revisit it.
- `os_utf8_to_local(codepage, utf8, &out_len)` — the same pair run backwards (`MultiByteToWideChar(CP_UTF8)`
  + `WideCharToMultiByte(codepage)`), for `do_paste()`. Un-representable characters get the code page's
  default substitute, same lossy behavior the old `CF_TEXT` paste had.

**Clipboard unified onto GLFW at the same time** (this is what the §5.5 "M3/D-F" clipboard note deferred
"to item K"). `os_clipboard_set_text()` / `os_clipboard_get_text()` are now one GLFW implementation in
[guios_common.cpp](guios_common.cpp) (`glfwSet/GetClipboardString`, UTF-8 / `CF_UNICODETEXT` on Windows) —
the `OpenClipboard`/`GlobalAlloc`/`CF_TEXT` versions are gone from both `guios_w32.cpp` and
`guios_portable.cpp`. `do_copy()` converts local→UTF-8 (`os_local_to_utf8`) before setting; `do_paste()`
converts UTF-8→local (`os_utf8_to_local`) after getting, because `insert_text_from_hglobal()` is shared
with the OLE drag sink (item O) and stays local-codepage. Both use `CP_ACP` — the encoding the old
`CF_TEXT` path used implicitly; a per-window-charset refinement is possible later. This also *improves*
interop: game text now lands on the clipboard as real Unicode instead of raw local-codepage bytes.
`os_clipboard_has_text()` stays per-platform: `can_paste()` calls it every frame from
`render_toolbar()`, and only Win32's `IsClipboardFormatAvailable()` answers without opening the clipboard
— the portable backend has to do a full `glfwGetClipboardString` fetch. `tadsole.cpp`'s own `CF_TEXT` use
(the OLE `IDataObject`, item O) is untouched.

`guios_portable.cpp` documents the K conversion gap (same as item B). **Verified**: clean build + link of
`guit3`, `0 warnings`, and an interactive test on `ditch3.t3` — with `café-rt “x”` (é + curly quotes) on
the Windows clipboard, `Edit > Paste` at the game's `>` prompt inserts it intact (exercises
`os_clipboard_has_text` per frame → Paste enabled, then `glfwGetClipboardString` → `os_utf8_to_local` →
`insert_text_from_hglobal` → the K text-render hooks); game text (em-dash, `“budget economy class”`) and
`Copyright ©2004` render correctly throughout. `do_copy()` (needs a drag-selection) was not click-tested —
it is the symmetric `os_local_to_utf8` + `glfwSetClipboardString` path. **Update: click-tested (and fixed)
during §5.4/L's Ctrl+C verification — see "3.2b `get_focus_subwin()`" below. `do_copy()`'s own K-hook path
was fine; the bug was one layer up, in the dispatch that decides which subwindow to call it on.**

**L. `CTadsApp`, keyboard, and accelerators.**

- **Correction found during M1 (§5.5): this bullet's premise was wrong about the big one.**
  `CTadsApp::event_loop(int *flag)` is **not** dead - it's the live redirector `main()` (`guimain.cpp`) and a
  dozen other call sites call into, which itself just forwards to `win->event_loop(flag)`, the real
  `CHtmlSys_mainwin::event_loop()` that drives every ImGui frame (§2 and throughout §3). There is no
  `event_loop(MSG*)` overload in `CTadsApp` at all - whatever this referred to no longer exists, if it ever did.
  **Left untouched**, along with `process_message()` (reachable via `osnet_host_process_message()` in
  `guimain.cpp`, a real callback the network layer can invoke - not verified dead) and the `CTadsAccelerator`
  dispatch (its `translate()`/`accel_translate()` message-based path may be as inert as §3.1 says, but the same
  class's key-mapping storage and `enum_keys()` back the live Keyboard preferences page, so the class can't be
  cut wholesale without first separating those two roles). **Only `add_modeless()`/`remove_modeless()` were
  removed (done, M1)** - confirmed genuinely zero call sites anywhere, unlike everything else in this bullet;
  `modeless_dlgs_`/`modeless_dlg_alloc_`/`modeless_dlg_cnt_` and the now-permanently-empty scan loop in
  `process_message()` were left in place as harmless dead weight rather than risk touching `process_message()`
  itself. MDI (`set_mdi_win()`/`mdi_win_`) was left alone too - `tadswin.cpp` still calls `set_mdi_win()` from
  `CTadsSyswinMdiFrame`, which is blocked on §5.4/A1 same as the rest of MDI (§5.3), not this bullet.
  **Before attempting the rest of this item, re-verify reachability from scratch** the same way the M1 pass had
  to for `event_loop()`, `process_message()`, and the §5.3 "dead" files that turned out not to be (see the
  corrections throughout §5.3 and §3.3) - grepping for a function's own name only tells you it's *declared*
  somewhere, not that every caller in the chain is actually reachable, and this section's original claims were
  wrong more often than right. `get_instance()` (71 call sites!) disappears entirely once resources are portable
  (§5.4/B) — it exists only to be passed to `LoadString`/`LoadCursor`/`LoadMenu`/`LoadImage`/`GetModuleFileName`.
- `tadskb.cpp`'s `VK_*` name table + `MapVirtualKey` back the Keyboard preferences page's key-name
  parsing/formatting. Re-map onto `GLFW_KEY_*`. The table is a plain array; the work is the key mapping.
- **Do real keyboard accelerators at the same time** (§3.1: menu shortcuts are still display-only and
  `CTadsAccelerator` is dead). Both need one canonical key enum, so doing them together avoids defining it
  twice.

**A2 seam done, and real accelerators shipped with it — see the fuller "L. Keyboard and real
accelerators" entry under §5.5's "A2 in progress" for the full account, including a correction to this
bullet's "backs the live Keyboard preferences page" claim (it doesn't - `CTadsAccelerator` turned out to
be instantiated only by the out-of-scope debugger, never by guit3 itself).** In short: `os_key_t`
(`GLFW_KEY_*`, guios.h) is the canonical key enum; `tadskb.cpp`'s table and `MapVirtualKey`/`VkKeyScan`
calls now run through it and two new `os_*` hooks; and real shortcuts now fire via
`CHtmlSys_mainwin::do_accel_keys()` reading the existing `IDR_ACCEL_WIN`/`IDR_ACCEL_EMACS` resources in
portable form (`os_load_accel_table()`), not through `CTadsAccelerator`.

**M. `guimain.cpp` startup/shutdown.** `CoInitialize`/`CoUninitialize` (only needed for the Web UI and
`tadsole.cpp`'s drag-and-drop — goes with O; **correction, M1: it does not actually go with O**, see there),
`InitCommonControlsEx` (goes with the dead dialog files),
`LoadLibrary("RICHED32.DLL")` (**audit — nothing in guit3 appears to use a rich edit control any more**),
`GetModuleHandle`, `init_debug_console`/`close_debug_console`, and the `CreateFile`/`WriteFile` crash-dump
writer (`tadscrsh.txt`, ~line 292) → `osfile`/`stdio`.

**N. The windows that don't render at all.** `CHtmlSys_aboutgamewin` and `CHtmlSysWin_win32_Popup`
(`htmlgui.cpp`, 18k–19k line range) compile but produce nothing in guit3. (`CHtmlSys_abouttadswin` and
`CHtmlSys_creditswin`, the other two originally in this bucket, are now real floating ImGui windows — §3.3a/
§3.3b — and License, originally a `LicenseDlg : CTadsDialog` native modal driven from inside About, is now a
real ImGui popup — §3.3d.) The two remaining are `CTadsWin`/`CTadsWinScroll` subclasses displaying HTML, so
most of the ImGui path already exists — **`CHtmlSys_dbglogwin` is the working precedent** for a secondary
top-level window rendered as an ImGui overlay, including its own menu bar and the coordinate-space trap (§2).
Porting `CHtmlSys_aboutgamewin` retires the last live entry points in `tadsdlg.cpp` (`modal_dlg_pre`/`post`,
`set_filedlg_center_hook`), which is what lets §5.3 drop those files.

**Done, both, plus a startup crash found along the way. Correction: `modal_dlg_pre`/`post` are not actually
retired** - `CHtmlSys_abouttadswin::run_dlg()` still calls them too (for the real, still-functional
`enable_accel(FALSE)` side effect - accelerators need suppressing while a dialog owns input, independent of
the dead `GetTopWindow`/`EnableWindow` disable-loop in the same function), so `CHtmlSys_aboutgamewin::
run_aboutbox()` keeps calling them for the same reason; §5.3 dropping `tadsdlg.cpp` needs a separate look at
whether `enable_accel()` can be hoisted out first, not a repeat of this bullet's original claim.

Both classes had the same root problem as the rest of this section: real Win32 calls (`CreateWindow`,
`MoveWindow`, `GetWindowRect`, `EnableWindow`, `DestroyWindow`, `GetParent`) still aimed at `handle_`/a
subwindow's `handle_`, which stopped being a real HWND a long time ago (`CTadsWin::create_system_window()`) -
every one of them was a silent no-op. Fixed by switching to the portable equivalents already established
elsewhere (`do_move()`/`do_resize()` - via a new `CHtmlSysWin_win32::reposition()` public passthrough where
the caller is a sibling class and they're otherwise protected; `setVisible()`; `get_parent()`; `destroy_now()`)
and, for both windows, actually registering with `push_active_dialog()`/`pop_active_dialog()` so
`event_loop()` knows to render them and route mouse input to them at all - neither was ever wired into that
stack, so on top of the dead Win32 calls, nothing would have drawn them even once fixed.
`push_active_dialog()`/`active_dialogs_` had to be retyped from `CHtmlSys_top_win *` to the common `CTadsWin *`
base for this, since `CHtmlSys_aboutgamewin` is a direct `CTadsWin` subclass (its HTML content comes from the
running game via `create_html_subwin(formatter)`, not from `CHtmlSys_top_win`'s own
`parser_`/`formatter_`/`build_contents()`), not a `CHtmlSys_top_win`. The native "OK" button
(`CreateWindow("BUTTON", ...)` on the fake `handle_`, so it never existed) became a plain ImGui button drawn
in a new `do_render_content_end()` override. `os_show_popup_menu()` was also creating `CHtmlSys_popup_menu_win`
with the main window as parent instead of null, so it rendered (once the above was fixed) as a title-barred,
draggable dialog nested in the main window's own content instead of a borderless top-level overlay - fixed by
passing a null parent and adding `CTadsWin::get_floating_window_flags()` (default no-op, overridden here to
`ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove`) so a floating window can opt out of the normal
decorated look. Not independently screenshot-verified end to end - no available test game triggers
`os_show_popup_menu()` (no caller of it was found anywhere in the tree either, core VM included; it may be
effectively dead in practice), and `ditch3.t3` doesn't define an "About This Game" resource, so
`ID_HELP_ABOUT_GAME` stayed disabled in manual testing. Verified instead by confirming the already-proven
`CHtmlSys_abouttadswin` floating-dialog path (same `push_active_dialog()`/`do_render_content_begin()`
machinery) still renders and dismisses correctly after the `active_dialogs_` retype, by inspection that the
popup/about-game code now follows that exact same proven pattern, and by a clean 0-warning build.

**Startup-crash correction:** the first version of this fix made `CHtmlSys_aboutgamewin`'s *initial* (still
hidden) `create_system_window(0, ...)` call directly from `CHtmlSys_mainwin::do_create()`, matching where it
already lived - but `do_create()` runs *from inside* the main window's own `create_system_window()` call,
before that call has gotten around to actually creating the real GLFW window/OpenGL context (that happens
afterward, in the same function, in the `parent == nullptr` branch). A parentless child's own
`create_system_window()` tries to create *its own* GLFW window too, and
`CTadsSyswin::syswin_create_system_window()`'s "only one real window" guard
(`glfwGetCurrentContext() != nullptr`) only refuses when a context already exists - at this exact moment none
does yet, so it raced a second, premature GLFW/GL init against the main window's own and crashed on every
launch (`0xc0000005`, reproducible, same offset every time - caught via `Get-WinEvent` against the
Application Error log, not any on-screen message). Fixed by moving the whole creation call out of
`do_create()` into a new `CHtmlSys_mainwin::create_aboutbox_win()`, called from `guimain.cpp` right after the
main window's own `create_system_window()` returns - the same spot and reasoning `CHtmlSys_dbglogwin`'s
creation already used one function down. Lesson for anything else in this bucket: a window being parentless
is only safe once *some* real window already exists; check the call site's place in the startup sequence, not
just whether the window itself looks like existing proven ones.

**O. Gate the Web UI.** `tadswebctl.*`, `guiwebui.h`, `tadscom.*`, `guinogch.cpp` and the `CoInitialize`
pair behind `TADS_WEBUI_ENABLED`, off by default (§4). `CTadsStatusline::get_handle()` exists solely to keep
`guiwebui.h` compiling (§3.2) and can go with it.

**Done (M1), except the `CoInitialize` pair - it can't be gated the way this bullet assumed.**
`tadswebctl.cpp`/`tadscom.cpp` are now built only `if (TADS_WEBUI_ENABLED)` (a new `CMakeLists.txt` option,
default `OFF`); `guinogch.cpp`'s no-op Game Chest stubs build in the `else()` branch, matching current (Game
Chest absent) behavior, so the default build is unchanged. `guiwebui.h`'s `#include` in `guimain.cpp` is now
`#ifdef TADS_WEBUI_ENABLED`-gated (nothing else in the live tree referenced its one class,
`CHtmlSys_webuiwin` - it's declared there but never defined/instantiated in `guit3`, only in
`htmltads/win32/w32webui.cpp`, which isn't compiled here). **`CoInitialize`/`CoUninitialize` in `guimain.cpp`
were left unconditional**, contradicting this bullet and the M-item comment above: `tadswin.cpp` calls the real
Win32 `RegisterDragDrop()`/`DoDragDrop()` for OLE drag-and-drop, unconditionally, independent of the Web UI
flag - gating COM init off by default would have silently broken that live functionality. `guinogch.cpp` was
left un-gated too, contrary to a literal reading of this bullet: it's the "Game Chest absent" implementation,
which is exactly what stays needed while `TADS_WEBUI_ENABLED` defaults off (there's no portable "Game Chest
present" implementation in `guit3` yet for the `ON` branch to build instead). `CTadsStatusline::get_handle()`
was left alone - out of caution, not because it was rechecked.

### 5.5 Suggested order

**M1 — shrink the surface (no new platform code, Windows build unchanged).** Drop the dead files (§5.3,
except the ones blocked on N); gate the Web UI (O); drop the DirectSound probe (I) and `HtmlHelp` (F); trim
`CTadsApp` (L, the dead half). *Fewer files, fewer link libs, smaller type shim.*

**Done. Originally landed with two deviations and one incomplete item; the M1 revisit (after the Manage
Themes port, §3.3, left zero native dialogs) closed both deviations.** Summary: `guifndlg.cpp`/`iconmenu.cpp`
dropped as planned (iconmenu.cpp needed a few more call sites removed than expected); `foldsel2.cpp` dropped
too, but only after also removing ~1750 lines of dead property-page classes and `run_preferences_dlg()` from
`htmlpref.cpp` that turned out to be its only remaining consumer.

- **`tadscbtn.cpp` — first pass: not dropped** (reachable through the then-still-native
  `Manage Profiles > Customize` chain, §3.3 correction). **M1 revisit: dropped.** With `ID_MANAGE_PROFILES`
  now on the ImGui `open_manage_themes_dialog()`, the whole `run_profiles_dlg()` → `CHtmlDialogAppearance` →
  `run_appearance_dlg()` → `CHtmlDialog{Fonts,Color,More,Media}` → `CColorBtnPropPage` chain is dead;
  ~1600 lines removed from `htmlpref.cpp`, `tadscbtn.cpp`/`.h` deleted, source-list and `#include` entries
  removed.
- **`HtmlHelp` (F) — first pass: kept**, wrapped in `#ifdef _WIN32`, per explicit direction mid-task.
  **M1 revisit: dropped as originally planned**, on explicit direction. `ID_HELP_CONTENTS` is now a plain
  `ShellExecute` to the online docs; `<htmlhelp.h>` and `Htmlhelp.lib` are gone.
- The DirectSound probe (I) is done as planned.
- The Web UI gate (O) is done, except `CoInitialize`/`CoUninitialize` had to stay unconditional (real OLE
  drag-and-drop in `tadswin.cpp` needs COM initialized regardless of the Web UI flag).
- `CTadsApp` trimming (L) turned out to be based on an incorrect premise - `event_loop()` is very much
  alive - so only the confirmed-dead `add_modeless()`/`remove_modeless()` were removed; the rest of that
  item needs a fresh reachability audit before it's attempted, not a repeat of this plan's original claims.

The original pass was verified with a full clean Windows build and a manual smoke-test launch (game loaded,
rendered, ran normally). The M1 revisit is verified with a clean Debug build of `guit3`; a smoke-test launch
of the Themes menu could not be run in this environment (synthetic-input automation is blocked by local
antivirus - see §6).

**M2 — build the seam.** `tadsplat.h` (A1) and the `os_*` hook headers with Windows-only backends (A2).
*Still Windows-only, but every remaining Win32 call sits behind a named, single-purpose hook.*

**A1 done.** New [tadsplat.h](tadsplat.h): on `_WIN32` it is a bare `#include <windows.h>` forwarder, so the
Windows translation is byte-identical; the `#else` branch is the ~360-line type shim §5.4/A1 describes
(opaque `void *` handles, the plain structs `RECT`/`POINT`/`SIZE`/`SCROLLINFO`/`LOGFONT`/`MSG`/`NMHDR`/…,
the scalar typedefs, the `WM_*`/`WS_*`/`SB_*`/`SW_*`/`SIF_*`/`MB_*`/`IDC_*`/`COLOR_*`/`GWL_*`/`CS_*`
constants that header-inline code still names, and the `RGB()`/`MAKEINTRESOURCE()`/`LOWORD()` macros).
The `#else` branch is **deliberately incomplete** — audio/MIDI (`WAVEFORMATEX`, `MIDIHDR`, …) and COM/OLE
(`IDataObject`, `REFIID`, …) are stubbed only far enough to parse, since those subsystems stay Windows-only
past M2 (items I, O, phase two); M4's first off-Windows compile is what closes the remaining gaps against
real compiler output. The 18 guit3 headers that did `#include <Windows.h>` now do `#include "tadsplat.h"`
instead (`tadswin.h`, `htmlgui.h`, `htmlpref.h`, `tadsapp.h`, `tadsfont.h`, `tadsimg.h`, `tadsstat.h`,
`tadscar.h`, `tadsistr.h`, `tadstab.h`, `tadsreg.h`, `tadsole.h`, `tadscsnd.h`, `tadswav.h`, `tadsmidi.h`,
`tadscom.h`, `tadsdlg.h`, `tadswebctl.h`); the sibling `<Ole2.h>`/`<commctrl.h>`/`<Shlobj.h>` includes are
left as-is (they back Windows-only OLE/common-control code, item O). The `.cpp` files still
`#include <windows.h>` directly — they compile Windows-only until M4 and there is no M2 benefit to touching
them. Dead MDI was stripped at the same time (§5.3): the `CTadsSyswinMdiFrame`/`Client`/`Child` classes and
their `register_win_class()` registration block are gone from `tadswin.h`/`tadswin.cpp`, and
`CTadsApp::set_mdi_win()`/`mdi_win_` plus the `TranslateMDISysAccel()` branch in
`CTadsApp::process_message()` are gone from `tadsapp.h`/`tadsapp.cpp`. The base-class
`mdi{child,frame}_message_handler()` statics and the `mdi*_win_class_name` string constants on `CTadsWin`
were left in place as harmless unreferenced dead native code — removing the rest of the base-class MDI
plumbing is out of A1's scope.

**Verified** by recompiling every guit3-local translation unit (`htmlgui.cpp`, `tadswin.cpp`, `tadsapp.cpp`,
`htmlpref.cpp`, `guimain.cpp`, and ~40 more) with MSVC at `/W1 /WX-`: **0 warnings, 0 errors**. The final
link and a runtime smoke test could **not** be run in this environment — Device Guard policy blocks the
freshly-built `t3res.exe` that the `guit3.t3r` custom build step invokes (`exit code 1073751882`), and
blocks running the produced `.exe` (same class of restriction §6 records for synthetic input). A1 changes
no logic and, on Windows, `tadsplat.h` expands to exactly `<windows.h>`, so a clean compile of the touched
units is strong evidence the change is inert on Windows; a full build + smoke test on an unrestricted
machine is still the last confirmation.

**A2 done for all of B–M** (see the "A2 done for all of B–M" note further down, after item M). The `os_*` call hooks per subsystem, each following the [tadsfont.h](tadsfont.h) /
[guifont.cpp](guifont.cpp) precedent: a neutral hook declaration + a Windows backend lifted verbatim from
the current call site, one landable commit per subsystem, Windows build kept byte-identical. New pair
[guios.h](guios.h) (neutral, `windows.h`-free) / [guios_w32.cpp](guios_w32.cpp) (Win32 backend), added to
`CMakeLists.txt` — a second backend file per platform gets selected there later.

- **F. Shell** — *done.* `os_open_url()` replaces the five inline `ShellExecute(0, "open"/0, url, …)` calls
  (`process_command()`; the two Help > TADS-on-the-Web menu items; `guitr.cpp`'s check-for-updates prompt;
  the about-game `http:` link). The `<= 32` failure test moved into the backend and is surfaced as a plain
  nonzero-on-success return, so the two sites that pop an "unable to open link" box still do.
- **E. System colors** — *done.* `os_get_sys_color(os_sys_color_t)` (enum: `HIGHLIGHT`, `HIGHLIGHT_TEXT`,
  `WINDOW`, `WINDOW_TEXT`), Win32 backend forwards to `GetSysColor()`. Routes the eight live *value* reads:
  the text-selection highlight fg/bg used for ImGui drawing, and the window fg/bg resolved when "Use Windows
  colors" is on (`set_html_bg_color`, `set_html_text_color`, `note_debug_format_changes`, `map_color`). The
  `GetSysColorBrush()` / `SetTextColor(dc, …)` / `FillRect(dc, …)` sites in the MORE-prompt and banner-border
  owner-draw paths are **left as-is** — GDI-brush code with no real DC behind it in guit3, part of the
  separate native-dead-code sweep, not a neutral color hook.
- **D. Services GLFW provides** — *done (hooks + Win32 backend; portable impls are M3).*
  - `os_get_tick_ms()` (Win32 backend: `GetTickCount()` at the time; replaced with a shared
    `std::chrono::steady_clock` implementation in M3/D-F, see below) replaces every `GetTickCount()` call in
    `htmlgui.cpp` and `tadswin.cpp`; `hos_gui.cpp`'s `os_get_time()` now forwards to it too.
  - Clipboard: `os_clipboard_set_text()` / `os_clipboard_has_text()` / `os_clipboard_get_text()` (the last
    returns a `th_malloc()`'d copy). `do_copy()` now builds the CR/LF-expanded text via
    `copy_to_new_hglobal(GMEM_FIXED, …)` and hands the plain buffer to the hook; `can_paste()` is one
    `os_clipboard_has_text()` call; `do_paste()` gets a copy from the hook, feeds
    `insert_text_from_hglobal()`, and `th_free()`s it. `copy_to_new_hglobal()` /
    `insert_text_from_hglobal()` are untouched — still shared with the OLE drag path (item O). The `has_text`
    backend uses `IsClipboardFormatAvailable(CF_TEXT)` in place of the old `OpenClipboard` +
    `EnumClipboardFormats` loop (equivalent, and no clipboard-open needed). *(Updated by item K: `set`/`get`
    moved to the shared GLFW path in `guios_common.cpp` and the transfer format became UTF-8, with
    `do_copy()`/`do_paste()` converting through `os_local_to_utf8()`/`os_utf8_to_local()`; `has_text` and the
    `copy_to_new_hglobal`/`insert_text_from_hglobal` framing are as described. §5.4/K.)*
  - Cursors: the four `HCURSOR` members (`ibeam_csr_`/`hand_csr_` on `CHtmlSysWin_win32`,
    `arrow_cursor_`/`wait_cursor_` on `CTadsWin`), their `LoadCursor()` init and their `DestroyCursor()`
    teardown are **gone**. The hover-shape cursors (arrow / I-beam / hand, set from `do_setcursor()` /
    `set_disp_item_cursor()` inside the frame loop) are now plain `ImGui::SetMouseCursor(ImGuiMouseCursor_…)`
    calls — no OS hook, since ImGui already pushes the shape to the platform every `NewFrame`. The one cursor
    ImGui **can't** do is the busy cursor around long synchronous work (`find_text()`, `do_formatting()`'s
    format loop, `maybe_prune_parse_tree()`): those block the render loop, so an ImGui request would never be
    applied.
    That case keeps an OS hook — `os_set_wait_cursor(void)` / `os_restore_cursor(os_cursor_token_t)` in
    [guios.h](guios.h) / [guios_w32.cpp](guios_w32.cpp), the Win32 backend `SetCursor(LoadCursor(NULL,
    IDC_WAIT))` with the opaque save/restore token. The old `os_set_mouse_cursor(os_mouse_cursor_t)` enum
    API and its `cursor_for()` shape table (including the custom `"HAND_CURSOR"` resource lookup — ImGui's
    hand cursor supersedes it) are gone. The lone `wc.hCursor = LoadCursor(...)` in `tadswin.cpp`'s dead
    `register_win_class()` is left alone (WNDCLASS plumbing, not cursor-setting).

  **Verified**: clean build **and link** of `guit3` (Device Guard no longer blocks the toolchain here), plus
  a smoke-test launch on `ditch3.t3` — game renders, menu/toolbar/status bar intact, the status-bar elapsed
  clock advances (exercises `os_get_tick_ms()`), and the Edit menu shows correct Copy/Paste enable state
  (exercises `can_copy()` / `os_clipboard_has_text()`). Interactive clipboard round-trip and cursor-shape
  checks were **not** run — `Add-Type`-based synthetic-input scripting is blocked in this environment (§6) —
  but those paths are verbatim extractions of the pre-existing Win32 code.

  **M3/D-F — portable backend landed.** The `guios.h` backend is now three files: `guios_common.cpp`
  (compiled everywhere, for hooks that are the same C++ on every platform) plus one per-platform backend —
  `CMakeLists.txt` picks it with `if (WIN32) guios_w32.cpp else() guios_portable.cpp` (new
  [guios_portable.cpp](guios_portable.cpp), the non-Windows counterpart of `guios_w32.cpp`). Implementations:

  - **D tick clock** → `std::chrono::steady_clock`, in **`guios_common.cpp`** — used on Windows too. It is a
    strict upgrade over `GetTickCount()` (MSVC backs `steady_clock` with `QueryPerformanceCounter`; callers
    only ever diff two readings). Measured from the first call; as an `unsigned long` it wraps after ~49 days
    of process uptime where `long` is 32-bit and effectively never where it is 64-bit.
  - **D clipboard** → `glfwGet/SetClipboardString(NULL, …)` (the window arg is deprecated-and-ignored since
    GLFW 3.0), CR/LF normalization left to the caller as before. *(Superseded by item K: `set`/`get` were
    unified onto this one GLFW path in `guios_common.cpp` — Windows included — when the charset hooks landed;
    the callers now convert local↔UTF-8 with `os_local_to_utf8()`/`os_utf8_to_local()`. Only
    `os_clipboard_has_text()` stays per-platform, since it runs every frame from the toolbar and GLFW has no
    probe short of a full fetch. See §5.4/K.)*
  - **D wait cursor** → **no-op** on the portable side (GLFW 3.5 has no busy/hourglass standard cursor, and
    these ops block the frame loop; the "Working…" status-line message still shows, so the cue isn't lost — a
    real busy cursor needs a bundled image via `glfwCreateCursor()`, deferred with B's other embedded
    assets). `guios_w32.cpp` keeps the real `SetCursor(LoadCursor(IDC_WAIT))`.
  - **E system colors** → fixed light-UI values in the same packed `0x00BBGGRR` encoding. Portable only;
    `guios_w32.cpp` keeps the real `GetSysColor()` read of the user's theme.
  - **F shell** → `fork()` + `execlp("xdg-open" / "open", url)` (`__APPLE__` picks `open`), reaped with
    `waitpid`, nonzero on a clean child exit. Portable only; `guios_w32.cpp` keeps `ShellExecute`.

  **Not** in `guios_portable.cpp` yet: item B's `os_load_string()` / `os_load_toolbar_rgba()` /
  `os_load_license_text()` — so a non-Windows link is still incomplete, which is moot until the
  `if (NOT WIN32) return()` gate lifts in M4. Windows build re-verified: `guit3` links (`guios_common.cpp` +
  `guios_w32.cpp` compiled, `guios_portable.cpp` excluded) and launches `ditch3.t3` normally, status-bar
  elapsed clock advancing (exercises the shared `os_get_tick_ms()`). The portable-only paths can't run until
  the M4 Linux build.

- **B. Resources** — *done, including the M3 portable backend (string table + embedded byte arrays,
  [guires_data.h](guires_data.h)/[guires_data.cpp](guires_data.cpp) — see §5.5's M3 write-up).*
  Three hooks in [guios.h](guios.h) / [guios_w32.cpp](guios_w32.cpp), backends lifted verbatim:
  - `os_load_string(id, buf, buflen)` — same `LoadString()` contract. Routed at every **live** call site:
    `CHtmlSysWin_win32::load_res_str()` and ~15 more in `htmlgui.cpp` (About-box HTML, Find "no more",
    "unable to open link", new-game / quit / go-to-Game-Chest confirms, the ImGui Themes menu items and
    toolbar tooltip, the file-dialog prompts, the hidden about-game window title), the theme-description
    lookup in `htmlpref.cpp`, and the game-name prompt in `guimain.cpp` (`+#include "guios.h"` added to the
    latter two). The ~6 `LoadString()` calls left direct in `htmlgui.cpp` are inside dead native-`HMENU`
    builders (`InsertMenuItem`/`MENUITEMINFO`), and `tadswin.cpp`'s native menu-label updater,
    `tadsdlg2.cpp`, and `guimain.cpp`'s `check_common_ctl_vsn()` COMCTL32 guard are Windows-only dead/startup
    code — all keep `<windows.h>` per A1.
  - `os_load_toolbar_rgba(&w, &h)` — the `LoadImage(LR_CREATEDIBSECTION)` + `GetDIBits()` + BGRA→RGBA +
    color-key→alpha block out of `CHtmlSys_mainwin::load_toolbar_texture()`; that function keeps only
    `glGenTextures`…`glTexImage2D`. Buffer is now `th_malloc()`'d / `th_free()`'d (was `new[]`/`delete[]`).
  - `os_load_license_text(&len)` — the `FindResource`/`LoadResource`/`SizeofResource` lookup out of
    `tadslicensedlg.cpp`, which is now `<windows.h>`-free.

  **Verified**: clean build + link of `guit3`, `0 warnings`, and a fresh smoke-test launch on `ditch3.t3` —
  the window renders the game intro, the **toolbar icons all draw with correct color-key transparency**
  (exercises `os_load_toolbar_rgba()`), menu-bar and menu labels are populated (exercises `os_load_string()`
  on the startup path and per-frame), no crash. The License dialog (`os_load_license_text()`) was not
  click-tested this pass but its backend is a byte-for-byte extraction.

- **C. Settings storage** — *done, including the M3 file-backed backend
  ([tadssettings_portable.cpp](tadssettings_portable.cpp) — see §5.5's M3 write-up).* New pair
  [tadssettings.h](tadssettings.h) (neutral, `windows.h`-free) / [tadssettings_w32.cpp](tadssettings_w32.cpp)
  (registry backend), added to `CMakeLists.txt`; the local fork of `tadsreg.cpp` / `tadsreg.h` is **deleted**
  (the `../win32` copy that the native builds use is untouched). `CTadsSettings` has the same
  open / query_key_* / set_key_* / value_exists / delete shape `CTadsRegistry` had — every value read/write
  helper is that code lifted verbatim — with two differences the neutral interface forces:
  - The base-key argument (always `HKEY_CURRENT_USER` at the call sites) and the never-read create-disposition
    out-param are gone; `open_key(path, create)` takes just the backslash-delimited path a file backend can
    treat as a relative path. The opaque `tads_settings_key_t` handle is really an `HKEY` on Windows.
  - **The enumeration primitives are first-class**, per the §5.4/C note that a flat key/value file doesn't
    give subkey listing for free: `enum_subkeys()` is the `RegEnumKeyEx` loop that
    `CHtmlPreferences::opt_refresh_profile_list()` and `CHtmlSys_mainwin::render_themes_menu_items()` each had
    inline (the theme/profile list is "the child keys of `Settings\Profiles`"); `enum_str_values()` is the
    `RegEnumValue` loop from `rename_profile_refs()`, keeping its "skip, don't stop at, a non-string value"
    behavior via a three-way `TADS_SETTINGS_ENUM_OK/END/SKIP` return.

  Live call sites routed through it: `htmlpref.cpp` — `opt_refresh_profile_list()`, the Options dialog's
  "Delete Theme" button (`CTadsSettings::delete_key()`, a shallow delete matching the old bare `RegDeleteKey`,
  *not* `CTadsRegistry::delete_key`'s recursive form — a profile key holds only values), `save_as()` /
  `restore_as()` / `equals_saved()` and their `write_to_registry()` / `read_from_registry()` /
  `equals_registry_value()` helpers (signature `HKEY` → `tads_settings_key_t`), `profile_exists()`;
  `htmlgui.cpp` — `render_themes_menu_items()`, `get_profile_assoc()` / `set_profile_assoc()`,
  `rename_profile_refs()` (dead — no caller since the native New/Delete-profile dialog went in M1 — but ported
  rather than left calling the deleted class). The one place left on raw Win32 is
  `load_menu_with_profiles()` — a dead native-`HMENU` builder (`InsertMenuItem`/`MENUITEMINFO`), reached only
  from the dead `WM_INITMENUPOPUP` / `TBN_DROPDOWN` handlers — so its two `CTadsRegistry` calls became bare
  `RegCreateKeyEx`/`RegCloseKey`, consistent with the B-item convention for dead native-menu code.
  A stale `#include "tadsreg.h"` in `guimain.cpp` (nothing from it was used) was dropped.

  **Verified**: clean build + link of `guit3`, `0 warnings` on the four touched TUs, and a fresh smoke-test
  launch on `ditch3.t3` — preferences load through `restore_as()` → `CTadsSettings` (fonts/colors are applied,
  no crash) and the window renders the game intro with menu bar (incl. the profile-backed Themes menu),
  toolbar and status bar intact. The Themes menu's live `enum_subkeys()` path was not click-tested this pass
  (foreground-focus contention with the IDE), but it is the same `RegEnumKeyEx` loop moved unchanged.

- **G. Fonts** — *done, including the M3 [fcfont.cpp](fcfont.cpp)/[ctfont.cpp](ctfont.cpp) backends and the
  `guifont.cpp`/`guifont_w32.cpp` split that made room for them (see §5.5's M3 write-up).* Two changes, in
  [tadsfont.h](tadsfont.h) / [tadsfont.cpp](tadsfont.cpp) /
  [guifont.cpp](guifont.cpp) / [htmlgui.cpp](htmlgui.cpp):
  - `os_font_data_for_name(name, weight, italic, charset, &size)` — the `CreateFontIndirect()` +
    `GetFontData()` block that turned a logical font into TrueType/OpenType file bytes for FreeType, moved
    verbatim out of `CTadsFont`'s constructor into `guifont.cpp` beside `os_font_family_is_present()`. The
    constructor still creates `handle_` (`measure_text()`'s GDI metrics path needs it) and now calls the
    hook for the FreeType bytes. Buffer is `ImGui::MemAlloc()`'d and handed to `AddFontFromMemoryTTF()`
    (atlas takes ownership), same as before. The `GDI_ERROR`/"System" pseudo-font null-outline check moved
    into the hook; the constructor just leaves `m_font` null and everything falls back to the atlas default.
  - `get_max_chars_in_width()` no longer opens a throwaway `HDC` — the `GetDC`/`select_font`/`ReleaseDC`
    dance only pushed the ImGui font. New DC-free `CTadsFont::push_imgui_font()` (the ImGui half of
    `select()`, which now calls it then `SelectObject`s) does just that. `measure_text()` is left as-is —
    it still genuinely uses GDI (`GetTextMetrics`/`GetTextExtentPoint32`).

  **Verified**: clean build + link, `0 warnings` on the three touched TUs, fresh smoke-test launch on
  `ditch3.t3` — game intro renders with correct proportional-font metrics, bold title, hyperlink layout and
  line breaking (exercises `os_font_data_for_name()` on every `CHtmlSysFont_win32` and
  `get_max_chars_in_width()` in the formatter's line-break path), menu/toolbar/status bar intact, no
  character-map warning, no crash.

- **J. File dialogs** — *done (A2 + M3 in one; no `guios` hook).* `tadsfiledlg.cpp` / `tadsfolderdlg.cpp`
  now scan and resolve paths through the portable `osifc` API already provided by the TADS runner
  (`os_open_dir()`/`os_read_dir()`/`os_close_dir()`, `osfmode()`+`OSFMODE_DIR`, `os_get_abs_filename()`,
  `os_build_full_path()`, `os_get_path_name()`/`os_get_root_name()`, `os_is_special_file()` to drop `.` but
  keep `..`). `PathMatchSpecA` → an in-file `wildcard_match()`/`spec_match()` (`*`/`?`, `;`-alternatives,
  case-insensitive, `*.*` matches all). Every hard-coded `'\\'` split/join removed; the "Up" button is
  `os_build_full_path(cur_dir, "..")`. `tadsfolderdlg.cpp` is now fully `<windows.h>`-free;
  `tadsfiledlg.cpp` keeps it only for `open_blocking()`'s `window == 0` native fallback. Swept up with it:
  `htmlpref.cpp` `PathAppend`→`os_build_full_path`, `GetCurrentDirectory`→`os_get_abs_filename(".")`;
  `htmlgui.cpp` `notify_load_game()` `GetFullPathName(&root_name)`→`os_get_abs_filename()`+
  `os_get_root_name()`; `Shlwapi.lib` removed from [CMakeLists.txt](CMakeLists.txt).
  `load_new_game()`'s `SetCurrentDirectory()` stays on Win32 (no portable `os_*` chdir).

  **Verified**: clean build + link, `0 warnings` on the four touched TUs, fresh smoke-test launch — from a
  no-game start, `File > Open New Game...` shows the ImGui file dialog with the cwd resolved via
  `os_get_abs_filename(".")`, only `ditch3.t3` listed under the `*.t3` "T3 Applications" filter (glob
  matcher), `[Dir] ..` shown; double-clicking `..` navigates to `C:\Projects\tads-runner` and lists its
  subdirectories sorted; descending back into `tests` and double-clicking `ditch3.t3` loads and renders the
  game. `Edit > Options > Starting > Browse...` opens the folder picker (directories only) nested in the
  Options modal, path resolved the same way. No crash in either.

- **K. Character encoding** — *done, including the M3 charmap-backed portable half in
  [guios_portable.cpp](guios_portable.cpp) (see §5.5's M3 write-up).*
  `os_local_to_utf8()` / `os_local_to_utf16()` / `os_utf8_to_local()` in [guios.h](guios.h) /
  [guios_w32.cpp](guios_w32.cpp), the `MultiByteToWideChar`/`WideCharToMultiByte` pairs lifted verbatim out
  of `htmlgui.cpp`'s `measure_text()`, `draw_text()` (`draw_text_clip()`) and `get_max_chars_in_width()`,
  plus the reverse for `do_paste()`. `get_max_chars_in_width()`'s "one wide unit == one character"
  assumption is deliberately kept (exact for the single-/double-byte code pages in play).
  **The clipboard was unified onto GLFW at the same time**: `os_clipboard_set_text()` /
  `os_clipboard_get_text()` are now one `glfwSet/GetClipboardString` implementation in
  [guios_common.cpp](guios_common.cpp) (UTF-8 / `CF_UNICODETEXT` on Windows), the `CF_TEXT`
  `OpenClipboard`/`GlobalAlloc` versions deleted from both per-platform backends; `do_copy()`/`do_paste()`
  convert local↔UTF-8 through the K hooks. Only `os_clipboard_has_text()` stays per-platform (per-frame
  toolbar call; Win32 keeps `IsClipboardFormatAvailable`). **Verified**: clean build + link, `0 warnings`;
  interactive `ditch3.t3` test — `café-rt “x”` on the clipboard pastes intact at the `>` prompt via
  `Edit > Paste`; game text + `Copyright ©2004` render correctly. See §5.4/K.

- **L. Keyboard and real accelerators** — *done (A2 seam; canonical enum needs no separate M3 backend,
  see below).* `os_key_t` (a plain `GLFW_KEY_*` value) in [guios.h](guios.h) is the "one canonical key
  enum" this bullet called for; `os_key_to_char()`/`os_char_to_key()` (Win32 backend: `MapVirtualKey`/
  `VkKeyScan`, moved out of `tadskb.cpp` unchanged) are the only two pieces that still need an OS query,
  since GLFW doesn't expose the live keyboard layout. `CTadsKeyboard` (`tadskb.cpp`/`.h`) now stores and
  looks up keys by this canonical code throughout — its `VK_xxx` table became a `GLFW_KEY_*` table
  (`VK_SELECT`/`VK_PRINT`/`VK_EXECUTE`/`VK_HELP` dropped, no GLFW equivalent), and `shiftmap[256]` grew to
  `[512]` to cover `GLFW_KEY_LAST` (348). Letters and digits needed no conversion at all: `VK_A..Z`/
  `VK_0..9`, `GLFW_KEY_A..Z`/`GLFW_KEY_0..9`, and ASCII are all numerically identical, so
  `tadskb.cpp`'s existing `isalpha`/`isdigit` fallback in `parse_key_name()` carried over unchanged.

  **Correction, found while scoping this bullet: its own premise about "the live Keyboard preferences
  page" was wrong.** `CTadsAccelerator`/`KeyMapTable` (`tadsapp.h`/`.cpp`) is **not** instantiated anywhere
  in guit3 — the only `new CTadsAccelerator()` in the whole tree is `win32/w32tdb.cpp` (the Workbench
  debugger), which isn't compiled into `guit3` at all. `htmlpref.cpp`'s already-ported `opt_render_keys_tab()`
  ("Keyboard" preferences tab) is three unrelated boolean toggles (`emacs_ctrl_v`/`emacs_alt_v`/
  `arrow_scroll`) that never touch this class. So there was no live customizable key mapping to preserve;
  `CTadsAccelerator` stays exactly as inert in guit3 as §3.1 already said it was. It was still widened to
  match (`KeyMapTable::row[256]` → `row[KEYMAP_ROW_COUNT]`, `KEYMAP_ROW_COUNT` = 512, `delete_subtab()`'s
  hardcoded `256` loop bound updated to match) since `map()`/`enum_keys()` now write/read canonical key
  codes via `CTadsApp::kb_` like everything else — a `KeyMapTable` sized for 0-255 would have been an
  out-of-bounds write the first time a real caller mapped a key above `GLFW_KEY_BACKSPACE`(259). Left
  genuinely untouched: `translate()`/`msg_to_command()`, which still index the table with the raw VK code
  out of a `WM_KEYDOWN`'s `wParam` — unreachable without a real Win32 message loop, so this is a documented
  landmine (see the comment on the class in `tadsapp.h`) rather than a live bug, but it means the class
  can't be handed to a future debugger port as-is; `translate()`'s side would need converting too.

  **Real keyboard accelerators**, the bullet's other half, turned out not to need `CTadsAccelerator` at
  all: `win32/htmlcmn.rc`'s `IDR_ACCEL_WIN`/`IDR_ACCEL_EMACS` `ACCELERATORS` resources already map real
  keys straight to `do_command()`'s command IDs (`Ctrl+O` → `ID_FILE_LOADGAME`, `Alt+.` → `ID_GO_NEXT`,
  ...) and were already being loaded into `HACCEL`s and switched between by preference
  (`CHtmlSysWin_win32_Input::set_current_accel()`) — just never dispatched, since nothing pumps
  `TranslateAccelerator()` without a Win32 message loop. New `os_load_accel_table()` (guios.h/
  guios_w32.cpp) reads the same two resources via `CopyAcceleratorTable()` into a portable
  `os_accel_entry_t[]` (handling both its `VIRTKEY` rows and the four character-mode ALT+`.`/`,`/`>`/`<`
  rows, the latter via `os_char_to_key()`). `CHtmlSysWin_win32_Input` now loads and switches a
  parallel portable table alongside the existing `HACCEL`s (additive — the `HACCEL` fields, `CTadsApp::
  set_accel(HACCEL, …)`, and `accel_translate()`/`process_message()` are untouched, since the latter is
  "not verified dead" per this bullet's own earlier note and there was no reason to risk it).
  `CHtmlSys_mainwin::do_accel_keys()` (`htmlgui.cpp`), called once per frame from `event_loop()` right
  after the existing character-forwarding block, does the actual dispatch: per-key "was it down last
  frame" edge detection against `glfwGetKey()` (ImGui's `IsKeyPressed()` needs an `ImGuiKey`, and there's
  no public GLFW-key-to-`ImGuiKey` conversion to drive it off that instead), gated on `!io.WantTextInput`
  (same gate the char-forwarding block uses) and `!ImGui::IsPopupOpen(0, ImGuiPopupFlags_AnyPopup)` (no
  non-text-widget dialog, e.g. Options, was blocking `io.WantTextInput` on its own), then
  `check_command()`/`do_command()` — the exact pair `render_menu_bar()`'s `item()` lambda uses — so a
  shortcut and its menu item always agree on whether the command is enabled.

  **Verified**: clean build + link, `0 warnings` on all four touched TUs (`guios_w32.cpp`, `htmlgui.cpp`,
  `tadsapp.cpp`, `tadskb.cpp`); interactive `ditch3.t3` smoke test — `Ctrl+O` at the title screen raised the
  real "Starting a new game will quit..." confirmation dialog (`ID_FILE_LOADGAME`, via the deferred pattern
  in §3.3), `Escape` dismissed it (No/cancel) cleanly, no repeat/stuck-key symptom. Sent via `SendInput()`
  (real scan-code-bearing synthetic input), not raw `PostMessage()`'d `WM_KEYDOWN` — see §6's note on why
  the latter is unsafe to use for testing this app.

  **Follow-up, same session: the command line's own editing keys had the identical "GLFW callback never
  fires for this" gap, on two different sub-paths.** Reported as "backspace doesn't work when entering
  text" - and Backspace was one instance of a wider pattern.

  - **Control characters (Backspace, and Emacs-style `Ctrl+`letter shortcuts) never reach
    `io.InputQueueCharacters`.** GLFW's char callback only ever delivers printable text - not Backspace
    (`do_char()`'s `case 8:` was correct, just unreachable, exactly like Enter before it) and never at all
    while Ctrl is held (by design - GLFW treats Ctrl+key as a shortcut, not text). `do_char()` already had
    correct handlers for the Emacs bindings (`^B`/`^D`/`^E`/`^K`/`^N`/`^P`/`^U`/`^V`/`^Y` - back/forward a
    char, delete, history, etc.), all equally unreachable. Fixed with manual per-key `IsKeyPressed()` checks
    in `event_loop()`, mirroring the existing Enter case. `^A`/`^C`/`^F`/`^X`/`^Z` were deliberately **not**
    restored this way - they're claimed by SelectAll/Copy/Find/Cut/Undo in the `IDR_ACCEL_WIN`/
    `IDR_ACCEL_EMACS` accelerator tables (same conflict the *original* Win32 app had: `TranslateAccelerator()`
    always ate the keystroke before it could become a `WM_CHAR`, so these `do_char()` cases were already
    dead code there too - restoring them here would be *new*, not restored, behavior, and `do_cut()` isn't
    safe to invoke twice on one keypress). `Ctrl+V`/`Ctrl+Y` are each claimed by only one of the two
    accelerator tables (Paste is bound to "V" in the Windows-style table, "Y" in the Emacs-style one), so
    each harmlessly double-invokes `do_paste()` through both paths in the *other* table's style - accepted
    since paste has no already-consumed state to conflict with, unlike cut.
  - **Command-line navigation (arrows, Home/End, PageUp/PageDown, Delete, Escape-to-clear-line) goes through
    `do_keydown()`, not `do_char()` at all - and nothing anywhere in guit3 was calling `do_keydown()`.**
    Worse than the control-character gap: this isn't a GLFW limitation, just a missing call. Unlike
    `do_char()`, whose `CTadsWin` default already forwards to `m_children` (the portable substitute for
    Win32 keyboard-focus routing, migration.md 3.4a), `do_keydown()`'s default was a bare `{ return FALSE;
    }` - fixed in `tadswin.h` to forward the same way. `event_loop()` now calls `do_keydown(VK_UP/DOWN/
    LEFT/RIGHT/HOME/END/PRIOR/NEXT/DELETE/ESCAPE, 0)` on `IsKeyPressed()` edges, mirroring the Ctrl-letter
    block above. Ctrl+Left/Right (word-left/right) and Ctrl+Home/End (top/bottom) needed no extra table
    entries - `do_keydown()` itself branches on live Ctrl state (`get_ctl_key()`, a real `GetKeyState()`
    query - unlike `GetFocus()`, this one was never broken) for the same `VK_LEFT`/`RIGHT`/`HOME`/`END`
    codes. F1-F10 (`CMD_F1..F10`) were deliberately left unwired: `VK_F1`/`VK_F3` collide with the
    `ID_HELP_COMMAND`/`ID_EDIT_FINDNEXT` accelerator bindings the same way `^F`/`^C`/etc. do above.
  - Both blocks also gained the `!ImGui::IsPopupOpen(0, ImGuiPopupFlags_AnyPopup)` guard `do_accel_keys()`
    already needed (a non-text-widget dialog doesn't set `io.WantTextInput` on its own), extended to the
    whole character-forwarding block including the pre-existing Enter case, closing that same leak for
    every key in one place rather than duplicating the guard per block.

  **Verified**: clean build + link, `0 warnings`. Interactive `ditch3.t3` tests via `SendInput()`: typed
  `xyz`, Backspace × 2 → `x` (confirms Backspace); typed `ac`, `Ctrl+B`, typed `b` → `abc`, i.e. the cursor
  genuinely moved back one character before the insert (confirms an Emacs `Ctrl+`letter binding end to end).
  Arrow-key/history-recall testing hit the `SendInput()`-doesn't-reach-`VK_UP` caveat recorded in §6 below;
  the user confirmed Up-arrow history recall works correctly with a real keypress once the synthetic-input
  gap was identified via temporary log-file instrumentation, so the `do_keydown()` wiring is confirmed
  working even though the automated part of this particular check came back inconclusive rather than
  positive.

  **Second follow-up, same session: `Alt+F` (etc.) not opening the File menu.** A third instance of "this
  ImGui build never gained a Win32 keyboard-navigation feature" - not a GLFW gap or a missing call this
  time, but a feature `render_menu_bar()` genuinely never had: §3.1 already noted "this ImGui build does
  not parse `&` mnemonics," but that was recorded as a *label-rendering* footnote, not flagged as a missing
  *behavior* until a user comparison against `htmlt3` surfaced it. **Fix**: `CHtmlSys_mainwin::
  pending_menu_mnemonic_` (`htmlgui.h`) records the letter from an `Alt+`letter press in `event_loop()`
  (checked against `F`/`E`/`V`/`T`/`G`/`H` - `win32/htmlcmn.rc`'s real `&File`/`&Edit`/`&View`/`&Themes`/
  `&Go`/`&Help` mnemonics, which all happen to be each label's first letter); `render_menu_bar()` consumes
  it right after `BeginMainMenuBar()`, calling `ImGui::OpenPopup(label)` for the matching menu immediately
  before that menu's own `BeginMenu(label)` call. That ID-stack position is required, not incidental:
  `ImGui::OpenPopup(str_id)` computes `g.CurrentWindow->GetID(str_id)`, and `BeginMenuEx()` (in
  `imgui_widgets.cpp`) computes its own popup ID the same way, then simply checks `IsPopupOpen(id)` before
  deciding whether to render as open - regardless of whether a click, hover, nav, or (now) an external
  `OpenPopup()` call put it on the open-popup stack. Calling `OpenPopup()` from any other context (a
  different window/ID-stack depth) would compute a different, non-matching ID and silently do nothing.
  **Gotcha caught by testing, not by reading the code**: the mnemonic check was originally folded into the
  same `game_wants_keys`-gated block as the Ctrl-letter/nav-key checks above - which meant it stopped
  firing the moment a menu was actually open, since an open `BeginMenu()` dropdown is itself a popup and
  `game_wants_keys` deliberately excludes "any popup open" (to keep game commands from reaching past a
  *modal* dialog). That silently broke switching from one open top-level menu to another (`Alt+F` then
  `Alt+E` left File open instead of switching to Edit) while the *first* mnemonic press still looked
  correct in isolation - moved to its own `!io.WantTextInput`-only gate (dropping the popup-open
  exclusion, which doesn't apply here) to fix it. **Verified**: clean build + link, `0 warnings`;
  interactive `ditch3.t3` test via `SendInput()` - `Alt+F` opened the File dropdown exactly like a click,
  and a follow-up `Alt+E` correctly switched to Edit rather than leaving File open or stacking both.

  **Third follow-up, same session: `Ctrl+V` paste duplicated the pasted text.** A real regression from
  the Ctrl-letter work above, not a GLFW/ImGui limitation: the manual Ctrl-letter block dispatched
  `do_char()`'s cases 22/25 (`^V`/`^Y`, both meaning Paste in one preference style or the other)
  *unconditionally*, without checking whether the *current* style's accelerator table already claimed
  that exact key - Ctrl+V is bound to `ID_EDIT_PASTE` in the Windows-style `IDR_ACCEL_WIN` table, so
  `do_accel_keys()` and the manual block both fired `do_paste()` for the same keypress. Unlike Copy/Cut/
  SelectAll's harmless-enough double-fire, a second `do_paste()` genuinely re-inserts the clipboard text,
  visibly duplicating it. **Fix**: each of `^V`/`^Y` now fires only in the preference style where it's
  *not* already accelerator-bound - `^V` only in Emacs style (where it means page-down, not paste, and
  isn't in `IDR_ACCEL_EMACS` at all), `^Y` only in Windows style (where `IDR_ACCEL_WIN` doesn't bind Y).

  **Chasing `^Y`'s fix uncovered a second, unrelated bug, then an even-more-unrelated environment quirk
  that had been faking a third one.** `^Y`, dispatched the same way as the other Ctrl-letter shortcuts
  (`ImGui::IsKeyPressed()` → `do_command(0, ID_EDIT_PASTE, 0)` from the char-forwarding block in
  `event_loop()`), reliably left the correct text in `cmdbuf_` (confirmed with temporary
  `fprintf`-to-a-log-file instrumentation in `do_paste()`/`CHtmlInputBuf`) but never visibly repainted it
  - while the *identical* `do_command()` call, dispatched from `do_accel_keys()` using its
  `glfwGetKey()`/`accel_key_down_`-based edge detection instead of `ImGui::IsKeyPressed()`, worked
  correctly every time. Root cause not pinned down (a plausible guess: a GLFW/ImGui key-state
  discrepancy between the poll-based and callback-based paths, specifically for a key without its own
  entry in the loaded accelerator table), but empirically conclusive after several rebuild-and-test
  cycles, so `^Y` is now dispatched from `do_accel_keys()`'s edge-detection instead of the
  `IsKeyPressed()`-driven Ctrl-letter block - see the comment at its call site for any future non-table
  key that needs the same treatment.

  Midway through isolating that, a **second, cleanly separate false lead** wasted real time: synthetic
  `Ctrl+Y` (`SendInput` with `wVk = VK_Y`) stopped reproducing the paste at all partway through, on what
  looked like a config-neutral retest. Cause: this machine's active keyboard layout is German (QWERTZ),
  confirmed via `GetKeyboardLayout()` returning language ID `0x0407`; QWERTZ swaps the `Y` and `Z`
  positions relative to US QWERTY. `SendInput`'s `wVk` is layout-remapped (it sends whichever physical
  key is *labeled* `Y` under the active layout), while GLFW reports key constants by *physical position*
  matching the US layout's labeling - so on this machine, `VK_Y` and `GLFW_KEY_Y` refer to two different
  physical keys. Sending `VK_Z` instead (the physically-correct key for `GLFW_KEY_Y` here) reproduced the
  paste reliably. **Any future synthetic `Ctrl+`letter/`Alt+`letter test on a non-US-QWERTY system needs
  this cross-check - a `SendInput` VK constant and a GLFW/ImGui key constant of the "same" letter are not
  guaranteed to be the same physical key.** See also §6's `SendInput` notes below.

  **Verified**: clean build + link, `0 warnings`; interactive `ditch3.t3` tests via `SendInput()` (using
  the physically-correct key per the layout note above) - `Ctrl+V` now pastes exactly once in the default
  Windows style; `Ctrl+Y` now pastes exactly once too, in isolation and without affecting `Ctrl+V`.

- **M. `guimain.cpp` startup/shutdown** — *done (A2 seam for the one real OS-service piece; two items
  turned out to be dead-code deletions, two are left alone as out-of-scope Windows plumbing).*
  [guios.h](guios.h)/[guios_w32.cpp](guios_w32.cpp) gained `os_init_debug_console()`/
  `os_close_debug_console()`, the `AllocConsole()` call and the drain-then-wait-for-a-keystroke shutdown
  loop moved verbatim out of `guimain.cpp`'s `init_debug_console()`/`close_debug_console()` (guarded by
  `TADSHTML_DEBUG` inside the backend exactly as before, so the interface `main()` calls is unconditional).
  `os_dbg_sys_msg()` itself was left where it was - it's already the established "one implementation per
  port" symbol every other port's own main file defines (see `win32/w32main.cpp`, `win32/w32webui.cpp`),
  not a call guit3's own code invokes, so there's nothing to route through a `guios` hook.
  - **`LoadLibrary("RICHED32.DLL")` — audited and deleted.** A repo-wide search turned up no
    `RichEdit`/`RICHEDIT`/`EM_*` rich-edit-control usage anywhere in `guit3`; the `HINSTANCE rich_ed_hdl`
    variable and its matching `FreeLibrary()` at shutdown went with it.
  - **The `CreateFile`/`WriteFile` crash-dump writer (`exc_handler()`, `tadscrsh.txt`) → `fopen`/`fwrite`/
    `fclose`.** This function is Windows SEH (`EXCEPTION_POINTERS`, raw `ctx->Ebp`/`Eip` stack walking) and
    stays Windows-only regardless; the ask was just to stop mixing raw Win32 file I/O into it. `fclose()`
    flushes before returning, so the dump file still survives `EXCEPTION_CONTINUE_SEARCH` tearing the
    process down right after.
  - **`InitCommonControlsEx` — audited, *not* removed.** Migration.md's original text expected this to go
    with the dead dialog files, but `tadsdlg2.cpp` (still compiled into `guit3`, per `CMakeLists.txt`) still
    creates real `WC_TABCONTROL`/`WC_TREEVIEW` child windows in a `WM_INITDIALOG` handler; confirming every
    path into that handler is truly unreachable is a bigger audit than this item, so the call stays with a
    comment recording why, to be dropped alongside `tadsdlg2.cpp` in a future cleanup pass.
  - **`GetModuleHandle`/`oss_G_hinstance` — left alone**, per this bullet's own original scope: it's a plain
    `HINSTANCE` assignment `oswin.c` requires by convention, with no non-Windows concept to abstract behind
    a hook; it becomes a Windows-only line under `#ifdef _WIN32` whenever M4 needs one, not before.
  - **`CoInitialize`/`CoUninitialize` — already resolved (M1, §5.4/O)**, no further action: they stay
    unconditional because `tadswin.cpp`'s real OLE drag-and-drop needs COM regardless of the Web UI flag.

  **Verified**: clean build + link of `guit3`, `0 warnings` on the two touched TUs (`guimain.cpp`,
  `guios_w32.cpp`); fresh launch on `ditch3.t3` — title screen renders with menu bar/toolbar/status bar and
  the game's cover image intact, process stays up (screenshotted), and a clean `Stop-Process` shutdown with
  no crash-dump file written, confirming the debug-console/crash-writer changes didn't disturb normal
  (non-debug, non-crashing) startup and shutdown.

**A2 done for all of B–M.** (H — images — needed no `os_*` hook, just deletion of the two Win32 calls,
§5.4/H. I — audio file I/O — is done: WAV/Ogg/MP3 decoders on the `osfile` API, `getbits.cpp` forked into
`imgui/`, §5.4/I. J — file-dialog browsing — is done: both dialogs routed through the existing portable
`osifc` filesystem API (`os_open_dir()` et al.), `PathMatchSpecA` replaced by an in-file glob matcher,
`Shlwapi.lib` dropped, §5.4/J. H, I and J each collapsed A2 and M3 — no `os_*` hook, no separate portable
backend. L's canonical key enum is likewise already portable — `GLFW_KEY_*` values don't vary per platform
— so, like H/I/J, there's no separate M3 backend left for it either; only `os_key_to_char()`/
`os_char_to_key()` need one, matching G/K's shape. M's own seam (`os_init_debug_console()`/
`os_close_debug_console()`) is the same shape again — trivial on a non-Windows backend, likely empty
functions, but that file doesn't exist until M4.)

**M3 — fill in portable implementations, cheapest-and-most-certain first.** GLFW-provided services (D) →
file dialogs (J) → system colors (E) and shell (F) → settings store (C) → resources (B) →
fonts (G) → images (H) → audio file I/O (I) → charset (K) → keyboard/accelerators (L).
Each is landable on Windows alone; the pure-portable backends (D/E/F) only *run* once M4's Linux build
exists.

**M3 is now complete** — every item in that list (D, J, E, F, C, B, G, H, I, K, L) has a non-Windows
backend. (Item M, the debug console, was never part of this list - see its own note below - and stays open
for M4.) **M4 is the next milestone**: flip the three build gates (§5.1) and get an actual Linux build, at
which point all of this can be compiled and exercised for real for the first time.

**D + E + F + J + C + B + G + K done.** D/E/F: [guios_portable.cpp](guios_portable.cpp), see the
"M3/D-F — portable backend landed" note in §5.4 above. J routed the two file dialogs straight through the
existing portable `osifc` filesystem API so it needed no `guios` backend at all (§5.4/J). H, I and L were
already fully done (H removed the Win32 image calls outright; I moved the decoders onto the
already-portable `osfile` API, §5.4/H, §5.4/I; L's canonical key enum is already portable, and its two
OS-layout queries got a Win32 backend with nothing left to add for a non-Windows one to plug into, §5.4/L).

The remaining four landed in this pass:

- **C** — [tadssettings_portable.cpp](tadssettings_portable.cpp): an in-memory key-node tree (subkey
  enumeration included, per §5.4/C's note that a flat file doesn't give that for free) loaded from and
  rewritten to a small INI-style file at `$XDG_CONFIG_HOME/HTML TADS 3/settings.ini` (macOS:
  `~/Library/Preferences/HTML TADS 3/`), `HTML TADS 3` matching `w32_appdata_dir` (guitrt3.cpp). Binary
  values (the one live case: the custom-color swatch array) are hex-encoded; string values are
  backslash-escaped for embedded newlines.
- **B** — [guires_data.h](guires_data.h)/[guires_data.cpp](guires_data.cpp) embed `win32/runtbar.bmp` and
  `notes3/license.txt` verbatim as byte arrays (mechanically generated with `xxd -i`, not hand-maintained);
  `guios_portable.cpp` adds a generated `os_load_string()` table (one entry per `IDS_*` id actually routed
  through the hook, text copied from `win32/htmlcmn.rc`'s STRINGTABLE) and a small self-contained
  1/4/8bpp-indexed BMP decoder for `os_load_toolbar_rgba()` (no GDI to lean on, so the color-key → alpha
  conversion runs against the decoded pixels instead of a GetDIBits() result).
- **G** — font hooks split out of `guifont.cpp` (which turned out to need nothing Windows-specific in
  `CHtmlSysFont_win32`'s own methods — FreeType/ImGui calls only — so it's now compiled on every platform)
  into three per-platform files picked by CMake: `guifont_w32.cpp` (Win32, unchanged code), `fcfont.cpp`
  (Linux, fontconfig's `FcFontList`/`FcFontMatch`), `ctfont.cpp` (macOS, CoreText's
  `CTFontManagerCopyAvailableFontFamilyNames`/`CTFontCreateCopyWithSymbolicTraits`).
- **K** — `guios_portable.cpp` routes `os_local_to_utf8()`/`os_local_to_utf16()`/`os_utf8_to_local()`
  through the TADS charmap layer (`charmap.h`) rather than a second parallel code-page table: a Windows
  code-page number maps straight to a `"cp<N>"` charmap table name, which is exactly the naming convention
  `tads3/charmap/*.tcm` already uses, loaded through a bare `CResLoader` (so it searches the current
  directory — same as `CResLoader`'s other bare-constructor callers, e.g. `msgcomp.cpp` — good enough until
  a real M4 Linux build shows otherwise) and cached forever; a table that fails to load falls back to
  built-in ASCII rather than losing all GUI text.

**Verification note:** C and B compile as pure host-side C++ with no Linux/macOS-only headers, so both were
syntax-checked end-to-end against this repo's real headers under WSL (Ubuntu 24.04, g++ 13, the same
`UNIX`/`OS_ANSI`/`LINUX_386` defines `tads_settings` sets for a real Unix build) with zero errors or
warnings; K's `guios_portable.cpp` changes were checked the same way. G's Linux backend (`fcfont.cpp`) was
additionally checked against a real `libfontconfig-dev` install in the same WSL environment. `ctfont.cpp`
(macOS) could not be checked at all — no macOS toolchain is available here — and is unverified beyond
inspection against the CoreText API; revisit it once M4 has a Mac build. None of this can actually *run*
until M4 lifts the Windows-only CMake gate (`if (NOT WIN32) return()`), and the Windows build was
re-verified after every change in this pass (clean build + link + a `ditch3.t3` smoke-test launch, no
crash-dump file).

M's A2 seam is built but its M3 half untouched - `os_init_debug_console()`/`os_close_debug_console()` need
a non-Windows backend (almost certainly a pair of empty functions, since a console window isn't needed
when stdout already goes somewhere visible), §5.4/M.

**M4 — flip the three gates (§5.1) and get a Linux build.** Expect a long tail in `htmlgui.cpp`/`tadswin.cpp`
that no census can predict; that's the point of doing M1–M3 first, so what the compiler finds is a
manageable remainder rather than thousands of errors. Add a Linux **compile-only** CI job at this point — it
is the cheapest possible regression net for a GUI app with no automated test coverage. Per the standing
decision, CI workflows for htmltads-specific artifacts belong in **this** repo, not `tads-runner`.

**M4 is done: guit3 builds, links, and runs its main loop on Ubuntu 24.04 (WSL)** without crashing, tested
against `ditch3.t3`. The Linux CI job described above is not yet added — still open, see the end of this
section.

**The three gates:**
- `tads-runner/CMakeLists.txt` §5.1 gate 1: `WITH_HTMLTADS` is now unconditional whenever `../htmltads`
  exists (was `if (WIN32 OR EMSCRIPTEN)`).
- `htmltads/CMakeLists.txt` gate 2: `libogg`/`libvorbis` (guit3's Ogg Vorbis decoder, `tadsvorb.cpp`) now
  build on every platform; `textindex`/`scintilla`/`t3doc`/`wbaddons` (Workbench-only, unrelated to guit3)
  stay Windows-only.
- `htmltads/htmltads/imgui/CMakeLists.txt` gate 3: the `if (NOT WIN32) return()` is gone. `../win32/htmlt3.rc`
  and the Win32-only link libs (`Comctl32.lib`, `Winmm.lib`, `Ws2_32.lib`, `Wininet.lib`, `Mpr.lib`,
  `Version.lib`) are now added only `if (WIN32)`; `tadsole.cpp`/`tadsdlg2.cpp` (both fully Windows-only, see
  below) likewise. `OpenGL::GL` (via `find_package(OpenGL)`) is linked on the `else()` side where
  `Opengl32.lib` was Windows-only.

**Two real, pre-existing bugs found and fixed along the way (not guit3-specific - they'd bite any Linux
build of this tree, and are worth remembering if this ever gets re-derived from a cleaner tree):**

- **`tads3/CMakeLists.txt`'s `find_package(CURL)` guard was `if (NOT ${CURL_FOUND})`** - the `${}` expands
  an *undefined* `CURL_FOUND` to nothing, producing `if (NOT )`, which CMake evaluates as `if(NOT <the
  string "NOT">)`-adjacent nonsense that came out **true-as-in-"skip the block"** here, i.e. `find_package`
  never ran and `VM_WITH_NETWORK` silently defaulted off even with libcurl installed. Fixed by dropping the
  `${}` (`if (NOT CURL_FOUND)`, the idiomatic form) - confirmed `-- Found CURL` and `VM_WITH_NETWORK=ON` on
  the very next configure. If networking-dependent code (`osnet_disconnect_webui()`, `os_net_cleanup()`,
  `guitrt3.cpp`) is ever missing at Unix link time again, check this first before assuming libcurl isn't
  installed.
- **`tadsplat.h`'s `HRESULT` (and `LONG`) were `typedef long ...`.** Windows' `LONG`/`HRESULT` are always 32
  bits (LLP64), but Linux's `long` is 64 bits (LP64) - so every `E_FAIL`-style negative-as-32-bit HRESULT
  constant silently became a large *positive* 64-bit value, and `SUCCEEDED()` read every one of the stub
  COM calls below as having succeeded. This produced a real crash: `CTadsApp::get_my_docs_path()`
  (tadsapp.cpp) calling a vtable method (`imal->Free()`) through a null `IMalloc*` because the
  `SHGetSpecialFolderLocation()` stub's `E_FAIL` read as success. Fixed by making both `typedef int32_t`
  instead. **`DWORD`/`ULONG` are still plain `unsigned long` (also 64-bit on Linux)** - left alone since
  nothing does a signed comparison on them the way `SUCCEEDED()`/`FAILED()` do on `HRESULT`, but worth
  revisiting if something behaves oddly with a "should be 32-bit" unsigned Windows type.

**A third real bug, in shared (non-Windows-specific) code**: `CResLoader::open_res_file()` (tads3/resload.cpp)
built a full path from `root_dir_` and a default-library name **without checking whether `root_dir_` was
null** in the `deflib`-fallback branch (the primary lookup a few lines above it does check). A bare
`CResLoader()` (used by `guios_portable.cpp`'s charmap loader, §5.4/K) has a null `root_dir_` by design, so
the very first `os_local_to_utf8()` call crashed inside `os_build_full_path()`'s `strlen(path)` on a null
pointer. Fixed by skipping `os_build_full_path()` and using the bare filename when `root_dir_ == 0`, matching
the primary lookup's existing null-check.

**The "long tail" itself**, roughly in the order it was found (all in `htmltads/htmltads/imgui/` unless
noted):

- **Every `.cpp`/`.h` that did `#include <windows.h>` (or `<Ole2.h>`, `<WinSock2.h>`, `<CommCtrl.h>`, ...)
  unconditionally** now does so only `#ifdef _WIN32`, falling back to `"tadsplat.h"` - roughly 30 files.
  `tadsplat.h`'s non-Windows branch (§5.4/A1) grew enormously in this pass: real `IUnknown`/`IDropSource`/
  `IDropTarget`/`IDropTargetHelper`/`IDataObject` base classes (mirroring real `oleidl.h` layout closely
  enough that `CTadsWin`'s existing multiple-inheritance-without-virtual-`IUnknown` pattern still works
  unchanged), `FORMATETC`/`STGMEDIUM` (just enough for `CHtmlSysWin_win32_Input`'s real, always-compiled
  `IDropTarget::DragEnter()`/`Drop()` - the OLE drag-and-drop *source* side, `tadsole.cpp`/`tadsole.h`, stays
  fully Windows-only, gated in both the `.h` and the CMakeLists), a `GMEM_FIXED`-only working
  `GlobalAlloc`/`GlobalLock`/`GlobalUnlock`/`GlobalFree` (do_copy()'s clipboard path is live off Windows
  too), and a long tail of inert no-op stand-ins (menus, GDI, dialogs, hooks, registry, version-resource
  lookup, ...) for code that's provably dead off Windows because guit3 never creates a real `HWND`
  (migration.md 3.4/3.4a) - `RegisterDragDrop()`/`CoCreateInstance()` etc. returning failure is exactly what
  makes `CTadsWin::drop_target_register()` correctly skip registering a real drop target off Windows.
- **`tadsdlg2.cpp`** (100% dead native property-sheet code, confirmed no live call sites) and **`tadsole.cpp`**
  (COM drag-source, Windows-only by design) are now `if (WIN32)`-only in the CMakeLists rather than ported.
- **`tads2/msdos/oswin.h`'s Windows-only `oss_set_open_file_dir()`/`oss_win_free_all()`/
  `oss_win_static_init_done()`/`oss_set_askfile_hook()`** have no non-Windows implementation at all (the
  Unix build of `Tads::tr32h` compiles `unix/osunixt.c`, not `oswin.c` - migration.md 5.1). Stubbed as
  no-ops directly in `tadsapp.cpp`/`guimain.cpp` (small, call-site-local, not worth a shared header). Real
  gap: **`os_askfile()` has no working hook wiring on Unix** - `unix/osunixt.c`'s own `os_askfile()` is
  compiled out under `USE_STDIO` (which this build defines), so `askf_tx.c`'s plain stdio prompt, not
  `CTadsFileDialog`, currently backs File > Open/Save/Restore off Windows. Revisit once someone actually
  exercises that path.
- **`html_os.h`'s switchboard** (which routes to `hos_gui.h` only when `IMGUI` is defined) needed `t3htm`
  and `tr32h` themselves to define `IMGUI` on the plain-Unix branch (not Emscripten) plus an include path to
  `imgui/`, since guit3 is their only non-Windows, non-Emscripten consumer - see `tads2/CMakeLists.txt` and
  `tads3/CMakeLists.txt`.
- **`tads2/unix/osunixt.c` defines its own `os_term()`/`os_advise_load_charmap()`**, conflicting with
  `hos_gui.cpp`'s guit3-specific versions (`os_term()` needs to do ImGui/GLFW shutdown, not just `exit()`).
  Both are now `#ifndef IMGUI` in `osunixt.c`, matching the `t3htm`/`tr32h` `IMGUI` define above. **Do not**
  try to fix this by disabling `USE_DOSEXT` tricks or moving files around - `osunixt.c`'s own `ossgetcolor()`/
  `oss_get_sysinfo()` (curses text-mode color scheme globals `sdesc_color`/`text_bold_color`/`os_f_plain`,
  meant to be defined by a real console front end) still need to link, so it's kept in the build and given
  harmless dummy definitions of those three globals in `hos_gui.cpp` instead.
- **`tads2/osnoui.c`'s `os_get_rel_path()`** (guit3's recent-games menu) was nested inside a big
  `#ifdef USE_DOSEXT` block along with several functions (`os_defext`/`os_remext`/`os_addext`/
  `os_get_root_name`/`os_build_full_path`/`os_combine_paths`/`os_is_file_absolute`) that `unix/osunixt.c`
  *already implements natively* - Unix intentionally never defines `USE_DOSEXT` (`unix/osunixt.h` has it
  commented out) specifically to avoid that clash. `os_get_rel_path()` is the one function in that block with
  no Unix implementation anywhere and no platform-specific logic outside its own internal
  `#if defined(MSDOS)` branch, so it (plus the tiny `pathchareq`/`ispathchar`/`oss_parse_volume` helpers it
  needs) was pulled out of the `USE_DOSEXT` gate to compile unconditionally, leaving the rest of that block
  exactly as gated as before. **Do not re-enable `USE_DOSEXT` for Unix** - it was deliberately left off and
  doing so reintroduces the `osunixt.c` conflict this avoided.
- **`tads3/unix/osunix.c`** (`os_get_exe_filename()`/`os_get_special_path()`) and **`tads2/osrestad.c`**
  (`os_get_str_rsc()`, the generic "compiled-in English strings" default every other non-Windows port
  already uses) needed adding to `t3htm`'s and `tr32h`'s respective plain-Unix source lists - previously only
  Emscripten (for the former) and Windows' `oswin.c` (for the latter) provided these.
- **`os_key_to_char()`/`os_char_to_key()`/`os_load_accel_table()`/`os_init_debug_console()`/
  `os_close_debug_console()`** (guios.h items L/M) got real non-Windows implementations in
  `guios_portable.cpp`: the keyboard functions lean on GLFW's named key constants being ASCII-aligned for
  the printable range (a table only for shifted punctuation/digits); the accelerator table is a
  hand-transcribed copy of `IDR_ACCEL_WIN`/`IDR_ACCEL_EMACS` from `win32/htmlcmn.rc` (keep both in sync if
  the `.rc` bindings ever change); the debug-console hooks are empty, per the M3 note's own prediction.
- **`guitr.cpp` got a new `os_input_dialog()`** (backs the TADS `inputDialog()` intrinsic, no non-Windows
  implementation existed) built on the same `tadswin_message_box()` used by `w32_msgbox()` just above it.
  Known gap: only OK / OK-Cancel / Yes-No are supported (matching `tadswin_message_box()`'s own limit);
  Yes-No-Cancel falls back to Yes-No, and custom-labeled buttons aren't supported at all. `inputDialog()` is
  rarely used; revisit if a real game needs the missing cases.
- **`CHtmlPreferences::cust_refresh_font_lists()`/`cust_font_select_*()`/`cust_font_enum_cb()`** (htmlpref.cpp,
  the Customize Theme dialog's font-family lists) are raw GDI `EnumFontFamiliesEx()` and stayed
  `#ifdef _WIN32`; there's no fontconfig-backed "enumerate every family, classified serif/sans/script/
  typewriter" equivalent yet (`fcfont.cpp`'s hooks only test/fetch one name at a time). The lists are simply
  empty off Windows for now - a real functional gap, not just a compile stub, but out of scope for getting
  Linux building.
- **`guiwebui.h`/`tadswebctl.h`** (the embedded Web UI window, phase two / §4) are now `#ifdef _WIN32`-gated
  in their entirety - they were being included unconditionally (regardless of `TADS_WEBUI_ENABLED`, which
  only gates the `.cpp` implementations) and pulled in `<exdisp.h>` and friends.
- **`guit3.cpp`'s `w32_webui_yield_foreground()`/`w32_webui_to_foreground()`** call the networking-layer
  `osnet_webui_*` functions (`win32/osnetwin.h`/`win32/osnet-connect.cpp`, Windows-only, no Unix
  equivalent) unconditionally; now `#ifdef _WIN32`-guarded, no-ops off Windows.
- **`tads2/unix/osunixt.h` used to `#define remove(filename) unlink(filename)`.** This macro-poisons the
  identifier `remove` for the rest of the translation unit, breaking `<cstdio>`'s `using ::remove;` in any
  C++ file that includes this header before `<string>`/`<cstdio>` (which several guit3 files do
  transitively via `tadswin.h`). glibc's own `remove()` (`<stdio.h>`, ISO C) already does exactly what the
  macro did for regular files, so the macro was simply redundant on Linux - removed outright rather than
  worked around.
- **`htmltads/jpeg/jconfig.h`'s `typedef unsigned char boolean;`** collided with `libmng_types.h`'s own
  `typedef int boolean;` (both get included in the same libmng translation units) - jconfig.h already had an
  `__EMSCRIPTEN__`-specific `typedef int boolean;` carve-out for exactly this reason; broadened it to
  `defined(__EMSCRIPTEN__) || !defined(_WIN32)` so Linux gets the same fix.
- **`htmltads/htmltads/win32/mpegamp/mpegamp.h`** (shared, unmodified-until-now Win32 MPEG decoder header)
  had an unconditional `#include <Windows.h>` for one `HANDLE` member and one MSVC-only
  `#define inline __inline`. Both are now `#ifdef _WIN32`/`#ifdef _MSC_VER`-gated; off Windows `HANDLE` is
  `void*`, matching `imgui/getbits.cpp`'s existing comment that it already treats `CMpegAmp::in_file` as an
  opaque `osfildef*`-holding token.
- **`htmlgui.h`'s `enum htmlw32_directx_err_t`** is *used* (via an elaborated-type-specifier return type) in
  `htmlpref.h` before it's *defined* later in the same file - works on MSVC (which tolerates tentative enum
  forward references) but not on standards-conforming GCC/Clang. Fixed with an explicit-underlying-type
  forward declaration (`enum htmlw32_directx_err_t : int;`) added to `htmlpref.h`, matching the later
  definition's now-added `: int`.

**The Linux compile-only CI job is done**: `htmltads/.github/workflows/build.yml`'s existing `ubuntu-latest`
matrix entry (it already ran `cmake --preset default` + `cmake --build build/default` for Linux, just
against a build where `WITH_HTMLTADS` used to be off) now installs the packages guit3's Linux build needs
(GLFW's X11/Wayland backends, fontconfig, libcurl for `VM_WITH_NETWORK`) before configuring, so it now
actually builds `htmltads`/guit3 rather than skipping it. No separate job was needed since one already
existed. `release.yml` is unaffected (Windows-only build).

**Still open after M4:**
- **Visual/manual verification is still outstanding.** guit3 was confirmed to compile, link, and run its
  main loop under WSL Ubuntu 24.04 without crashing (`ditch3.t3`, several seconds, steady ~98% CPU as
  expected from an uncapped render loop) - but the actual rendered frame was never visually confirmed there:
  no X11 window ever appeared (`xwininfo -root -tree` found none), and the process logged
  `libEGL warning: ... MESA: error: ZINK: failed to choose pdev` at startup even with `WAYLAND_DISPLAY`
  unset before launch, while `glxinfo` on the same machine reports working direct-rendering GLX. This looks
  like a WSLg-specific Mesa/EGL software-rendering quirk rather than a guit3 defect (nothing in the port
  touches EGL, Wayland, or zink), but it was **not** run on a real Ubuntu desktop with a real GPU to confirm
  that theory - do that before trusting the Linux build's rendering path.
- `get_my_docs_path()`/`SHGetSpecialFolderLocation()` et al. now correctly report failure off Windows
  (post-HRESULT-fix), meaning the Options dialog's "Starting directory" default and similar features get no
  real value there rather than a wrong one - a real fallback (e.g. `$HOME`) would be a nicer follow-up.

**Windows re-verified after this pass**: `cmake --preset default` + `cmake --build build/default --target
guit3` still succeed and the exe still launches `ditch3.t3` cleanly. This caught one real regression before
it landed: the new `os_input_dialog()` (`guitr.cpp`, above) was originally unguarded and collided with
`oswin.c`'s real Windows implementation (`LNK2005`) - fixed by wrapping it in `#ifndef _WIN32` like
everything else added to a file that's still compiled on both platforms. Lesson for next time: anything
added to a *shared* file (as opposed to a per-platform file the CMakeLists selects between, like
`guios_portable.cpp`/`guios_w32.cpp`) needs its own `#ifdef`/`#ifndef _WIN32` guard, even if the function
"obviously" has no Windows implementation - it might.

**M5 — phase two.** The four unported windows (N, if not done earlier), a portable MIDI synth (TinySoundFont
+ a bundled GM soundfont through `CTadsAudioDevice`, §3.7), the real cross-platform embedded Web UI behind
the flag from O, and the Emscripten target as its own effort (§4).

**Two things worth doing out of band, whenever convenient:** source a sound-bearing `.t3`/`.gam` so the
miniaudio path can be verified by ear (§3.7), and click-test `CTadsFileDialog`'s Game Chest-tab nesting,
which has the latent nested-popup bug described in §3.3.

## 6. Working notes for a fresh session

### Building and running

`guit3`'s source lives in the `htmltads` repo, but it is **built from the `tads-runner` superproject**, which
auto-detects `../htmltads`. Iterate with:

```
cmake --build tads-runner/build/default --target guit3
```

A handful of `.cpp`s, seconds to a couple of minutes — no need to rebuild tads2/tads3/curl. Output:
`tads-runner/build/default/htmltads/htmltads/imgui/guit3.exe`.

Run it with a test game from `tads-runner/tests/`, with the working directory set there (that's where
`imgui.ini` and save files land). The charmap warning is fixed at the source (§2) — if it reappears, the
`POST_BUILD` copy step was removed or the build dir is stale.

### Verifying UI changes — there is no automated test coverage

- **Wait ~5 seconds after launch** before assuming the window exists; 3s was sometimes too early and produced
  false "window not found" results.
- **Find the window via `EnumWindows`/`GetWindowThreadProcessId`/`GetClassName`** filtered to the target PID,
  class `GLFW30`. Don't rely on `FindWindow` by class + null title.
- **Screenshot with `GetWindowRect` + `Graphics.CopyFromScreen`.** For small chrome (status bar, scrollbar
  track), crop and 3× upscale rather than eyeballing the full screenshot; `Bitmap.GetPixel()` is the reliable
  way to confirm an exact color reached the screen.
- **The Windows taskbar can bleed into the bottom few pixels** of a `GetWindowRect` capture (DWM extends the
  rect) — don't mistake that sliver for something the app drew.
- **Never fight for foreground.** Screenshotting a *freshly launched* process works reliably with no focus
  call at all. Forcing focus on an already-running window went wrong twice: `SetForegroundWindow()` silently
  failed (normal OS restriction) and the capture grabbed whatever *was* foreground — once an unrelated
  Outlook window; and `AttachThreadInput` + `SetForegroundWindow` got blocked outright by antivirus as
  "malicious script content". **Launch a fresh process per verification pass.** If focus really matters, use
  `(New-Object -ComObject WScript.Shell).AppActivate($pid)` (what `SendKeys` uses internally), and/or compare
  `GetForegroundWindow()` against the known `hwnd` before capturing so a mismatch aborts.
- **Prefer synthetic mouse clicks over synthetic keyboard input — and if keyboard input is unavoidable, use
  `SendInput()`, not `PostMessage(WM_KEYDOWN/WM_KEYUP)`.** `SetCursorPos` + `mouse_event` at a coordinate read
  off an actual screenshot worked every time. Raw `PostMessage()` was a repeated source of trouble: a
  `WM_KEYDOWN`/`WM_KEYUP` pair with a placeholder `lParam` (missing the real scan-code/repeat-count bits) was
  read as a *stuck key* and submitted dozens of blank commands over several minutes before it was caught —
  genuinely disruptive if the user is watching. `WM_CHAR`-only input is needed for `do_char()`'s Enter
  handling at the command prompt but didn't reliably reach the title screen's wait-for-keystroke state.
  **Update (§5.4/L verification): `user32!SendInput()` with a proper `KEYBDINPUT` (real `wVk`, a clean
  single down/up pair, no placeholder `lParam` since `SendInput` goes through the real input stack) worked
  reliably and safely** — used to verify `do_accel_keys()`'s real `Ctrl+O`/`Escape` dispatch and a
  `Ctrl+B` Emacs-editing shortcut with no stuck-key symptom. Prefer it over raw `PostMessage()` whenever
  synthetic keyboard input is unavoidable. **Caveat found testing arrow keys specifically**: a `SendInput()`
  tap of `VK_UP` (`0x26`) with no extra flags silently never reached GLFW at all — confirmed by temporary
  `fprintf`-to-a-log-file instrumentation showing the log was never even created, i.e. `IsKeyPressed()`
  never fired. The user then pressed the physical Up arrow key and confirmed the feature worked correctly,
  proving the gap was in the synthetic send, not the fix. Non-modifier navigation keys (arrows, Home, End,
  PageUp/Down) likely need `KEYEVENTF_SCANCODE` and/or `KEYEVENTF_EXTENDEDKEY` in the `KEYBDINPUT` to be
  recognized reliably (unlike the plain letter/Ctrl combos that worked fine as bare `wVk` taps) - untried,
  since asking the user to press the real key was faster once the log confirmed nothing was arriving.
  **When an app-level keyboard test comes back negative, check whether the synthetic input actually arrived
  before concluding the feature is broken** - a real keypress (or the log/instrumentation technique above)
  is a cheap way to tell apart "the fix doesn't work" from "the test didn't reach the app."
  **Second caveat, found testing `Ctrl+Y`: this machine's keyboard layout is German QWERTZ (confirmed via
  `GetKeyboardLayout()` → language ID `0x0407`), which swaps `Y` and `Z` versus US QWERTY.** `SendInput`'s
  `wVk` is layout-remapped (`VK_Y` sends whatever key is currently *labeled* Y), while GLFW/ImGui key
  constants are physical-position-based, matching US-layout labeling regardless of the active layout - so
  `VK_Y` and `GLFW_KEY_Y`/`ImGuiKey_Y` are two *different physical keys* on this machine. A synthetic
  `Ctrl+Y` test that mysteriously stops reproducing a bug partway through a session (looking like the bug
  "went away") may just be missing the swap. **Before testing any `Ctrl+`/`Alt+`letter combo synthetically
  on a non-US-QWERTY system, check the active layout and, if it remaps the letter in question, send the
  `VK_*` code for the key that's physically in the target `GLFW_KEY_*`/`ImGuiKey_*` position instead of the
  letter's own `VK_*` code.**
- **`Graphics.CopyFromScreen()` can silently capture the Windows lock screen** — it returned the same stock
  photo regardless of which window or region was requested, which looked exactly like "screenshots don't work
  in this sandbox" until the session turned out to have been locked. If a recipe that worked before suddenly
  returns identical images across unrelated windows, check for a locked session. `EnumWindows`/
  `IsWindowVisible` are a real-API fallback for yes/no visibility questions, but can't diagnose a
  rendering/layout bug.
- **Some sandboxes have no interactive desktop at all** — full-virtual-screen captures come back solid black
  and synthetic input has no effect. There, fall back to temporary `fprintf`-to-a-file instrumentation
  (below) and say plainly what was and wasn't verified.
- **`imgui.ini` persists the window position across launches**, so a window-relative click offset computed
  from one run's `GetWindowRect()` silently stops matching after a relaunch. Re-query per process.
- **Watch out for adjacent menu items with the same visible effect.** File > "Quit Game" sits directly above
  "Exit" and both produce the same in-game "Do you really want to quit?" prompt — a misclick looks like a
  successful test of the wrong thing.
- **Once a native dialog is replaced by an in-app one, stop checking for its window class.** A test that
  looked for `#32770` as a proxy for "warning shown" was written *after* the conversion, so it could never
  fire again and was silently testing nothing. Check rendered content instead.

### Debugging techniques that actually worked

- **When code-reading stalls, add temporary `fprintf()`-to-a-log-file instrumentation** in the suspect
  functions — rebuild, reproduce once via a single scripted mouse click, read the log, remove it. Several
  rounds of "this looks like the bug" fixes in the scrollbar/text-overlap investigation (§3.5) were
  real-but-insufficient; the log settled it in one pass. Reach for this *earlier* than feels necessary.
- **For render-side state, draw it on screen**: `ImGui::GetForegroundDrawList()->AddText()` with a static
  call counter or a capped event-trace string. That found both hidden layers of the caret bug (§3.9).
- **For ImGui's own internals, read them directly** — `g.BeginPopupStack` / `g.OpenPopupStack` from
  `imgui_internal.h` is what proved the nested-modal eviction (§3.3); `g.NextWindowData.HasFlags` ruled out a
  leaked size constraint (§3.2).
- **`glReadPixels` on the real backbuffer** (after `ImGui_ImplOpenGL3_RenderDrawData`, before
  `glfwSwapBuffers`) samples exact rendered colors — but see §3.8 for the coordinate-space trap.
- **A reported assert that doesn't reproduce in the `default` preset's `Release` build may still be real** -
  check it against a `Debug` build (`cmake --build build/vs --target guit3 --config Debug`) before writing it
  off. `/DNDEBUG` in Release compiles out the plain `assert()` Dear ImGui's `IM_ASSERT` maps to here, so a
  violated invariant (§3.3a's reentrant-`NewFrame()` bug) can run right through Release without a symptom.

### The one coordinate-space bug that keeps coming back

`m_pos` is **parent-relative** (the Win32 `MoveWindow` convention this code was written against);
`ImGui::SetNextWindowPos()` and `io.MousePos` are **absolute**. This was invisible for the whole port while
the outer window sat at screen `(0,0)`, and broke the moment the menu bar and toolbar pushed it down.

- **Rendering side**: `CTadsWin::do_render_content_begin()`'s `parent_` branch captures
  `ImGui::GetWindowPos()` *before* `SetNextWindowPos()` — at that point the parent's `Begin()`/`BeginChild()`
  is still open, so it correctly returns the parent's absolute position — and adds `m_pos` to it. Generalizes
  to any nesting depth.
- **Input side**: the *same mistake had a second, independent copy*.
  `CHtmlSysWin_win32::do_leftbtn_down()`/`do_mousemove()`/`do_setcursor()` each did `x -= m_pos.x`. They now
  subtract `CTadsWin::get_screen_pos()` ([tadswin.h](tadswin.h)), which walks the `parent_` chain summing
  each ancestor's `m_pos`. For this to agree with rendering, the top-level window's `m_pos` must track its
  real screen position every frame — `CHtmlSys_mainwin::do_render()` sets `m_pos = viewport->WorkPos`.

**Lesson: when a window-position bug shows up in this codebase, check for both a rendering-side and an
input-side copy of the same math.** The raw Win32-message-shaped input handlers (`do_leftbtn_down` etc.) were
written independently of the ImGui rendering code they now have to agree with, so fixing one does not fix the
other. §2's debug-window band and §3.2a's swallowed banner clicks are both instances of this same family.

### Finally

**Line numbers in this document drift.** `htmlgui.cpp` is ~19,400 lines and every edit shifts everything
below it. Treat every line reference as "was roughly here as of this writing" and re-`grep` for the
function or symbol name before trusting it.
