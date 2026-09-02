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
chrome is ImGui-native: menu bar, toolbar, status bar, all context menus (§3.1–3.2), and all five dialogs
— Options, Customize Theme, file open/save, Find, folder picker (§3.3). There are **no real Win32 windows
in guit3 at all** any more — not the frame, not banners/scrollbars/tooltips, not even `handle_` (§3.4,
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
- **Keyboard shortcuts are display-only.** `MenuItem()`'s shortcut parameter is cosmetic; the native
  `CTadsAccelerator` dispatch is equally dead. Real shortcuts need a new dispatcher in `event_loop()`
  calling `do_command()` from `ImGui::IsKeyPressed()` — separate task (§5.4).
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
  padding and linear filtering visibly bleeds neighbours.
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

### 3.3 Dialogs — done, all five

All five app dialogs are ImGui-native. The general `CTadsDialog`-mirroring base class this section once
recommended turned out to be unnecessary — each dialog follows the same small shape directly.

| Dialog | Implementation | Replaces |
|---|---|---|
| Options (Edit > Options) | `CHtmlPreferences::open_options_dialog()`/`render_options_dialog()` ([htmlpref.cpp](htmlpref.cpp)) | native property sheet, 8 `CHtmlDialog*PropPage` classes |
| Customize Theme | `open_customize_theme_dialog()`/`render_customize_theme_dialog()` (same file) | `run_appearance_dlg()`, 4 property pages |
| File open/save | `CTadsFileDialog` ([tadsfiledlg.h](tadsfiledlg.h)/[.cpp](tadsfiledlg.cpp)) | every live `GetOpenFileName()` call site |
| Find | `CTadsFindDialog` ([tadsfinddlg.h](tadsfinddlg.h)/[.cpp](tadsfinddlg.cpp)) | `CTadsDialogFind` (`guifndlg.cpp`) |
| Folder picker | `CTadsFolderDialog` ([tadsfolderdlg.h](tadsfolderdlg.h)/[.cpp](tadsfolderdlg.cpp)) | `CTadsDialogFolderSel2` (`foldsel2.cpp`) |

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
  `CHtmlSys_creditswin`, `CHtmlSysWin_win32_Popup`, in the 18k–19k line range of `htmlgui.cpp`) are
  `CTadsWin` subclasses that were never ImGui-ported and **do not render in guit3 today**. They compile with
  the token plus the no-op conversions. Making them work is outstanding (§5.4).

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
- **Deliberately still GDI**: the constructor's `CreateFontIndirect()` + `GetFontData()` trick that resolves
  a font *name* to its actual TTF/OTF bytes so FreeType has something to parse. **FreeType has no system
  font matching capability** — this is inherently OS integration, not rendering, and is the same underlying
  problem as `os_font_family_is_present()`. See §5.4.

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
- **Win32-only link libraries**, each marking a dependency to resolve or drop: `Htmlhelp.lib` (obsolete
  `.chm` help — drop, §5.4/F), `Comctl32.lib` (native controls — goes with the dead dialog files, §5.3),
  `Winmm.lib` (MIDI only, already `_WIN32`-gated), `Ws2_32.lib`/`Wininet.lib`/`Mpr.lib` (networking — the
  superproject already vendors `curl/`), `Shlwapi.lib` (`PathMatchSpecA`, §5.4/J), `Version.lib`,
  `dxguid.lib` (only the vestigial DirectSound probe, §5.4/I), `Opengl32.lib` (already `if (WIN32)`-guarded).
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
| `tadswin.h` | 144 | 2386 | Mostly **types in signatures** (`HWND`, `HMENU`, `LRESULT`, `RECT`, `SCROLLINFO`) — the handles are already opaque tokens (§3.4a). Plus dead MDI. |
| `htmlgui.cpp` | 135 | 19402 | The long tail: cursors, clipboard, `LoadString`, `GetSysColor`, `ShellExecute`, `HtmlHelp`, codepage conversion, `GetTickCount`, the toolbar bitmap loader, the unported About/Credits/License windows. |
| `tadswin.cpp` | 127 | 4255 | Same as `tadswin.h` plus the dead window-class registration and MDI. |
| `htmlgui.h` | 98 | 4583 | Types in signatures. |
| `htmlpref.cpp` | 80 | 5393 | Registry (theme profiles), `GetCurrentDirectory`, `EnumFontFamiliesEx`, plus the dead property-page classes. |
| `tadsdlg.cpp`/`.h`/`tadsdlg2.cpp` | 49/46/20 | 2463 | Dead except three entry points — §5.3. |
| `tadswebctl.h`/`guiwebui.h`/`tadscom.h` | 36/13/22 | — | Web UI, to be gated out — §5.4/O. |
| `tadsapp.cpp`/`.h` | 25/17 | 2000 | `MSG` pump, accelerators, modeless list, MDI — mostly dead — §5.4/L. |
| `tadsreg.cpp` | 21 | 419 | Registry — §5.4/C. |
| `guimain.cpp` | 21 | 956 | Startup/shutdown — §5.4/M. |
| `tadsmidi.cpp` | 20 | 2219 | Already `#ifdef _WIN32`. Phase two. |
| `foldsel2.cpp`/`guifndlg.cpp`/`iconmenu.cpp` | 20/16/12 | 2351 | Dead — §5.3. |
| `tadsimg.cpp`/`.h` | 15/8 | 700 | `CreateDIBSection` allocator, `AlphaBlend` gate — §5.4/H. |
| `tadsvorb.cpp`/`tadswav.cpp`/`mpegamp_w32.cpp`/`tadscsnd.cpp` | 10/8/9/2 | — | Decoder file I/O — §5.4/I. |
| `tadsfont.cpp`/`guifont.cpp`/`tadsfont.h` | 4/6/3 | 493 | The GDI font-bytes lookup and enumeration — §5.4/G. |
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
| `tadscbtn.cpp` | 231 | `CColorBtn`/`CColorCombo` — **zero references anywhere**. Drop now. |
| `guifndlg.cpp` | 908 | Superseded by `CTadsFindDialog` (§3.3). `CTadsDialogFindReplace`/`FindRegex` are Workbench-only. Drop. |
| `foldsel2.cpp` | 1011 | Superseded by `CTadsFolderDialog` (§3.3). Drop. |
| `iconmenu.cpp` | 432 | Owner-drawn menu icons — dead, but `htmlgui.cpp:10396` still `new`s an `IconMenuHandler` in `do_create()`. Remove that one construction, then drop the file. |
| `tadsdlg.cpp`/`.h`/`tadsdlg2.cpp` | 2463 | Dead **except** `CTadsDialog::modal_dlg_pre()`/`modal_dlg_post()`, `set_filedlg_center_hook()` and `LicenseDlg : CTadsDialog` — all four used only by the unported About/Credits/License windows (§5.4/N). Port those windows, then drop these. |
| `tadsole.cpp` | 430 | `CTadsDataObjText`, used by `htmlgui.cpp:1585`'s `get_data_object()` (OLE drag-and-drop source). Gate with the Web UI/COM flag (§5.4/O) or drop drag-out support off-Windows. |
| `tadswebctl.cpp`/`tadscom.cpp`/`guinogch.cpp` | ~470 | Web UI / COM — gate behind `TADS_WEBUI_ENABLED` (§4, §5.4/O). |
| MDI in `tadswin.h`/`.cpp` | — | `CTadsSyswinMdiFrame`/`MdiClient`, `client_handle_`. Workbench-only (§4). Strip while doing §5.4/A1. |

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
- `HtmlHelp` (`htmlgui.cpp:13867`) — obsolete `.chm` help. Drop it (point Help > Contents at the online docs)
  and `Htmlhelp.lib` goes with it.

