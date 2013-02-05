project(libsocket_wrapper C)

include_directories(${CMAKE_BINARY_DIR})
add_library(socket_wrapper SHARED socket_wrapper.c)
