# Embedded Web UI (IE ActiveX browser control embedding + Game Chest) support.
# Phase two (see migration.md section 4 and 5.4/O): off by default, so it just
# compiles out cleanly rather than being ported or deleted.  tadswebctl.cpp/
# tadscom.cpp implement it and are excluded from the build when this is off;
# guinogch.cpp supplies the no-op Game Chest stubs used in that case instead.
option(TADS_WEBUI_ENABLED "Enable guit3's embedded Web UI / Game Chest support (Windows-only, unimplemented off Windows)" OFF)

add_executable(guit3
	../tadshtml.cpp
	../tadsrtyp.cpp
	../htmlprs.cpp
	../htmltags.cpp
	../htmlfmt.cpp
	../htmldisp.cpp
	../htmlsys.cpp
	../htmltxar.cpp
	../htmlinp.cpp
	../htmljpeg.cpp
	../htmlrc.cpp
	../htmlrf.cpp
	../htmlhash.cpp
	../htmlplst.cpp
	../oshtml.cpp
	../htmlpng.cpp
	../pngext.c
	../htmlmng.cpp
	../htmlsnd.cpp
	
	../tadshtml3.cpp
	
	../win32/mpegamp/misc2.cpp
	../win32/mpegamp/layer3.cpp
	../win32/mpegamp/layer2.cpp
	../win32/mpegamp/getdata.cpp
	../win32/mpegamp/huffman.cpp
	../win32/mpegamp/transform.cpp
	# guit3-local forks of the amp MPEG driver.  mpegamp_w32.cpp/.h drop the
	# DirectSound/HWND ctor params (miniaudio output path, migration.md 3.7);
	# getbits.cpp routes its two file-I/O functions through the portable TADS
	# osfile API instead of ReadFile/SetFilePointer (migration.md 5.4/I).
	# The rest of win32/mpegamp/* is unchanged and shared with htmlt3.
	getbits.cpp
	mpegamp_w32.cpp
	mpegamp_w32.h

	# miniaudio - single-file cross-platform audio output backend, compiled
	# straight into guit3 (its only consumer).  See migration.md 3.7.
	../../miniaudio/miniaudio.c

	htmlgui.cpp
	guimain.cpp

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
	guiimg.cpp
	hos_gui.cpp
	tadswin.cpp
	tadsapp.cpp
	tadskb.cpp
	tadsfont.cpp
	tadscar.cpp
	tadsstat.cpp
	htmlpref.cpp
	tadsdlg.cpp
	tadsfiledlg.h
	tadsfiledlg.cpp
	tadsfinddlg.h
	tadsfinddlg.cpp
	tadslicensedlg.h
	tadslicensedlg.cpp
	tadsfolderdlg.h
	tadsfolderdlg.cpp
	tadsjpeg.cpp
	tadsimg.cpp
	tadspng.cpp
	tadsmng.cpp
	tadssnd.cpp
	tadscsnd.cpp
	tadsaudiodev.cpp
	tadsaudiodev.h
	tadsvorb.cpp
	tadsmidi.cpp
	tadswav.cpp
	oem_w32.c

	tadsapp.h
	tadsplat.h

	hos_gui.h

	guitr.cpp
	guitrt3.cpp
	t3main.cpp
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
	target_sources(guit3 PRIVATE tadsole.cpp)
endif()

if (TADS_WEBUI_ENABLED)
	target_sources(guit3 PRIVATE
		tadswebctl.cpp
		tadscom.cpp
	)
	target_compile_definitions(guit3 PRIVATE TADS_WEBUI_ENABLED)
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

# tadsfont.h's os_font_family_is_present()/os_font_data_for_name() backend:
# GDI, fontconfig, or CoreText.  See migration.md 5.4/G.
if (WIN32)
	target_sources(guit3 PRIVATE guifont_w32.cpp)
elseif (APPLE)
	target_sources(guit3 PRIVATE ctfont.cpp)
	target_link_libraries(guit3 PRIVATE
		"-framework CoreText"
		"-framework CoreFoundation"
	)
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
	../win32/mpegamp
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
	glfw
)

if (WIN32)
	target_link_libraries(guit3 PRIVATE Opengl32.lib)
else()
	find_package(OpenGL REQUIRED)
	target_link_libraries(guit3 PRIVATE OpenGL::GL)
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

tads_source_group(guit3 ${HTMLTADS_DIR})

install(TARGETS guit3
	DESTINATION .
)

install(FILES
	${CMAKE_SOURCE_DIR}/tads3/charmap/cmaplib.t3r
	DESTINATION charmap
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/guit3.t3r
	DESTINATION .
)
