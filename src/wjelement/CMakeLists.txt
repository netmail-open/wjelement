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
	VERSION 0.0.0
	SOVERSION 0
	INSTALL_NAME_DIR "${LIB_DEST_DIR}"
)

install(TARGETS wjelement DESTINATION ${LIB_DEST_DIR})


# unit tests
add_executable(wjeunit
	wjeunit.c
)

target_link_libraries(wjeunit
	${AVOID_VERSION}
	wjreader
	wjwriter
	wjelement
#	hulamemmgr
#	hulautil
	${PTHREAD_LIBS}
	${ALL_LIBS}
)

add_test(WJElement:SelfReference		./wjeunit self			)
add_test(WJElement:ElementNames			./wjeunit names			)
add_test(WJElement:Offsets				./wjeunit offsets		)
add_test(WJElement:Lists				./wjeunit lists			)
add_test(WJElement:Paths				./wjeunit paths			)
add_test(WJElement:GetValues			./wjeunit getvalue		)
add_test(WJElement:GetMultipleValues	./wjeunit getvalues		)
add_test(WJElement:SetValues			./wjeunit setvalue		)
add_test(WJElement:SetMultipleValues	./wjeunit setvalues		)
add_test(WJElement:NewValues			./wjeunit newvalue		)
add_test(WJElement:NewMultipleValues	./wjeunit newvalues		)
add_test(WJElement:PutValues			./wjeunit putvalue		)
add_test(WJElement:PutMultipleValues	./wjeunit putvalues		)
add_test(WJElement:Append				./wjeunit append		)
add_test(WJElement:Conditions			./wjeunit conditions	)
