add_library(wjreader
	wjreader.c
)

target_link_libraries(wjreader
	xpl
	${ALL_LIBS}
)

SET_TARGET_PROPERTIES(
	wjreader
	PROPERTIES
	VERSION 1.0.0
	SOVERSION 1
	INSTALL_NAME_DIR "${LIB_DEST_DIR}"
)

install(TARGETS wjreader
	LIBRARY DESTINATION ${LIB_DEST_DIR}
	ARCHIVE DESTINATION ${LIB_DEST_DIR}
	)
