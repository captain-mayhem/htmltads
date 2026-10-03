# The 3rdparty libraries below are also provided by the guitads sibling repo
# (which owns guit3). When tads-runner finds both repos, it adds guitads
# first, so every target guitads already defines is skipped here (the
# vendored copies are identical) and only the targets unique to this repo get
# added.

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

# libogg/libvorbis back htmlt3's Ogg Vorbis audio decoder (win32/tadsvorb.cpp).
# textindex/scintilla are Workbench-only and stay Windows-only.
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
