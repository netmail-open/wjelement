add_library(wjwriter
	wjwriter.c
)

target_link_libraries(wjwriter
	xpl
	${ALL_LIBS}
)
#link_directories($(MPLUS_GUARDIAN_BINARY_DIR)/src/libs/wjwriter)

SET_TARGET_PROPERTIES(
	wjwriter
	PROPERTIES
	VERSION 1.0.0
	SOVERSION 1
	INSTALL_NAME_DIR "${LIB_DEST_DIR}"
)


install(TARGETS wjwriter
    LIBRARY DESTINATION ${LIB_DEST_DIR}
    ARCHIVE DESTINATION ${LIB_DEST_DIR}
    )
