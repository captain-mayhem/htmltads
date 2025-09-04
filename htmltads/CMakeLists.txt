add_executable(htmlt3_tmp
	win32/htmlw32.cpp
)

target_include_directories(htmlt3_tmp PRIVATE
.
)

target_link_libraries(htmlt3_tmp PRIVATE
	t3htm
)
