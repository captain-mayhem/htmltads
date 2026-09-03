#for now, while porting, we only allow win32
if (NOT WIN32)
	return()
endif()

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
	../win32/mpegamp/getbits.cpp
	../win32/mpegamp/huffman.cpp
	../win32/mpegamp/transform.cpp
	# guit3-local fork of the amp MPEG driver: same as
	# ../win32/mpegamp/mpegamp_w32.cpp but with the DirectSound/HWND params
	# dropped from CMpegAmpW32's ctor (miniaudio output path).  See
	# migration.md 3.7.  The rest of win32/mpegamp/* is unchanged and shared.
	mpegamp_w32.cpp
	mpegamp_w32.h

	# miniaudio - single-file cross-platform audio output backend, compiled
	# straight into guit3 (its only consumer).  See migration.md 3.7.
	../../miniaudio/miniaudio.c

	htmlgui.cpp
	guimain.cpp
	guifont.cpp
	guifont.h
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
	tadsdlg2.cpp
	tadsfiledlg.h
	tadsfiledlg.cpp
	tadsfinddlg.h
	tadsfinddlg.cpp
	tadsfolderdlg.h
	tadsfolderdlg.cpp
	tadscbtn.cpp
	tadsreg.cpp
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
	tadsole.cpp
	oem_w32.c
	foldsel2.cpp
	guifndlg.cpp
	iconmenu.cpp
	tadswebctl.cpp
	tadscom.cpp
	
	tadsapp.h
	
	hos_gui.h
	
	guitr.cpp
	guinogch.cpp
	guitrt3.cpp
	t3main.cpp
	guit3.cpp

	../win32/htmlt3.rc
)

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

	Htmlhelp.lib
	Comctl32.lib
	# Winmm.lib is still needed for tadsmidi.cpp's midiStream* MIDI sequencer,
	# which is gated to _WIN32 until a portable software synth replaces it.
	Winmm.lib
	Ws2_32.lib
	Wininet.lib
	Mpr.lib
	Version.lib
	Shlwapi.lib
	# still referenced only by CHtmlSys_mainwin::get_directsound()'s vestigial
	# DirectSound-availability probe in htmlgui.cpp (IID_IDirectSoundNotify).
	# Audio output itself no longer uses DirectSound; that probe can be
	# removed in a follow-up, and this line with it.
	dxguid.lib
)

target_link_libraries(guit3 PRIVATE
	imgui
	glfw
)

if (WIN32)
	target_link_libraries(guit3 PRIVATE Opengl32.lib)
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
