project(tests C)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

add_cmocka_test(testsuite testsuite.c ${CMOCKA_LIBRARY})
set_property(
    TEST
        testsuite
    PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/src/libsocket_wrapper.so)
set_property(
    TEST
        testsuite
    APPEND
    PROPERTY
        ENVIRONMENT SOCKET_WRAPPER_DIR=${CMAKE_CURRENT_BINARY_DIR})
set_property(
    TEST
        testsuite
    APPEND
    PROPERTY
        ENVIRONMENT SOCKET_WRAPPER_DEFAULT_IFACE="11")
