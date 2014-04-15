project(tests C)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

set(TORTURE_LIBRARY torture)

# RFC862 echo server
add_executable(echo_srv echo_srv.c)
target_link_libraries(echo_srv ${SWRAP_REQUIRED_LIBRARIES})

add_library(${TORTURE_LIBRARY} STATIC torture.c)
target_link_libraries(${TORTURE_LIBRARY}
    ${CMOCKA_LIBRARY}
    ${SWRAP_REQUIRED_LIBRARIES})

set(SWRAP_TESTS
    test_ioctl
    test_echo_tcp_connect
    test_echo_tcp_socket_options
    test_echo_tcp_write_read
    test_echo_tcp_writev_readv
    test_echo_tcp_get_peer_sock_name
    test_echo_udp_sendto_recvfrom
    test_echo_udp_send_recv
    test_echo_udp_sendmsg_recvmsg)

if (HAVE_STRUCT_MSGHDR_MSG_CONTROL)
    set(SWRAP_TESTS ${SWRAP_TESTS} test_sendmsg_recvmsg_fd)
endif (HAVE_STRUCT_MSGHDR_MSG_CONTROL)

foreach(_SWRAP_TEST ${SWRAP_TESTS})
    add_cmocka_test(${_SWRAP_TEST} ${_SWRAP_TEST}.c ${TORTURE_LIBRARY})

    if (OSX)
        set_property(
            TEST
                ${_SWRAP_TEST}
            PROPERTY
                ENVIRONMENT DYLD_FORCE_FLAT_NAMESPACE=1;DYLD_INSERT_LIBRARIES=${SOCKET_WRAPPER_LOCATION})
    else ()
        set_property(
            TEST
                ${_SWRAP_TEST}
            PROPERTY
                ENVIRONMENT LD_PRELOAD=${SOCKET_WRAPPER_LOCATION})
    endif()
endforeach()
