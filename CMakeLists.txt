
if (WIN32)
	add_subdirectory(zlib)
	add_subdirectory(libpng)
	add_subdirectory(jpeg)
	add_subdirectory(libmng)

	add_subdirectory(libogg)
	add_subdirectory(libvorbis)

	add_subdirectory(textindex)
	add_subdirectory(scintilla)

	add_subdirectory(htmltads)
	add_subdirectory(t3doc)
	add_subdirectory(wbaddons)
endif()

if (EMSCRIPTEN)
endif()