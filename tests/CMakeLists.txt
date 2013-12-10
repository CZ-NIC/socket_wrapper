project(tests C)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

set(TORTURE_LIBRARY torture)

# RFC862 echo server
add_executable(echo_srv echo_srv.c)

add_library(${TORTURE_LIBRARY} STATIC torture.c)
target_link_libraries(${TORTURE_LIBRARY}
    ${CMOCKA_LIBRARY}
    ${SWRAP_REQUIRED_LIBRARIES})

set(SWRAP_TESTS testsuite test_echo_udp_sendto_recvfrom)

foreach(_SWRAP_TEST ${SWRAP_TESTS})
    add_cmocka_test(${_SWRAP_TEST} ${_SWRAP_TEST}.c ${TORTURE_LIBRARY})

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
