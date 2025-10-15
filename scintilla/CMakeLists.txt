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

set(LEXSRC
	lexers/LexA68k.cxx
	lexers/LexAbaqus.cxx
	lexers/LexAda.cxx
	lexers/LexAPDL.cxx
	lexers/LexAsm.cxx
	lexers/LexAsn1.cxx
	lexers/LexASY.cxx
	lexers/LexAU3.cxx
	lexers/LexAVE.cxx
	lexers/LexBaan.cxx
	lexers/LexBash.cxx
	lexers/LexBasic.cxx
	lexers/LexBullant.cxx
	lexers/LexCaml.cxx
	lexers/LexCLW.cxx
	lexers/LexCmake.cxx
	lexers/LexCOBOL.cxx
	lexers/LexCoffeeScript.cxx
	lexers/LexConf.cxx
	lexers/LexCPP.cxx
	lexers/LexCrontab.cxx
	lexers/LexCsound.cxx
	lexers/LexCSS.cxx
	lexers/LexD.cxx
	lexers/LexEiffel.cxx
	lexers/LexErlang.cxx
	lexers/LexEScript.cxx
	lexers/LexFlagship.cxx
	lexers/LexForth.cxx
	lexers/LexFortran.cxx
	lexers/LexGAP.cxx
	lexers/LexGui4Cli.cxx
	lexers/LexHaskell.cxx
	lexers/LexHTML.cxx
	lexers/LexInno.cxx
	lexers/LexKix.cxx
	lexers/LexLisp.cxx
	lexers/LexLout.cxx
	lexers/LexLua.cxx
	lexers/LexMagik.cxx
	lexers/LexMarkdown.cxx
	lexers/LexMatlab.cxx
	lexers/LexMetapost.cxx
	lexers/LexMMIXAL.cxx
	lexers/LexModula.cxx
	lexers/LexMPT.cxx
	lexers/LexMSSQL.cxx
	lexers/LexMySQL.cxx
	lexers/LexNimrod.cxx
	lexers/LexNsis.cxx
	lexers/LexOpal.cxx
	lexers/LexOthers.cxx
	lexers/LexPascal.cxx
	lexers/LexPB.cxx
	lexers/LexPerl.cxx
	lexers/LexPLM.cxx
	lexers/LexPOV.cxx
	lexers/LexPowerPro.cxx
	lexers/LexPowerShell.cxx
	lexers/LexProgress.cxx
	lexers/LexPS.cxx
	lexers/LexPython.cxx
	lexers/LexR.cxx
	lexers/LexRebol.cxx
	lexers/LexRuby.cxx
	lexers/LexScriptol.cxx
	lexers/LexSmalltalk.cxx
	lexers/LexSML.cxx
	lexers/LexSorcus.cxx
	lexers/LexSpecman.cxx
	lexers/LexSpice.cxx
	lexers/LexSQL.cxx
	lexers/LexTACL.cxx
	lexers/LexTADS3.cxx
	lexers/LexTadsCmd.cxx
	lexers/LexTAL.cxx
	lexers/LexTCL.cxx
	lexers/LexTeX.cxx
	lexers/LexTxt2tags.cxx
	lexers/LexVB.cxx
	lexers/LexVerilog.cxx
	lexers/LexVHDL.cxx
	lexers/LexYAML.cxx
)
	
add_library(SciLexer SHARED
	lexlib/Accessor.cxx
	src/AutoComplete.cxx
	src/CallTip.cxx
	src/Catalogue.cxx
	src/CellBuffer.cxx
	lexlib/CharacterSet.cxx
	src/CharClassify.cxx
	src/ContractionState.cxx
	src/Decoration.cxx
	src/Document.cxx
	src/Editor.cxx
	src/ExternalLexer.cxx
	src/Indicator.cxx
	lexlib/LexerBase.cxx
	lexlib/LexerModule.cxx
	lexlib/LexerSimple.cxx
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
	src/StyleContext.cxx
	src/UniConversion.cxx
	src/ViewStyle.cxx
	lexlib/WordList.cxx
	src/XPM.cxx

	${LEXSRC}

	win32/ScintRes.rc
	win32/Scintilla.def
)

target_include_directories(SciLexer 
PUBLIC 
	include 
	lexlib
PRIVATE
	src)

target_compile_definitions(SciLexer
PRIVATE
SCI_LEXER
DLL
)

target_link_libraries(SciLexer
	Imm32
)

add_library(Scintilla::SciLexer ALIAS SciLexer)

install(TARGETS 
	SciLexer
	EXPORT TadsTargets
	DESTINATION .
)
