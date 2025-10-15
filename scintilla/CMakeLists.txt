add_library(Scintilla SHARED
	src/AutoComplete.cxx
	src/CallTip.cxx
	src/CellBuffer.cxx
	lexlib/CharacterSet.cxx
	src/CharClassify.cxx
	src/ContractionState.cxx
	src/Decoration.cxx
	src/Document.cxx
	src/Editor.cxx
	src/Indicator.cxx
	src/KeyMap.cxx
	src/LineMarker.cxx
	src/PerLine.cxx
	win32/PlatWin.cxx
	src/PositionCache.cxx
	lexlib/PropSetSimple.cxx
	src/RESearch2.cxx
	src/RunStyles.cxx
	src/ScintillaBase.cxx
	win32/ScintillaWin.cxx
	src/Selection.cxx
	src/Style.cxx
	src/UniConversion.cxx
	src/ViewStyle.cxx
	src/XPM.cxx

	win32/ScintRes.rc
	win32/Scintilla.def
)

target_include_directories(Scintilla 
PUBLIC 
	include 
	lexlib
PRIVATE
	src)

target_compile_definitions(Scintilla
PRIVATE
DLL
)

target_link_libraries(Scintilla
	Imm32
)

add_library(Scintilla::Scintilla ALIAS Scintilla)

install(TARGETS 
	Scintilla
	EXPORT TadsTargets
	DESTINATION .
)
