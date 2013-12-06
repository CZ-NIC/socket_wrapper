project(tests C)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

add_executable(echo_srv echo_srv.c)

add_cmocka_test(testsuite testsuite.c ${CMOCKA_LIBRARY} ${SWRAP_REQUIRED_LIBRARIES})
if (OSX)
set_property(
    TEST
        testsuite
    PROPERTY
        ENVIRONMENT DYLD_FORCE_FLAT_NAMESPACE=1;DYLD_INSERT_LIBRARIES=${CMAKE_BINARY_DIR}/src/libsocket_wrapper.dylib)
else ()
set_property(
    TEST
        testsuite
    PROPERTY
        ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/src/libsocket_wrapper.so)
endif()
