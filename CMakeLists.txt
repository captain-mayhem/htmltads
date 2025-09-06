
add_subdirectory(zlib)
add_subdirectory(libpng)

if (NOT WIN32)
	return()
endif()

add_subdirectory(htmltads)