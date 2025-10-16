add_library(babel STATIC
	babel_handler.c
    babel_ifiction_functions.c
    babel_story_functions.c
    ifiction.c
    misc.c
    register.c
    register_ifiction.c
	
	blorb.c
    advsys.c
    magscrolls.c
    agt.c
    level9.c
    adrift.c
    hugo.c
    glulx.c
    zcode.c
    executable.c
    tads2.c
    tads3.c
	
	tads.c
)

target_include_directories(babel PUBLIC .)

add_library(Tads::babel ALIAS babel)