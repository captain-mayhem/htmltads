if (WIN32)
	add_subdirectory(win32/babel)
	add_subdirectory(win32)
endif()

set(base_src
	tadshtml.cpp
    tadsrtyp.cpp
    htmlprs.cpp
    htmltags.cpp
    htmlfmt.cpp
    htmldisp.cpp
    htmlsys.cpp
    htmltxar.cpp
    htmlinp.cpp
    htmljpeg.cpp
    htmlrc.cpp
    htmlrf.cpp
    htmlhash.cpp
    htmlplst.cpp
    oshtml.cpp
    htmlpng.cpp
	pngext.c
    htmlmng.cpp
    htmlsnd.cpp
)

if (WIN32)
	set(base_src
		${base_src}
		win32/htmlw32.cpp
		win32/w32main.cpp
		win32/w32font.cpp
		win32/w32font.h
		win32/w32snd.cpp
		win32/w32img.cpp
		win32/hos_w32.cpp
		win32/tadswin.cpp
		win32/tadsapp.cpp
		win32/tadskb.cpp
		win32/tadsfont.cpp
		win32/tadscar.cpp
		win32/tadsstat.cpp
		win32/htmlpref.cpp
		win32/tadsdlg.cpp
		win32/tadsdlg2.cpp
		win32/tadscbtn.cpp
		win32/tadsreg.cpp
		win32/tadsjpeg.cpp
		win32/tadsimg.cpp
		win32/tadspng.cpp
		win32/tadsmng.cpp
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
		win32/w32tr.cpp
		${mpeg_src}
	)

	set(trt3_src
		${tr3_base_src}
		win32/w32nogch.cpp
		win32/w32trt3.cpp
		win32/t3main.cpp
		win32/w32t3.cpp
	)
endif()

if (EMSCRIPTEN)
	set(base_src
		${base_src}
		emscripten/htmlemscripten.cpp
		emscripten/emfont.cpp
		emscripten/emfont.h
		emscripten/emmain.cpp
		emscripten/hos_emscripten.cpp
	)
	set(trt3_src
		emscripten/t3main.cpp
	)
endif()

set(trt3_src
	${base_src}
	${trt3_src}
    tadshtml3.cpp
)

if (WIN32)
	set(htmlt3_target_name htmlt3_tmp)
else()
	set(htmlt3_target_name htmlt3)
endif()

add_executable(${htmlt3_target_name}
    WIN32
	${trt3_src}
    win32/htmlt3.rc
)

target_include_directories(${htmlt3_target_name} PRIVATE
.
)

target_link_libraries(${htmlt3_target_name} PRIVATE
	Tads::t3htm
    Tads::tr32h
	
	PNG::PNG
	JPEG::JPEG
	MNG::MNG
)

if (WIN32)
	target_include_directories(${htmlt3_target_name} PRIVATE
		win32
		win32/mpegamp
	)

	target_link_libraries(${htmlt3_target_name} PRIVATE
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
endif()

if (EMSCRIPTEN)
	target_include_directories(${htmlt3_target_name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/emscripten
	)
	
	target_compile_options(
		${htmlt3_target_name} PRIVATE
		-pthread
	)
	
	target_link_libraries(${htmlt3_target_name} PRIVATE
		pthread
	)
	
	#-sPROXY_TO_PTHREAD,  -s EXPORTED_RUNTIME_METHODS=ccall -sEXPORTED_FUNCTIONS=_real_socket,_main -sPROXY_POSIX_SOCKETS 
	set_target_properties(${htmlt3_target_name} PROPERTIES LINK_FLAGS "-s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=_emscripten_receive_on_main_thread_js -s PROXY_TO_PTHREAD -s FORCE_FILESYSTEM -s INITIAL_MEMORY=256MB -s SAFE_HEAP=1 -lwebsocket.js -gsource-map -pthread -sPROXY_POSIX_SOCKETS -Wl,--wrap=fgets,--wrap=fflush,--wrap=getchar,--wrap=socket")
	
	install(TARGETS ${htmlt3_target_name}
		DESTINATION .
	)
	
	install (FILES
		"$<TARGET_FILE_DIR:htmlt3>/htmlt3.wasm"
		"$<TARGET_FILE_DIR:htmlt3>/htmlt3.wasm.map"
		${CMAKE_CURRENT_SOURCE_DIR}/emscripten/htmlt3.html
		DESTINATION .)
endif()

#we only build the runtime for non-windows targets
if(NOT WIN32)
	return()
endif()

make_t3r(htmlt3 win32/about3.jpg=about.jpg)

make_trx(htmlt3 ${htmlt3_target_name} ${CMAKE_CURRENT_BINARY_DIR}/htmlt3.t3r)

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

#tadsweb
add_executable(tadsweb
	WIN32
	win32/w32webui.cpp
	win32/tadswebctl.cpp
	win32/tadscom.cpp
	win32/tadswin.cpp
	win32/tadsapp.cpp
	win32/htmlpref.cpp
	htmlplst.cpp
	win32/tadsdlg.cpp
	win32/tadsdlg2.cpp
	win32/tadscbtn.cpp
	win32/foldsel2.cpp
	win32/tadsreg.cpp
	win32/tadsstat.cpp
	win32/tadsfont.cpp
	win32/tadskb.cpp
	htmlhash.cpp
	win32/w32nogch.cpp
	tadshtml.cpp
	win32/tadshtmldlg.cpp
	tadshtml3.cpp
	win32/tadsweb.rc
)

target_include_directories(tadsweb PRIVATE
.
win32
)

target_link_libraries(tadsweb PRIVATE
	Tads::t3htm
    Tads::tr32h
	PNG::PNG

    Comctl32.lib
    Shlwapi.lib
    Mpr.lib
    Urlmon.lib
	Ws2_32.lib
)

###install rules

install(TARGETS tadsweb
	DESTINATION .
)

install(PROGRAMS
	${htmlt3_file} ${htmltdb3_file}
	DESTINATION .
)

install(FILES
    win32/htmladdin.cpp
    win32/htmladdin.h
    win32/htmladdin.rc
    win32/itadsworkbench.h
    win32/itadsworkbenchiid.cpp
    win32/make-addins.vc
    win32/tads3addin.cpp
    win32/tads3addin.h
    win32/tads3addin.rc
    win32/wb_addin.h
    DESTINATION addins
)

install(FILES
	notes3/authkit/license.txt
	DESTINATION .
)

install(DIRECTORY
	win32/mkwebfiles
	DESTINATION .
)