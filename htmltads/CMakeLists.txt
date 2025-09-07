add_executable(htmlt3_tmp
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
)

target_include_directories(htmlt3_tmp PRIVATE
.
win32
win32/mpegamp
)

target_link_libraries(htmlt3_tmp PRIVATE
	t3htm
	PNG::PNG
	MNG::MNG

	Htmlhelp.lib
	Comctl32.lib
)
