# Embedded Web UI (IE ActiveX browser control embedding + Game Chest) support.
# Phase two (see migration.md section 4 and 5.4/O): off by default, so it just
# compiles out cleanly rather than being ported or deleted.  tadswebctl.cpp/
# tadscom.cpp implement it and are excluded from the build when this is off;
# guinogch.cpp supplies the no-op Game Chest stubs used in that case instead.
option(TADS_WEBUI_ENABLED "Enable guit3's embedded Web UI / Game Chest support (Windows-only, unimplemented off Windows)" OFF)

add_executable(guit3
	../tadshtml.cpp
	../tadshtml.h
	../tadsrtyp.cpp
	../htmlprs.cpp
	../htmlprs.h
	../htmltags.cpp
	../htmltags.h
	../htmlfmt.cpp
	../htmlfmt.h
	../htmldisp.cpp
	../htmldisp.h
	../htmlsys.cpp
	../htmlsys.h
	../htmltxar.cpp
	../htmltxar.h
	../htmlinp.cpp
	../htmlinp.h
	../htmljpeg.cpp
	../htmljpeg.h
	../htmlrc.cpp
	../htmlrc.h
	../htmlrf.cpp
	../htmlrf.h
	../htmlhash.cpp
	../htmlhash.h
	../htmlplst.cpp
	../htmlplst.h
	../oshtml.cpp
	../htmlpng.cpp
	../htmlpng.h
	../pngext.c
	../pngext.h
	../htmlmng.cpp
	../htmlmng.h
	../htmlsnd.cpp
	../htmlsnd.h

	../tadshtml3.cpp
	
	../mpegamp/misc2.cpp
	../mpegamp/layer3.cpp
	../mpegamp/layer2.cpp
	../mpegamp/getdata.cpp
	../mpegamp/huffman.cpp
	../mpegamp/transform.cpp
	# guit3-local forks of the amp MPEG driver.  mpegamp_w32.cpp/.h drop the
	# DirectSound/HWND ctor params (miniaudio output path, migration.md 3.7);
	# getbits.cpp routes its two file-I/O functions through the portable TADS
	# osfile API instead of ReadFile/SetFilePointer (migration.md 5.4/I).
	# The rest of ../mpegamp/* is platform-independent and shared with htmlt3;
	# these three Win32-specific files stay forked in win32/mpegamp/ instead.
	getbits.cpp
	mpegamp_w32.cpp
	mpegamp_w32.h

	# miniaudio - single-file cross-platform audio output backend, compiled
	# straight into guit3 (its only consumer).  See migration.md 3.7.
	../../miniaudio/miniaudio.c
	../../miniaudio/miniaudio.h

	htmlgui.cpp
	htmlgui.h
	guimain.cpp
	guimain.h

	# CTadsFont/CHtmlSysFont_win32 (portable - FreeType/ImGui only) + the
	# os_font_family_is_present()/os_font_data_for_name() platform hooks
	# tadsfont.h declares, one backend per platform selected below (Win32:
	# guifont_w32.cpp; Linux: fcfont.cpp/fontconfig; macOS: ctfont.cpp/
	# CoreText).  See migration.md 5.4/G.
	guifont.cpp
	guifont.h

	# guit3 OS-service platform hooks (shell, system colors, clipboard,
	# cursors, tick clock) - neutral header + hooks that are the same code
	# everywhere (guios_common.cpp) + one per-platform backend selected
	# below.  Same shape as guifont.cpp's os_font_family_is_present().  See
	# migration.md 5.4/D-F and 5.5.
	guios.h
	guios_common.cpp

	# Persistent settings store (app options, per-theme "profile" trees,
	# per-game theme associations) - neutral interface + one backend per
	# platform, replacing CTadsRegistry / the inline Reg* calls.  The Win32
	# backend is that registry code lifted verbatim; the non-Windows backend
	# is an INI-style file under $XDG_CONFIG_HOME / ~/Library/Preferences.
	# See migration.md 5.4/C and 5.5 (M2/A2).
	tadssettings.h
	guisnd.cpp
	guisnd.h
	guiimg.cpp
	guiimg.h
	hos_gui.cpp
	tadswin.cpp
	tadswin.h
	tadsapp.cpp
	tadskb.cpp
	tadskb.h
	tadsfont.cpp
	tadsfont.h
	tadscar.cpp
	tadscar.h
	tadsstat.cpp
	tadsstat.h
	htmlpref.cpp
	htmlpref.h
	tadsdlg.cpp
	tadsdlg.h
	tadsfiledlg.h
	tadsfiledlg.cpp
	tadsfinddlg.h
	tadsfinddlg.cpp
	tadslicensedlg.h
	tadslicensedlg.cpp
	tadsfolderdlg.h
	tadsfolderdlg.cpp
	tadsjpeg.cpp
	tadsjpeg.h
	tadsimg.cpp
	tadsimg.h
	tadspng.cpp
	tadspng.h
	tadsmng.cpp
	tadsmng.h
	tadssnd.cpp
	tadssnd.h
	tadscsnd.cpp
	tadscsnd.h
	tadsaudiodev.cpp
	tadsaudiodev.h
	tadsvorb.cpp
	tadsvorb.h
	tadsmidi.cpp
	tadsmidi.h
	tadswav.cpp
	tadswav.h
	oem_w32.c

	tadsapp.h
	tadsplat.h

	../hos_gui.h

	guitr.cpp
	guitrt3.cpp
	t3main.cpp
	t3main.h
	guit3.cpp
)