**G. Fonts — two OS-integration hooks, not a rendering problem.**

- `os_font_family_is_present()` — the hook exists (§3.5); it needs a fontconfig backend (`fcfont.cpp`) and a
  CoreText one (`ctfont.cpp`).
- **A second hook is still needed**: font *name* → font file *bytes*, currently the `CreateFontIndirect()` +
  `GetFontData()` trick in `CTadsFont`'s constructor. FreeType cannot do system font matching. Propose
  `os_font_data_for_name(name, weight, italic, &bytes, &size)` alongside the presence hook, with the GDI
  implementation moving into `guifont.cpp` next to the enumeration one. Linux: `FcFontMatch` then read the
  matched file. macOS: CoreText's font URL.
- While there: `get_max_chars_in_width()` still opens a DC purely to call `select_font()` so the right ImGui
  font is pushed (§3.5). Untangle that so no DC is involved.

**H. Images.** `alloc_dib()`'s `CreateDIBSection` is now *only* an allocator (nothing blits the DIB) → plain
`os_alloc_huge()`. `get_alphablend_proc()`/`is_alpha_supported()` gate whether decoders keep an alpha channel
at all; off Windows this must simply return true, and arguably should on Windows too since GL always blends.
`guiimg.cpp`'s `*_win32` class names are cosmetic — rename opportunistically, not as a task.

