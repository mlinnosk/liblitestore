cmake_minimum_required(VERSION 3.0)
project(litestore VERSION 0.1.0)
include(GNUInstallDirs)
include(gtest.cmake)

set(LIB_DIR ${PROJECT_SOURCE_DIR}/libs)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(TEST_DIR ${PROJECT_SOURCE_DIR}/tests)

# subdirs
add_subdirectory(${LIB_DIR}/sqlite-amalgamation-3220000)
add_subdirectory(${TEST_DIR})

# main target
add_library(litestore SHARED
    ${SRC_DIR}/litestore.c)
#set_target_properties(litestore PROPERTIES
#    PUBLIC_HEADER src/litestore.h
#    PUBLIC_HEADER src/litestore_helpers.h)
target_compile_options(litestore
    PUBLIC -fPIC
    # -Wpedantic -Wconversion -Wswitch-default -Wswitch-enum -Wunreachable-code -Wwrite-strings -Wcast-align -Wshadow -Wundef
    PRIVATE -std=c99 -O3 -Wall -Wextra -Werror)
target_include_directories(litestore
    INTERFACE ${INCLUDE_DIR}
    PRIVATE ${INCLUDE_DIR}
)
target_link_libraries(litestore
    PRIVATE sqlite3)

CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

# install directives
install(TARGETS litestore LIBRARY
    DESTINATION lib)
install(FILES
    ${INCLUDE_DIR}/litestore/litestore.h
    ${INCLUDE_DIR}/litestore/litestore_helpers.h
    DESTINATION include/litestore)
install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
    DESTINATION lib/pkgconfig)