if (WIN32)
	# no resource compiler off-Windows - see migration.md 5.1/5.4/B.
	target_sources(guit3 PRIVATE ../win32/htmlt3.rc)

	# tadsdlg2.cpp is entirely dead code off-Windows (native property-sheet
	# WM_NOTIFY handlers - migration.md 5.3): unlike tadsdlg.cpp, which still
	# has two live entry points, nothing in it is ever called from guit3 once
	# the ImGui dialogs replaced the property sheets. Keep it Windows-only
	# rather than porting Win32 API calls that would never run.
	target_sources(guit3 PRIVATE tadsdlg2.cpp)

	# tadsole.cpp implements CTadsDataObjText, the real COM IDataObject
	# backing OLE drag-and-drop *source* support - Windows-only until phase
	# two (migration.md 5.3); get_drag_dataobj() (htmlgui.cpp) just returns
	# null off Windows instead.
	target_sources(guit3 PRIVATE tadsole.cpp tadsole.h)
endif()

if (TADS_WEBUI_ENABLED)
	target_sources(guit3 PRIVATE
		tadswebctl.cpp
		tadswebctl.h
		tadscom.cpp
		tadscom.h
	)
	target_compile_definitions(guit3 PRIVATE TADS_WEBUI_ENABLED)

	if (WIN32)
		# Game Chest (favorites/library manager) - see migration.md 5.4/O.
		# Ported from win32/w32chest.cpp, and logically independent of the
		# embedded IE Web UI browser this flag was originally introduced
		# for (tadswebctl.cpp/tadscom.cpp above) - the two features just
		# happen to share this one build flag for now. Game Chest's own UI
		# renders as an HTML page (via the ordinary HTML formatter/parser)
		# plus a handful of native, self-pumping Win32 modal dialogs, not
		# ImGui - see tadschest.cpp's own header comment - so it stays
		# Windows-only, same as the browser half of this flag. Its dialog
		# resources (DLG_GAME_CHEST_*, DLG_DOWNLOAD) are already compiled
		# in via win32/htmlt3.rc's #include of win32/htmlcmn.rc above.
		#
		# foldsel2.cpp (the native folder-browse dialog used by the
		# find-and-add wizard's root-folder page) is reactivated here
		# rather than replaced with the ImGui CTadsFolderDialog, which has
		# no blocking entry point and so can't be called synchronously from
		# this dialog's WM_COMMAND handler the way
		# CTadsDialogFolderSel2::run_select_folder() can; left compiled but
		# unused everywhere else, same "harmless dead code" bucket as
		# tadsdlg.cpp/tadsdlg2.cpp. Copied into this directory (rather than
		# referenced as ../win32/foldsel2.cpp) rather than built from
		# win32/ in place, because its quote-included "tadsdlg.h" must
		# resolve to *this* directory's ImGui-era CTadsDialog - a source
		# file physically under win32/ would instead pick up win32/tadsdlg.h
		# (found first via same-directory quote-include resolution), an
		# incompatible class of the same name, and fail to link.
		target_sources(guit3 PRIVATE
			tadschest.cpp
			foldsel2.cpp
			foldsel.h
			foldselr.h
		)
	else()
		# no real (Windows-only) Game Chest port off Windows yet - fall
		# back to the same no-op stubs the flag's OFF branch below uses
		target_sources(guit3 PRIVATE guinogch.cpp)
	endif()