**I. Audio — the decoders' file I/O is the last real blocker for a non-Windows digitized path.**
`CreateFile`/`ReadFile`/`SetFilePointer`/`CloseHandle` in `tadscsnd.cpp` (the shared `in_file_`),
`tadswav.cpp` (header + data reads), `tadsvorb.cpp` (the `datasource_t` callbacks) and `mpegamp_w32.cpp`
(`get_input()`). → the TADS `osfile` API (`osfoprb`/`osfrb`/`osfseek`/`osfpos`) already available through
`tr32h`. Mechanical but spans five files, and includes packed-struct WAV header parsing
(`PCMWAVEFORMAT`/`WAVEFORMATEX`) that should become explicit little-endian field reads. **Get a
sound-bearing test game first** — nothing in `tests/` has audio, so this is currently unverifiable by ear
(§3.7). Also here: drop `CHtmlSys_mainwin::get_directsound()`'s `LoadLibrary("DSOUND.DLL")` version probe
(nothing uses the returned `IDirectSound*` any more) in favour of a plain "audio available" bool from
miniaudio's `ma_context` init — that removes `dxguid.lib`.

**J. File-system browsing in the ImGui dialogs.** `tadsfiledlg.cpp` and `tadsfolderdlg.cpp` use
`FindFirstFileA`/`FindNextFileA`/`GetFileAttributesA`/`GetFullPathNameA`/`PathMatchSpecA`. → `std::filesystem`
(C++17) plus a small glob matcher to replace `PathMatchSpecA`. Self-contained, two files, no interface
change, and it removes `Shlwapi.lib`. Also fix the path separator assumption in `CTadsFileDialog::open()`
(`strrchr('\\')`, §3.3). Related: `SetCurrentDirectory`/`GetFullPathName` in `htmlgui.cpp:14237`/`:16136` and
`GetCurrentDirectory` ×3 in `htmlpref.cpp`.

**K. Character encoding.** `MultiByteToWideChar`/`WideCharToMultiByte` with `CP_ACP`, 10 sites in
`htmlgui.cpp` — `measure_text()`, `draw_text()`, `get_max_chars_in_width()` and the clipboard paths all
convert the engine's local-codepage bytes to UTF-16/UTF-8 for ImGui. The consistent answer is to route this
through the TADS charmap layer the VM already loads (`charmap/cmaplib.t3r`, shipped next to the exe — §2)
rather than adding a second, parallel encoding assumption. Note the existing code already assumes
"wide-char count == source character count" in `get_max_chars_in_width()` (true for single-byte codepages,
false in general) — decide explicitly whether to keep that assumption.

**L. `CTadsApp`, keyboard, and accelerators.**

- `tadsapp.cpp`'s `event_loop(MSG*)`, `process_message()`, accelerator translation, modeless-dialog list and
  MDI handling are all dead in guit3 but drag `MSG`/`HACCEL` into the headers. Reduce `CTadsApp` to what is
  actually called: `get_openfile_dir`/`_path`, mouse capture, `get_font()`, statusline registration.
  `get_instance()` (71 call sites!) disappears entirely once resources are portable (§5.4/B) — it exists
  only to be passed to `LoadString`/`LoadCursor`/`LoadMenu`/`LoadImage`/`GetModuleFileName`.
- `tadskb.cpp`'s `VK_*` name table + `MapVirtualKey` back the Keyboard preferences page's key-name
  parsing/formatting. Re-map onto `GLFW_KEY_*`. The table is a plain array; the work is the key mapping.
