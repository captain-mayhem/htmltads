
add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(jpeg)
add_subdirectory(libmng)

add_subdirectory(libogg)
add_subdirectory(libvorbis)

if (NOT WIN32)
	return()
endif()

add_subdirectory(htmltads)