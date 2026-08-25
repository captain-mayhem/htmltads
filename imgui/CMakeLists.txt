add_library(imgui STATIC
	imconfig.h
	imgui.cpp
	imgui.h
	imgui_demo.cpp
	imgui_draw.cpp
	imgui_impl_glfw.cpp
	imgui_impl_glfw.h
	imgui_impl_opengl3.cpp
	imgui_impl_opengl3.h
	imgui_impl_opengl3_loader.h
	imgui_internal.h
	imgui_tables.cpp
	imgui_widgets.cpp
	imstb_textedit.h
	imstb_rectpack.h
	imstb_truetype.h
	misc/freetype/imgui_freetype.cpp
	misc/freetype/imgui_freetype.h
)

# PUBLIC so consumers can use both `#include <imgui/imgui.h>` (this directory's
# parent) and the plain `#include "imgui.h"` used internally by misc/freetype's
# sources (this directory itself).
target_include_directories(imgui PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(imgui PUBLIC glfw freetype)

if (WIN32)
	target_link_libraries(imgui PUBLIC Opengl32.lib)
endif()

add_executable(imgui_test
	main.cpp
)

target_link_libraries(imgui_test PRIVATE imgui)