- **Do real keyboard accelerators at the same time** (§3.1: menu shortcuts are still display-only and
  `CTadsAccelerator` is dead). Both need one canonical key enum, so doing them together avoids defining it
  twice.

**M. `guimain.cpp` startup/shutdown.** `CoInitialize`/`CoUninitialize` (only needed for the Web UI and
`tadsole.cpp`'s drag-and-drop — goes with O), `InitCommonControlsEx` (goes with the dead dialog files),
`LoadLibrary("RICHED32.DLL")` (**audit — nothing in guit3 appears to use a rich edit control any more**),
`GetModuleHandle`, `init_debug_console`/`close_debug_console`, and the `CreateFile`/`WriteFile` crash-dump
writer (`tadscrsh.txt`, ~line 292) → `osfile`/`stdio`.

**N. The four windows that don't render at all.** `CHtmlSys_aboutgamewin`, `CHtmlSys_abouttadswin`,
`CHtmlSys_creditswin` and `CHtmlSysWin_win32_Popup` (`htmlgui.cpp`, 18k–19k line range) compile but produce
nothing in guit3 (§3.4a). They are `CTadsWin`/`CTadsWinScroll` subclasses displaying HTML, so most of the
ImGui path already exists — **`CHtmlSys_dbglogwin` is the working precedent** for a secondary top-level
window rendered as an ImGui overlay, including its own menu bar and the coordinate-space trap (§2). Doing
this also retires the last live entry points in `tadsdlg.cpp` (`modal_dlg_pre`/`post`,
`set_filedlg_center_hook`, `LicenseDlg`/`DLG_LICENSE`), which is what lets §5.3 drop those files.

**O. Gate the Web UI.** `tadswebctl.*`, `guiwebui.h`, `tadscom.*`, `guinogch.cpp` and the `CoInitialize`
pair behind `TADS_WEBUI_ENABLED`, off by default (§4). `CTadsStatusline::get_handle()` exists solely to keep
`guiwebui.h` compiling (§3.2) and can go with it.

### 5.5 Suggested order

**M1 — shrink the surface (no new platform code, Windows build unchanged).** Drop the dead files (§5.3,
except the ones blocked on N); gate the Web UI (O); drop the DirectSound probe (I) and `HtmlHelp` (F); trim
`CTadsApp` (L, the dead half). *Fewer files, fewer link libs, smaller type shim.*

**M2 — build the seam.** `tadsplat.h` (A1) and the `os_*` hook headers with Windows-only backends (A2).
*Still Windows-only, but every remaining Win32 call sits behind a named, single-purpose hook.*

**M3 — fill in portable implementations, cheapest-and-most-certain first.** GLFW-provided services (D) →
`std::filesystem` dialogs (J) → system colors (E) and shell (F) → settings store (C) → resources (B) →
fonts (G) → images (H) → audio file I/O (I) → charset (K) → keyboard/accelerators (L).
Each is landable and testable on Windows alone.

**M4 — flip the three gates (§5.1) and get a Linux build.** Expect a long tail in `htmlgui.cpp`/`tadswin.cpp`
that no census can predict; that's the point of doing M1–M3 first, so what the compiler finds is a
manageable remainder rather than thousands of errors. Add a Linux **compile-only** CI job at this point — it
is the cheapest possible regression net for a GUI app with no automated test coverage. Per the standing
decision, CI workflows for htmltads-specific artifacts belong in **this** repo, not `tads-runner`.

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
- **Prefer synthetic mouse clicks over synthetic keyboard input.** `SetCursorPos` + `mouse_event` at a
  coordinate read off an actual screenshot worked every time. Keyboard was a repeated source of trouble: a
  `WM_KEYDOWN`/`WM_KEYUP` pair with a placeholder `lParam` (missing the real scan-code/repeat-count bits) was
  read as a *stuck key* and submitted dozens of blank commands over several minutes before it was caught —
  genuinely disruptive if the user is watching. `WM_CHAR`-only input is needed for `do_char()`'s Enter
  handling at the command prompt but didn't reliably reach the title screen's wait-for-keystroke state.
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
