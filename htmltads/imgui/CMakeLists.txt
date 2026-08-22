add_subdirectory(imgui)

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
	../win32/mpegamp/mpegamp_w32.cpp
	
	htmlgui.cpp
	w32main.cpp
	w32font.cpp
	w32font.h
	w32snd.cpp
	w32img.cpp
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
	tadsvorb.cpp
	tadsmidi.cpp
	tadswav.cpp
	tadsole.cpp
	oem_w32.c
	foldsel2.cpp
	w32fndlg.cpp
	iconmenu.cpp
	tadswebctl.cpp
	tadscom.cpp
	
	tadsapp.h
	
	hos_gui.h
	
	w32tr.cpp
	w32nogch.cpp
	w32trt3.cpp
	t3main.cpp
	w32t3.cpp

	imgui/imconfig.h
	imgui/imgui.cpp
	imgui/imgui.h
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_impl_glfw.cpp
	imgui/imgui_impl_glfw.h
	imgui/imgui_impl_opengl3.cpp
	imgui/imgui_impl_opengl3.h
	imgui/imgui_impl_opengl3_loader.h
	imgui/imgui_internal.h
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/imstb_textedit.h
	imgui/imstb_rectpack.h
	imgui/imstb_truetype.h

	imgui/misc/freetype/imgui_freetype.cpp
	imgui/misc/freetype/imgui_freetype.h

	../win32/htmlt3.rc
)

target_compile_definitions(guit3 PRIVATE
  IMGUI
)

target_include_directories(guit3 PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/imgui
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
)

target_link_libraries(guit3 PRIVATE
	Vorbis::vorbisfile

	Htmlhelp.lib
	Comctl32.lib
	Winmm.lib
	Ws2_32.lib
	Wininet.lib
	Mpr.lib
	Version.lib
	Shlwapi.lib
	dxguid.lib
)

target_link_libraries(guit3 PRIVATE 
	glfw 
	freetype
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

tads_source_group(guit3 ${HTMLTADS_DIR})