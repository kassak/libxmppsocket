cmake_minimum_required (VERSION 2.6)
project (libxmppsocket)

set(CMAKE_C_FLAGS "-Wimplicit")
set(CMAKE_C_FLAGS_DEBUG "-Wimplicit")
set(CMAKE_C_FLAGS_RELEASE "-Wimplicit")

add_subdirectory(libstrophe)
link_directories(${CMAKE_BINARY_DIR}/libstrophe)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/libstrophe/liboccam)

set(SOURCES
  src/socket.c
  src/xmpp_socket.c
)

add_executable(test ${SOURCES})
target_link_libraries(test strophe)
