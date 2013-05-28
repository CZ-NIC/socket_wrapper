project(tests C)

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

add_cmocka_test(testsuite testsuite.c ${CMOCKA_LIBRARY})
