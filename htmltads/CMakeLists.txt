set(base_src
	win32/htmlw32.cpp
	win32/w32main.cpp
	tadshtml.cpp
    tadsrtyp.cpp
    htmlprs.cpp
    htmltags.cpp
    htmlfmt.cpp
    htmldisp.cpp
    htmlsys.cpp
	win32/w32font.cpp
    win32/w32snd.cpp
    win32/w32img.cpp
    win32/hos_w32.cpp
	win32/tadswin.cpp
    win32/tadsapp.cpp
    win32/tadskb.cpp
    win32/tadsfont.cpp
	win32/tadscar.cpp
    htmltxar.cpp
    htmlinp.cpp
    htmljpeg.cpp
    htmlrc.cpp
    htmlrf.cpp
    htmlhash.cpp
    win32/tadsstat.cpp
    win32/htmlpref.cpp
    win32/tadsdlg.cpp
    win32/tadsdlg2.cpp
    win32/tadscbtn.cpp
    htmlplst.cpp
    win32/tadsreg.cpp
    oshtml.cpp
    win32/tadsjpeg.cpp
    htmlpng.cpp
    htmlmng.cpp
    pngext.c
    win32/tadsimg.cpp
    win32/tadspng.cpp
    win32/tadsmng.cpp
    htmlsnd.cpp
    win32/tadssnd.cpp
    win32/tadscsnd.cpp
    win32/tadsvorb.cpp
    win32/tadsmidi.cpp
    win32/tadswav.cpp
    win32/tadsole.cpp
    win32/oem_w32.c
    win32/foldsel2.cpp
    win32/w32fndlg.cpp
    win32/iconmenu.cpp
    win32/tadswebctl.cpp
    win32/tadscom.cpp
)

set(mpeg_src
    win32/mpegamp/misc2.cpp
    win32/mpegamp/layer3.cpp
    win32/mpegamp/layer2.cpp
    win32/mpegamp/getdata.cpp
    win32/mpegamp/getbits.cpp
    win32/mpegamp/huffman.cpp
    win32/mpegamp/transform.cpp
    win32/mpegamp/mpegamp_w32.cpp
)

set(tr3_base_src
	${base_src}
	win32/w32tr.cpp
	${mpeg_src}
)

set(trt3_src
	${tr3_base_src}
	win32/w32nogch.cpp
    win32/w32trt3.cpp
    win32/t3main.cpp
    win32/w32t3.cpp
    tadshtml3.cpp
)

add_executable(htmlt3_tmp
    WIN32
	${trt3_src}
    win32/htmlt3.rc
)

target_include_directories(htmlt3_tmp PRIVATE
.
win32
win32/mpegamp
)

target_link_libraries(htmlt3_tmp PRIVATE
	Tads::t3htm
    Tads::tr32h

	PNG::PNG
    JPEG::JPEG
	MNG::MNG
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

function(make_t3r output)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r
		COMMAND Tads::t3res ARGS -create ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r -add ${ARGN}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	add_custom_target(${output}.t3r DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r)
endfunction()

make_t3r(htmlt3 win32/about3.jpg=about.jpg)
