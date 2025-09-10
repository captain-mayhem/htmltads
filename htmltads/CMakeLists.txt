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
	${trt3_src}
)

target_include_directories(htmlt3_tmp PRIVATE
.
win32
win32/mpegamp
)

target_link_libraries(htmlt3_tmp PRIVATE
	t3htm
	PNG::PNG
    JPEG::JPEG
	MNG::MNG
    Vorbis::vorbisfile

	Htmlhelp.lib
	Comctl32.lib
)
