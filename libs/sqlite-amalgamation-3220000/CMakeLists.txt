cmake_minimum_required(VERSION 3.0)
project(sqlite3 VERSION 3.2.2)
include(GNUInstallDirs)
add_library(sqlite3 STATIC
    sqlite3.c)
set_target_properties(sqlite3 PROPERTIES
    PUBLIC_HEADER sqlite3.h)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(sqlite3
    PUBLIC -fPIC
    PRIVATE -O3) 