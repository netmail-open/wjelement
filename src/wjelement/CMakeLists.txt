add_library(wjelement SHARED
	element.c
	element.h
	search.c
	types.c
	schema.c
	hash.c
)

if (WINDOWS)
	set(WIN_EXTRA_LIBS hulamemmgr)
endif(WINDOWS)

target_link_libraries(wjelement
	${WIN_EXTRA_LIBS}
	wjreader
	wjwriter

	${ALL_LIBS}
)

SET_TARGET_PROPERTIES(
	wjelement
	PROPERTIES
	VERSION 1.0.0
	SOVERSION 1
	INSTALL_NAME_DIR "${LIB_DEST_DIR}"
)

install(TARGETS wjelement DESTINATION ${LIB_DEST_DIR})


# unit tests
add_executable(wjeunit
	wjeunit.c
)

target_link_libraries(wjeunit
	wjreader
	wjwriter
	wjelement
#	hulamemmgr
#	hulautil
	${PTHREAD_LIBS}
	${ALL_LIBS}
)

# The use of ${EXECUTABLE_OUTPUT_PATH} is required for windows
add_test(WJElement:SelfReference		${EXECUTABLE_OUTPUT_PATH}/wjeunit self			)
add_test(WJElement:ElementNames			${EXECUTABLE_OUTPUT_PATH}/wjeunit names			)
add_test(WJElement:Offsets				${EXECUTABLE_OUTPUT_PATH}/wjeunit offsets		)
add_test(WJElement:Lists				${EXECUTABLE_OUTPUT_PATH}/wjeunit lists			)
add_test(WJElement:Paths				${EXECUTABLE_OUTPUT_PATH}/wjeunit paths			)
add_test(WJElement:GetValues			${EXECUTABLE_OUTPUT_PATH}/wjeunit getvalue		)
add_test(WJElement:GetMultipleValues	${EXECUTABLE_OUTPUT_PATH}/wjeunit getvalues		)
add_test(WJElement:SetValues			${EXECUTABLE_OUTPUT_PATH}/wjeunit setvalue		)
add_test(WJElement:SetMultipleValues	${EXECUTABLE_OUTPUT_PATH}/wjeunit setvalues		)
add_test(WJElement:NewValues			${EXECUTABLE_OUTPUT_PATH}/wjeunit newvalue		)
add_test(WJElement:NewMultipleValues	${EXECUTABLE_OUTPUT_PATH}/wjeunit newvalues		)
add_test(WJElement:PutValues			${EXECUTABLE_OUTPUT_PATH}/wjeunit putvalue		)
add_test(WJElement:PutMultipleValues	${EXECUTABLE_OUTPUT_PATH}/wjeunit putvalues		)
add_test(WJElement:Append				${EXECUTABLE_OUTPUT_PATH}/wjeunit append		)
add_test(WJElement:Conditions			${EXECUTABLE_OUTPUT_PATH}/wjeunit conditions	)
add_test(WJElement:Optionals			${EXECUTABLE_OUTPUT_PATH}/wjeunit optionals		)
