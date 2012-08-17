add_library(xpl SHARED
	xpl.c
)

if (WINDOWS)
	set(WIN_EXTRA_LIBS hulamemmgr)
endif(WINDOWS)

target_link_libraries(xpl
	${WIN_EXTRA_LIBS}
	${ALL_LIBS}
)

SET_TARGET_PROPERTIES(
	xpl
	PROPERTIES
	VERSION 0.0.0
	SOVERSION 0
	INSTALL_NAME_DIR "${LIB_DEST_DIR}"
)

install(TARGETS xpl DESTINATION ${LIB_DEST_DIR})
