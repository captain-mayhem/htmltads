
add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(jpeg)
add_subdirectory(libmng)

if (NOT WIN32)
	return()
endif()

add_subdirectory(htmltads)