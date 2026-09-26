if (WIN32)
	add_subdirectory(win32/babel)
	add_subdirectory(win32)
endif()

set(base_src
	tadshtml.cpp
	tadshtml.h
    tadsrtyp.cpp
    htmlprs.cpp
    htmlprs.h
    htmltags.cpp
    htmltags.h
    htmlfmt.cpp
    htmlfmt.h
    htmldisp.cpp
    htmldisp.h
    htmlsys.cpp
    htmlsys.h
    htmltxar.cpp
    htmltxar.h
    htmlinp.cpp
    htmlinp.h
    htmljpeg.cpp
    htmljpeg.h
    htmlrc.cpp
    htmlrc.h
    htmlrf.cpp
    htmlrf.h
    htmlhash.cpp
    htmlhash.h
    htmlplst.cpp
    htmlplst.h
    oshtml.cpp
    htmlpng.cpp
    htmlpng.h
	pngext.c
	pngext.h
    htmlmng.cpp
    htmlmng.h
    htmlsnd.cpp
    htmlsnd.h
)

if (WIN32)
	set(base_src
		${base_src}
		win32/htmlw32.cpp
		win32/htmlw32.h
		win32/w32main.cpp
		win32/w32main.h
		win32/w32font.cpp
		win32/w32font.h
		win32/w32snd.cpp
		win32/w32snd.h
		win32/w32img.cpp
		win32/w32img.h
		win32/hos_w32.cpp
		win32/hos_w32.h
		win32/tadswin.cpp
		win32/tadswin.h
		win32/tadsapp.cpp
		win32/tadsapp.h
		win32/tadskb.cpp
		win32/tadskb.h
		win32/tadsfont.cpp
		win32/tadsfont.h
		win32/tadscar.cpp
		win32/tadscar.h
		win32/tadsstat.cpp
		win32/tadsstat.h
		win32/htmlpref.cpp
		win32/htmlpref.h
		win32/tadsdlg.cpp
		win32/tadsdlg.h
		win32/tadsdlg2.cpp
		win32/tadscbtn.cpp
		win32/tadscbtn.h
		win32/tadsreg.cpp
		win32/tadsreg.h
		win32/tadsjpeg.cpp
		win32/tadsjpeg.h
		win32/tadsimg.cpp
		win32/tadsimg.h
		win32/tadspng.cpp
		win32/tadspng.h
		win32/tadsmng.cpp
		win32/tadsmng.h
		win32/tadssnd.cpp
		win32/tadssnd.h
		win32/tadscsnd.cpp
		win32/tadscsnd.h
		win32/tadsvorb.cpp
		win32/tadsvorb.h
		win32/tadsmidi.cpp
		win32/tadsmidi.h
		win32/tadswav.cpp
		win32/tadswav.h
		win32/tadsole.cpp
		win32/tadsole.h
		win32/oem_w32.c
		win32/foldsel2.cpp
		win32/w32fndlg.cpp
		win32/w32fndlg.h
		win32/iconmenu.cpp
		win32/iconmenu.h
		win32/tadswebctl.cpp
		win32/tadswebctl.h
		win32/tadscom.cpp
		win32/tadscom.h
	)

	set(mpeg_src
		mpegamp/misc2.cpp
		mpegamp/layer3.cpp
		mpegamp/layer2.cpp
		mpegamp/getdata.cpp
		win32/mpegamp/getbits.cpp
		mpegamp/huffman.cpp
		mpegamp/transform.cpp
		win32/mpegamp/mpegamp_w32.cpp
		win32/mpegamp/mpegamp_w32.h
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
		win32/t3main.h
		win32/w32t3.cpp
	)
endif()

