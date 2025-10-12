add_library(docsearch STATIC
	docsearch.cpp
	docsearch.h
)

target_include_directories(docsearch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(Docsearch::docsearch ALIAS docsearch)
