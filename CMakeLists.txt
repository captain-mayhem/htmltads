# The 3rdparty libraries and guit3 below are also provided by the guitads
# sibling repo. When tads-runner finds both repos, it adds guitads first, so
# every target guitads already defines is skipped here (the vendored copies
# are identical) and only the targets unique to this repo get added. Without
# guitads, everything is built here as before.

if (NOT TARGET ZLIB::ZLIB)
	add_subdirectory(zlib)
endif()
if (NOT TARGET PNG::PNG)
	add_subdirectory(libpng)
endif()
if (NOT TARGET JPEG::JPEG)
	add_subdirectory(jpeg)
endif()
if (NOT TARGET MNG::MNG)
	add_subdirectory(libmng)
endif()

option(IMGUI_TADS "Build Tads based on glfw and imgui" ON)

if (IMGUI_TADS)
	if (NOT EMSCRIPTEN AND NOT TARGET glfw)
		# GLFW has no Emscripten backend of its own to build - under that
		# platform, Emscripten emulates the GLFW API itself via the
		# -sUSE_GLFW=3 link flag (set on guit3 itself, see
		# htmltads/imgui/CMakeLists.txt), so there is no vendored library to
		# add here.
		set(GLFW_LIBRARY_TYPE "STATIC")
		add_subdirectory(glfw)
	endif()

	set (BUILD_SHARED_LIBS OFF)
	set(SKIP_INSTALL_ALL ON)
	if (NOT TARGET freetype)
		add_subdirectory(freetype)
	endif()

	if (NOT TARGET imgui)
		add_subdirectory(imgui)
	endif()
endif()

# libogg/libvorbis back guit3's Ogg Vorbis audio decoder (tadsvorb.cpp) on every
# platform. textindex/scintilla are Workbench-only and stay Windows-only.
if (NOT TARGET Ogg::ogg)
	add_subdirectory(libogg)
endif()
if (NOT TARGET vorbis)
	add_subdirectory(libvorbis)
endif()

if (WIN32)
	add_subdirectory(textindex)
	add_subdirectory(scintilla)
endif()

add_subdirectory(htmltads)

if (WIN32)
	add_subdirectory(t3doc)
	add_subdirectory(wbaddons)
endif()
