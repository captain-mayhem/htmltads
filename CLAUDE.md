# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

The legacy HTML TADS sources (`htmlt3`/`htmltdb3`/`tadsweb`) plus their third-party dependencies (`zlib`, `libpng`, `jpeg`, `libmng`, `libogg`, `libvorbis`, `scintilla`, `textindex`, `wbaddons`, `t3doc`). The cross-platform Dear ImGui/GLFW `guit3` client and its guit3-only dependencies (`imgui`, `glfw`, `freetype`, `miniaudio`) no longer live here — they are in the sibling `guitads` repo. Unlike the sibling [tads-runner](https://github.com/captain-mayhem/tads-runner) repo (GPL-2.0), the `htmltads/` subfolder ships under a restrictive license that only permits porting TADS to new platforms/compilers/build systems without changing its feature set — see `README.md` for the exact license text. Keep that constraint in mind: this is a porting effort, not a feature-development one, for the legacy `htmltads/htmltads/win32/` code.

This repo is **not built standalone**. It is a sibling checkout to `tads-runner` (both cloned into the same parent directory); `tads-runner/CMakeLists.txt` auto-detects `../htmltads` and pulls it in via `add_subdirectory(../htmltads htmltads)` when `WITH_HTMLTADS` is enabled (Windows or Emscripten only). Always configure/build from `tads-runner`, not from here — see that repo's `CLAUDE.md` and its `README.md`'s "How to build" section for the full instructions (Visual Studio 2022/2026, CMake >= 3.19, `cmake -DCMAKE_INSTALL_PREFIX=install ..\tads-runner` from a sibling build directory).

## Targets in this repo

- `htmlt3` / `htmltdb3` (via `htmltdb3_tmp`) — the original Win32 runtime and debugger builds, defined in `htmltads/htmltads/CMakeLists.txt`; Windows-only, still fully native. `htmlt3` also has an Emscripten build (`htmltads/emscripten/`).
- `tadsweb` — a separate Win32 ActiveX web-UI helper executable, also untouched.
