add_subdirectory(win32)

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

make_t3r(htmlt3 win32/about3.jpg=about.jpg)

make_trx(htmlt3 htmlt3_tmp ${CMAKE_CURRENT_BINARY_DIR}/htmlt3.t3r)

trx_target_file(htmlt3 htmlt3_file)

### t3 debugger

set(xzip_src
    win32/xzip/XZip.cpp
)

set(tdb_base_src
	${base_src}
    ${mpeg_src}
    ${xzip_src}

    win32/w32nogch.cpp
	htmldcfg.cpp
	htmldbg.cpp
	win32/w32tdb.cpp
	win32/tinyxml.cpp
	win32/tadshtmldlg.cpp
	win32/w32help.cpp
	win32/itadsworkbenchiid.cpp
	win32/w32sci.cpp
	win32/tbarmbar.cpp
	win32/mditab.cpp
	win32/tadsifid.c
	win32/w32expr.cpp
	win32/w32bpdlg.cpp
	win32/w32dbgpr.cpp
	win32/tadstab.cpp
	win32/tadsdde.cpp
	win32/tadsdock.cpp
	win32/w32dbwiz.cpp
	win32/babelifc.cpp
)

set(tdbt3_src
    ${tdb_base_src}
    win32/w32tdbt3.cpp
    htmldb3.cpp
	htmldc3.cpp
    win32/t3main.cpp
    win32/w32prj.cpp
	win32/w32script.cpp
    win32/w32t3.cpp
    win32/w32dbgpr3.cpp
    tadshtml3.cpp
)

add_executable(htmltdb3_tmp
	WIN32
	${tdbt3_src}
    win32/htmltdb3.rc
)

target_include_directories(htmltdb3_tmp PRIVATE
.
win32
win32/mpegamp
win32/xzip
)

target_link_libraries(htmltdb3_tmp PRIVATE
	Tads::t3htm_d
    Tads::trd32h
    Tads::babel

	PNG::PNG
    JPEG::JPEG
	MNG::MNG
    Vorbis::vorbisfile
    Docsearch::docsearch
    Scintilla::SciInterface

	Htmlhelp.lib
	Comctl32.lib
    Winmm.lib
    Ws2_32.lib
    Wininet.lib
    Mpr.lib
    Version.lib
    Shlwapi.lib
    dxguid.lib
    Urlmon.lib
)

target_link_options(htmltdb3_tmp PRIVATE
	/MANIFEST:NO
)


make_t3r(htmltdb3 win32/about3.jpg=about.jpg win32/aboutWB3.jpg=aboutwb.jpg)

make_trx(htmltdb3 htmltdb3_tmp ${CMAKE_CURRENT_BINARY_DIR}/htmltdb3.t3r)

trx_target_file(htmltdb3 htmltdb3_file)

###install rules

install(PROGRAMS
	${htmlt3_file} ${htmltdb3_file}
	DESTINATION .
)