else()
	# no-op Game Chest stubs, used whenever the real (Web UI-dependent)
	# implementation isn't built in - see migration.md 5.4/O
	target_sources(guit3 PRIVATE guinogch.cpp)
endif()

# guios.h per-platform backend: the Win32 code lifted verbatim, or the
# portable (GLFW / POSIX) implementation.  guios_common.cpp (above) carries
# the hooks that are identical on both.  See migration.md 5.4/D-F.
if (WIN32)
	target_sources(guit3 PRIVATE guios_w32.cpp)
else()
	target_sources(guit3 PRIVATE
		guios_portable.cpp
		# item B's embedded runtbar.bmp / license.txt byte arrays, generated
		# with `xxd -i` - see migration.md 5.4/B.
		guires_data.h
		guires_data.cpp
	)
endif()

# tadssettings.h per-platform backend: the registry code lifted verbatim, or
# the portable INI-file implementation.  See migration.md 5.4/C.
if (WIN32)
	target_sources(guit3 PRIVATE tadssettings_w32.cpp)
else()
	target_sources(guit3 PRIVATE tadssettings_portable.cpp)
endif()

# tadsfont.h's os_font_family_is_present()/os_font_data_for_name()/
# os_enum_font_families() backend: GDI, fontconfig, CoreText, or
# (Emscripten) a small bundled font set - there is no real system font store
# to enumerate inside a browser sandbox, so emfont.cpp instead scans a
# handful of font files (the standard Dear ImGui sample fonts - see
# ../../imgui/misc/fonts/README.txt) packaged into the Emscripten build's
# virtual filesystem below, at the "fonts" virtual path emfont.cpp reads
# from. CTadsFont falls back to the ImGui atlas default font for any name
# that isn't among them, same as it would for a real system with no
# matching font installed.
if (WIN32)
	target_sources(guit3 PRIVATE guifont_w32.cpp)
elseif (APPLE)
	target_sources(guit3 PRIVATE ctfont.cpp)
	target_link_libraries(guit3 PRIVATE
		"-framework CoreText"
		"-framework CoreFoundation"
	)
elseif (EMSCRIPTEN)
	target_sources(guit3 PRIVATE emfont.cpp)
else()
	find_package(Fontconfig REQUIRED)
	target_sources(guit3 PRIVATE fcfont.cpp)
	target_link_libraries(guit3 PRIVATE Fontconfig::Fontconfig)
endif()

target_compile_definitions(guit3 PRIVATE
  IMGUI
)