if (EMSCRIPTEN)
	set(base_src
		${base_src}
		emscripten/htmlemscripten.cpp
		emscripten/htmlemscripten.h
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

# The classic htmlt3/tadsweb executables below only have a Win32 (and, for
# htmlt3, an Emscripten) OS-glue backend - html_os.h has no non-Windows,
# non-Emscripten branch for them (guit3, added via add_subdirectory(imgui)
# below, is the supported cross-platform GUI target - see migration.md M4).
# Skip them entirely elsewhere. Under Emscripten this classic htmlt3 and
# guit3 build side by side from the same configure - see migration.md 5.6
# for how t3htm (tads3/CMakeLists.txt) serves both without forking.
if (WIN32 OR EMSCRIPTEN)

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
		mpegamp
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
	set_target_properties(${htmlt3_target_name} PROPERTIES LINK_FLAGS "-s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=_emscripten_receive_on_main_thread_js -s PROXY_TO_PTHREAD -s FORCE_FILESYSTEM -s INITIAL_MEMORY=256MB -s SAFE_HEAP=1 -lwebsocket.js -gsource-map -pthread -sPROXY_POSIX_SOCKETS -Wl,--wrap=fgets,--wrap=fflush,--wrap=getchar,--wrap=socket,--wrap=getsockname,--wrap=getpeername")
	
	install(TARGETS ${htmlt3_target_name}
		DESTINATION .
		COMPONENT Classic
	)

	install (FILES
		"$<TARGET_FILE_DIR:htmlt3>/htmlt3.wasm"
		"$<TARGET_FILE_DIR:htmlt3>/htmlt3.wasm.map"
		${CMAKE_CURRENT_SOURCE_DIR}/emscripten/htmlt3.html
		DESTINATION .
		COMPONENT Classic)
endif()

tads_source_group(${htmlt3_target_name} ${HTMLTADS_DIR})

endif() # WIN32 OR EMSCRIPTEN

if (IMGUI_TADS)
	add_subdirectory(imgui)
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
    win32/xzip/XZip.h
)

set(tdb_base_src
	${base_src}
    ${mpeg_src}
    ${xzip_src}

    win32/w32nogch.cpp
	htmldcfg.cpp
	htmldcfg.h
	htmldbg.cpp
	htmldbg.h
	win32/w32tdb.cpp
	win32/w32tdb.h
	win32/tinyxml.cpp
	win32/tinyxml.h
	win32/tadshtmldlg.cpp
	win32/tadshtmldlg.h
	win32/w32help.cpp
	win32/w32help.h
	win32/itadsworkbenchiid.cpp
	win32/w32sci.cpp
	win32/w32sci.h
	win32/tbarmbar.cpp
	win32/tbarmbar.h
	win32/mditab.cpp
	win32/mditab.h
	win32/tadsifid.c
	win32/tadsifid.h
	win32/w32expr.cpp
	win32/w32expr.h
	win32/w32bpdlg.cpp
	win32/w32bpdlg.h
	win32/w32dbgpr.cpp
	win32/w32dbgpr.h
	win32/tadstab.cpp
	win32/tadstab.h
	win32/tadsdde.cpp
	win32/tadsdde.h
	win32/tadsdock.cpp
	win32/tadsdock.h
	win32/w32dbwiz.cpp
	win32/w32dbwiz.h
	win32/babelifc.cpp
	win32/babelifc.h
)

set(tdbt3_src
    ${tdb_base_src}
    win32/w32tdbt3.cpp
    htmldb3.cpp
	htmldc3.cpp
    win32/t3main.cpp
    win32/t3main.h
    win32/w32prj.cpp
    win32/w32prj.h
	win32/w32script.cpp
	win32/w32script.h
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
mpegamp
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

tads_source_group(htmltdb3_tmp ${HTMLTADS_DIR})


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

tads_source_group(tadsweb ${HTMLTADS_DIR})

###install rules

install(TARGETS tadsweb
	DESTINATION .
	COMPONENT Classic
)

install(PROGRAMS
	${htmlt3_file} ${htmltdb3_file}
	DESTINATION .
	COMPONENT Classic
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
    COMPONENT Classic
)

install(FILES
	notes3/authkit/license.txt
	DESTINATION .
	COMPONENT Classic
)

install(DIRECTORY
	win32/mkwebfiles
	DESTINATION .
	COMPONENT Classic
)

install(FILES
	notes/changes.htm
	notes/compat.htm
	DESTINATION doc
  COMPONENT Classic
)