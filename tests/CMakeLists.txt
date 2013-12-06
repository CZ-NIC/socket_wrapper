project(tests C)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

add_executable(echo_srv echo_srv.c)

add_cmocka_test(testsuite testsuite.c ${CMOCKA_LIBRARY} ${SWRAP_REQUIRED_LIBRARIES})
add_cmocka_test(test_echo_udp_sendto_recvfrom test_echo_udp_sendto_recvfrom.c ${CMOCKA_LIBRARY} ${SWRAP_REQUIRED_LIBRARIES})

set(SWRAP_TESTS testsuite test_echo_udp_sendto_recvfrom)
foreach(_SWRAP_TEST ${SWRAP_TESTS})
    if (OSX)
        set_property(
            TEST
                ${_SWRAP_TEST}
            PROPERTY
                ENVIRONMENT DYLD_FORCE_FLAT_NAMESPACE=1;DYLD_INSERT_LIBRARIES=${CMAKE_BINARY_DIR}/src/libsocket_wrapper.dylib)
    else ()
        set_property(
            TEST
                ${_SWRAP_TEST}
            PROPERTY
                ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/src/libsocket_wrapper.so)
    endif()
endforeach()
