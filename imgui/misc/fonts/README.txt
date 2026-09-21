Sample fonts vendored from the upstream Dear ImGui project's own misc/fonts/
directory (https://github.com/ocornut/imgui), added here as the packaged
font set for guit3's Emscripten build - see htmltads/imgui/CMakeLists.txt's
em_package(guit3fonts ...) call and emfont.cpp. Not used by the native
Win32/Linux/macOS builds, which read fonts from the real OS font store
instead (guifont_w32.cpp/fcfont.cpp/ctfont.cpp).

Licenses, per Dear ImGui's own misc/fonts/README.txt:

  Cousine-Regular.ttf   by Steve Matteson, Apache License 2.0
  DroidSans.ttf         by Google, Apache License 2.0
  Karla-Regular.ttf     by Jonathan Pinhorn, SIL Open Font License 1.1
  ProggyClean.ttf       by Tristan Grimmer, public domain
                        (see http://www.proggyfonts.net/)
  ProggyTiny.ttf        by Tristan Grimmer, public domain
                        (see http://www.proggyfonts.net/)
  Roboto-Medium.ttf     by Google, Apache License 2.0
