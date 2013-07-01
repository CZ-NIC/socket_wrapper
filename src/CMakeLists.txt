project(libsocket_wrapper C)

include_directories(${CMAKE_BINARY_DIR})
add_library(socket_wrapper SHARED socket_wrapper.c)

target_link_libraries(socket_wrapper ${SWRAP_REQUIRED_LIBRARIES})

install(
  TARGETS
    socket_wrapper
  RUNTIME DESTINATION ${BIN_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
)

set_target_properties(
  socket_wrapper
    PROPERTIES
      VERSION
        ${LIBRARY_VERSION}
      SOVERSION
        ${LIBRARY_SOVERSION}
)