target_include_directories(guit3 PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(guit3 PRIVATE
	Tads::t3htm
    Tads::tr32h
	
	PNG::PNG
	JPEG::JPEG
	MNG::MNG
)

target_include_directories(guit3 PRIVATE
	.
	../mpegamp
	../../miniaudio
)

target_link_libraries(guit3 PRIVATE
	Vorbis::vorbisfile
)

if (WIN32)
	target_link_libraries(guit3 PRIVATE
		Comctl32.lib
		# Winmm.lib is still needed for tadsmidi.cpp's midiStream* MIDI sequencer,
		# which is gated to _WIN32 until a portable software synth replaces it.
		Winmm.lib
		Ws2_32.lib
		Wininet.lib
		Mpr.lib
		Version.lib
	)
endif()

target_link_libraries(guit3 PRIVATE
	imgui
)

if (EMSCRIPTEN)
	# GLFW and OpenGL come from the Emscripten toolchain itself (see the
	# link flags below), not from the vendored glfw target or a system
	# OpenGL - there is neither under this platform.
else()
	target_link_libraries(guit3 PRIVATE glfw)

	if (WIN32)
		target_link_libraries(guit3 PRIVATE Opengl32.lib)
	else()
		find_package(OpenGL REQUIRED)
		target_link_libraries(guit3 PRIVATE OpenGL::GL)
	endif()
endif()

if (EMSCRIPTEN)
	target_compile_options(guit3 PRIVATE -pthread)

	# Link flags verified to compile and link as of migration.md 5.7; the
	# -sPROXY_TO_PTHREAD/-sPROXY_POSIX_SOCKETS pairing (needed - and correct
	# - for htmlt3's blocking-socket networking) was dropped again in 5.9
	# once real browser testing (via the guit3.html added in 5.8) showed it
	# runs guit3's main() - and therefore glfwInit() - on a Web Worker,
	# where `window`/`document` don't exist; Emscripten's GLFW3 emulation
	# calls window.addEventListener() straight out of glfwInit(), so this
	# threw "ReferenceError: window is not defined" on every load. guit3
	# doesn't need main() proxied to a pthread (it has no equivalent
	# blocking-socket use on the main thread to work around), so it just
	# runs on the browser's real main thread instead, same as any other
	# GLFW/Emscripten app. -sUSE_GLFW=3/-sFULL_ES3=1/-sUSE_WEBGL2=1 select
	# Emscripten's own GLFW-API-over-HTML5 and WebGL2 emulation in place of
	# the vendored glfw/desktop-GL this target uses elsewhere. -pthread and
	# the fgets/fflush/getchar --wrap flags are kept even though guit3 itself
	# doesn't call the wrapped functions, since it links the same t3htm
	# static lib (compiled with those wrap targets in mind) htmlt3 does.
	set_target_properties(guit3 PROPERTIES LINK_FLAGS "-s ASSERTIONS=1 -s WASM=1 -s USE_GLFW=3 -s FULL_ES3=1 -s USE_WEBGL2=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=_emscripten_receive_on_main_thread_js -s FORCE_FILESYSTEM -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=256MB -s SAFE_HEAP=1 -lwebsocket.js -pthread -Wl,--wrap=fgets,--wrap=fflush,--wrap=getchar")

	# Manual browser test page for guit3-under-Emscripten (migration.md 5.7
	# left runtime behavior completely unverified). Wired up the same way
	# tads3/CMakeLists.txt does it for t3run (copy the hand-written .html
	# next to the build output, since we don't want emcc's own generated
	# shell - see that file's SUFFIX ".html" comment) - but with a plain
	# canvas shell instead of t3run's xterm.js terminal, since guit3 is a
	# graphical ImGui app, not a console one.
	add_custom_command(
		TARGET guit3 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/emscripten/guit3.html $<TARGET_FILE_DIR:guit3>)

	# Packages tests/ditch3.t3 (the same TADS3 test game tests/CMakeLists.txt
	# packages for the classic t3run web build) for guit3's own Emscripten
	# virtual filesystem. Named distinctly from that other em_package(ditch3
	# ...) call - em_package()'s add_custom_target() name is derived from
	# this argument, and CMake target names must be unique project-wide, not
	# just per-directory - the packaged path (ditch3.t3, after the "@") is
	# what guit3.html and guit3's own argv actually reference.
	em_package(ditch3game ${CMAKE_SOURCE_DIR}/tests/ditch3.t3@ditch3.t3)

	# Packages the standard Dear ImGui sample fonts (../../imgui/misc/fonts -
	# see that directory's README.txt for the list and licenses) into their
	# own virtual filesystem entry, mapped to the "fonts" virtual directory
	# emfont.cpp's os_font_family_is_present()/os_font_data_for_name()/
	# os_enum_font_families() scan at runtime (see the font-backend selection
	# above and emfont.cpp itself). A separate em_package() call, same
	# reasoning as ditch3game above - distinct target name, own .data/.js
	# pair guit3.html loads alongside ditch3game's.
	em_package(guit3fonts ${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/misc/fonts@fonts)

	# Unlike ditch3game above (a test-only asset nobody minds having to
	# build separately), the font package is load-bearing for guit3 itself
	# (emfont.cpp has nothing to enumerate without it) - em_package()'s
	# custom target is only added to the default "all" target, not to
	# guit3's own dependency graph, so make that explicit.
	add_dependencies(guit3 build_guit3fonts.data)

	# Packages the TADS charmap library (tads3/charmap/cmaplib.t3r) into the
	# virtual path "charmap/cmaplib.t3r" - the exact relative path
	# CResLoader::open_res_file() (resload.cpp, via
	# CCharmap::open_map_file()'s deflib argument "charmap/cmaplib")
	# expects to find it at when it falls through from the loose-.tcm-file
	# lookup, same as the native builds' POST_BUILD copy_if_different step
	# below puts it on disk for. Without this, guios_portable.cpp's
	# get_to_uni()/get_to_local() (item K) never find a table to load inside
	# Emscripten's virtual filesystem - there's no on-disk directory a
	# browser build can just read from the way a native build reads its
	# CWD - and silently fall back to CCharmapToUniASCII/ToLocalASCII, which
	# map every non-ASCII byte to U+FFFD instead of the real character.
	# Load-bearing the same way guit3fonts is above: real text depends on
	# it, so it's a real dependency of guit3, not just the default "all"
	# target em_package() adds it to.
	em_package(guit3charmap ${CMAKE_SOURCE_DIR}/tads3/charmap/cmaplib.t3r@charmap/cmaplib.t3r)
	add_dependencies(guit3 build_guit3charmap.data)

	install(FILES
		"$<TARGET_FILE_DIR:guit3>/guit3.wasm"
		${CMAKE_CURRENT_SOURCE_DIR}/emscripten/guit3.html
		DESTINATION .
		COMPONENT GuiT3
	)
endif()

if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
	# guit3 statically links its own copies of libpng/zlib/libjpeg/libmng
	# (see ../../CMakeLists.txt), which by default export symbols like
	# "png_free" into the executable's dynamic symbol table alongside its
	# own code. GLFW's Wayland backend loads window-decoration plugins via
	# libdecor, which (on glibc, per a known libdecor/dlmopen limitation)
	# sanity-checks a handful of well-known symbols shared with its GTK
	# plugin - if dlsym(RTLD_DEFAULT, "png_free") resolves to guit3's own
	# statically-linked libpng instead of the system one the GTK plugin
	# itself links against, libdecor refuses to load that plugin ("Plugin
	# "GTK3 plugin" uses conflicting symbol "png_free"") and silently
	# falls back to plainer decorations. --exclude-libs keeps every
	# symbol pulled in from a static archive out of guit3's dynamic
	# symbol table, without having to touch each vendored library's own
	# build.
	target_link_options(guit3 PRIVATE "-Wl,--exclude-libs,ALL")
endif()

# copy the charmap library next to the built exe, same as t3run's CMakeLists.txt does -
# without this, guit3 can't find a mapping file for the local character set and shows
# a startup warning about it on every run.
add_custom_command(
		TARGET guit3 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:guit3>/charmap
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/tads3/charmap/cmaplib.t3r $<TARGET_FILE_DIR:guit3>/charmap/)

# Private HTML resources (the "About" box background image, etc.) - same
# source image and .t3r layout as htmlt3's make_t3r() call, but unlike
# htmlt3 we deliberately do NOT follow this up with make_trx() to bind the
# .t3r into the .exe via maketrx32: guit3.t3r is instead left as a loose
# file next to guit3.exe, which CHtmlSys_mainwin::load_exe_resources()
# falls back to reading directly when it doesn't find resources embedded in
# the executable (see htmlgui.cpp).
make_t3r(guit3 ../win32/about3.jpg=about.jpg)
add_dependencies(guit3 build_guit3.t3r)

if (EMSCRIPTEN)
	# Packages the private-resources .t3r built just above (the About box's
	# background image) into the virtual path "guit3.t3r" - the exact
	# filename CHtmlSys_mainwin::load_exe_resources() (htmlgui.cpp) looks
	# for next to the "executable" (see guimain.cpp's exefile fix: off
	# Windows that name is now hardcoded to "guit3" instead of coming back
	# empty from the GetModuleFileName() stub, so the loose-.t3r lookup
	# resolves to "guit3.t3r" the same way it does on a native build reading
	# its own directory). Declared here rather than up with guit3fonts/
	# guit3charmap because build_guit3.t3r - the target em_package()'s
	# input file actually depends on - isn't defined until make_t3r() above
	# runs; em_package() itself has no way to express that dependency (it
	# never DEPENDS on its packaged input paths), so add_dependencies()
	# below does it explicitly rather than relying on script order alone.
	em_package(guit3about ${CMAKE_CURRENT_BINARY_DIR}/guit3.t3r@guit3.t3r)
	add_dependencies(build_guit3about.data build_guit3.t3r)
	add_dependencies(guit3 build_guit3about.data)
endif()

tads_source_group(guit3 ${HTMLTADS_DIR})

install(TARGETS guit3
	DESTINATION .
	COMPONENT GuiT3
)

install(FILES
	${CMAKE_SOURCE_DIR}/tads3/charmap/cmaplib.t3r
	DESTINATION charmap
	COMPONENT GuiT3
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/guit3.t3r
	DESTINATION .
	COMPONENT GuiT3
)
