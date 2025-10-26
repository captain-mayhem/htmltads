include(GenerateExportHeader)

add_library(docsearch SHARED
	docsearch.cpp
	docsearch.h
)

generate_export_header(docsearch)

target_sources(docsearch
	PRIVATE
	 ${CMAKE_CURRENT_BINARY_DIR}/docsearch_export.h
)

target_include_directories(docsearch PUBLIC 
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
$<INSTALL_INTERFACE:include>
)

add_library(Docsearch::docsearch ALIAS docsearch)

install(TARGETS
	docsearch
	EXPORT TadsTargets
	RUNTIME DESTINATION .
)
