cmake_minimum_required(VERSION 3.0)
project(sqlite3 VERSION 3.2.2)
include(GNUInstallDirs)
add_library(sqlite3 STATIC
    sqlite3.c)
target_include_directories(sqlite3
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(sqlite3
    PUBLIC -fPIC
    PRIVATE -O3) 