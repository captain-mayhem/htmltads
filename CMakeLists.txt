
add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(jpeg)
add_subdirectory(libmng)

add_subdirectory(libogg)
add_subdirectory(libvorbis)

add_subdirectory(textindex)
add_subdirectory(scintilla)

if (NOT WIN32)
	return()
endif()

add_subdirectory(htmltads)
add_subdirectory(t3doc)