# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

The HTML TADS sources (`htmlt3`/`guit3`/`htmltdb3`) plus their third-party dependencies (`zlib`, `libpng`, `jpeg`, `libmng`, `libogg`, `libvorbis`, `freetype`, `glfw`, `scintilla`, `textindex`, `wbaddons`, `t3doc`). Unlike the sibling [tads-runner](https://github.com/captain-mayhem/tads-runner) repo (GPL-2.0), the `htmltads/` subfolder ships under a restrictive license that only permits porting TADS to new platforms/compilers/build systems without changing its feature set — see `README.md` for the exact license text. Keep that constraint in mind: this is a porting effort, not a feature-development one, for the legacy `htmltads/htmltads/win32/` code.

This repo is **not built standalone**. It is a sibling checkout to `tads-runner` (both cloned into the same parent directory); `tads-runner/CMakeLists.txt` auto-detects `../htmltads` and pulls it in via `add_subdirectory(../htmltads htmltads)` when `WITH_HTMLTADS` is enabled (Windows or Emscripten only). Always configure/build from `tads-runner`, not from here — see that repo's `CLAUDE.md` and its `README.md`'s "How to build" section for the full instructions (Visual Studio 2022/2026, CMake >= 3.19, `cmake -DCMAKE_INSTALL_PREFIX=install ..\tads-runner` from a sibling build directory).

## Active task: the guit3 migration

**The current, actively-worked task in this repo is the `guit3` migration** — porting the legacy Win32 `htmlt3` GUI client to a cross-platform Dear ImGui + GLFW + OpenGL3 client. The full plan, status, subsystem-by-subsystem inventory, and working notes for a fresh session live in **[htmltads/imgui/migration.md](htmltads/imgui/migration.md)** — read that file before starting any work on `guit3`. It is a living document: update it (status, gotchas, decisions) as you make progress, not just the roadmap checkboxes, per the working-notes style already established in its §6.

Quick orientation (see `migration.md` for the authoritative, detailed version):
- `htmltads/win32/` — the original Win32 `htmlt3` app, kept untouched as reference.
- `htmltads/imgui/` — the `guit3` rewrite in progress; started as a copy of `win32/` sources and is being converted file-by-file.
- `htmltads/imgui/imgui/` — vendored Dear ImGui + GLFW/OpenGL3 backends (third-party, not app code).
- `htmltads/emscripten/` — a separate, older, parallel web-port effort; not part of `guit3` and out of scope for the migration plan.
- The `guit3` target (`htmltads/imgui/CMakeLists.txt`) is currently gated to Windows-only (`if (NOT WIN32) return()`); removing that gate incrementally is the end goal.

## Building just guit3

Once `tads-runner` has been configured (see that repo), iterate on `guit3` alone without rebuilding the whole superproject:
```
cmake --build tads-runner/build/default --target guit3
```
Output binary: `tads-runner/build/default/htmltads/htmltads/imgui/guit3.exe`.

## Verifying guit3 changes

`guit3` is a GUI app with no automated/headless test coverage — changes must be verified by actually running it and looking (or scripting a screenshot). `migration.md` §6 documents a working PowerShell recipe (launch with a test game from `tads-runner/tests/`, wait for the window, find it via `EnumWindows`/class `GLFW30`, screenshot with `GetWindowRect`, sample pixels to confirm exact colors) — reuse that rather than re-deriving it.

## Other targets in this repo

- `htmlt3` / `htmltdb3` (via `htmltdb3_tmp`) — the original Win32 runtime and debugger builds, defined in `htmltads/htmltads/CMakeLists.txt`; Windows-only, still fully native, not part of the migration.
- `tadsweb` — a separate Win32 ActiveX web-UI helper executable, also untouched.
- These are unaffected by `guit3` work and should keep building as before; avoid changes here unless a task specifically targets them.
