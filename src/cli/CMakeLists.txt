add_executable(wjecli wjecli.c wjecli.h cmds.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_CFLAGS} ${PTHREAD_CFLAGS} ${OPENSSL_CFLAGS}")

target_link_libraries(wjecli
	wjelement
)

install(TARGETS wjecli DESTINATION bin)
install(PROGRAMS wje DESTINATION bin)

