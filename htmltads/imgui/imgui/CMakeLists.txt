add_executable(imgui_test
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
	main.cpp
	imstb_textedit.h
	imstb_rectpack.h
	imstb_truetype.h
	misc/freetype/imgui_freetype.cpp
	misc/freetype/imgui_freetype.h
)

target_link_libraries(imgui_test PRIVATE glfw)

target_include_directories(imgui_test
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(imgui_test PRIVATE freetype)

if (WIN32)
	target_link_libraries(imgui_test PRIVATE Opengl32.lib)
endif()