add_subdirectory(babel)

add_library(tads3 SHARED
	tads3addin.cpp
	itadsworkbenchiid.cpp
	tads3addin.rc
)

target_link_libraries(tads3 PRIVATE Scintilla::SciInterface)

set_target_properties(tads3 PROPERTIES SUFFIX .twbAddIn)

add_library(html SHARED
	htmladdin.cpp
	itadsworkbenchiid.cpp
	htmladdin.rc
)

target_link_libraries(html PRIVATE Scintilla::SciInterface)

set_target_properties(html PROPERTIES SUFFIX .twbAddIn)

add_library(script SHARED
	scriptaddin.cpp
	itadsworkbenchiid.cpp
	scriptaddin.rc
)

target_link_libraries(script PRIVATE Scintilla::SciInterface)

set_target_properties(script PROPERTIES SUFFIX .twbAddIn)

install(TARGETS 
	tads3 html script
	EXPORT TadsTargets
	RUNTIME DESTINATION .)