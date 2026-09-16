install(FILES
    Adv3-advanced.tdb-project-starter
	Adv3-advanced-webui.tdb-project-starter
	Adv3-intro.tdb-project-starter
	Adv3-intro-webui.tdb-project-starter
    DESTINATION lib/adv3
    COMPONENT Classic
)

install(FILES
    Plain-T3.tdb-project-starter
    DESTINATION lib
    COMPONENT Classic
)

install(FILES
    Epsilon.keymap
	WorkbenchDefault.keymap
	readme.txt
    DESTINATION .
    COMPONENT Classic
)
