add_library(wjreader SHARED
	wjreader.c
)

if (WINDOWS)
	set(WIN_EXTRA_LIBS hulamemmgr)
endif(WINDOWS)

target_link_libraries(wjreader
	${WIN_EXTRA_LIBS}
	${ALL_LIBS}
)

SET_TARGET_PROPERTIES(
	wjreader
	PROPERTIES
	VERSION 1.0.0
	SOVERSION 1
	INSTALL_NAME_DIR "${LIB_DEST_DIR}"
)

install(TARGETS wjreader LIBRARY DESTINATION ${LIB_DEST_DIR})
