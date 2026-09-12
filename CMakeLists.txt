add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(jpeg)
add_subdirectory(libmng)

option(IMGUI_TADS "Build Tads based on glfw and imgui" ON)

if (IMGUI_TADS)
	set(GLFW_LIBRARY_TYPE "STATIC")
	add_subdirectory(glfw)
	
	set (BUILD_SHARED_LIBS OFF)
	set(SKIP_INSTALL_ALL ON)
	add_subdirectory(freetype)

	add_subdirectory(imgui)
endif()

# libogg/libvorbis back guit3's Ogg Vorbis audio decoder (tadsvorb.cpp) on every
# platform. textindex/scintilla are Workbench-only and stay Windows-only.
add_subdirectory(libogg)
add_subdirectory(libvorbis)

if (WIN32)
	add_subdirectory(textindex)
	add_subdirectory(scintilla)
endif()

add_subdirectory(htmltads)

if (WIN32)
	add_subdirectory(t3doc)
	add_subdirectory(wbaddons)
endif()