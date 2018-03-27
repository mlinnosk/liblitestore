# Test target
set(TEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/litestore_helpers_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/litestore_test_common.h
    ${CMAKE_CURRENT_LIST_DIR}/litestore_test.cpp)
add_executable(unit_tests ${TEST_SOURCES})
target_include_directories(unit_tests
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}
) 
target_compile_options(unit_tests
    PRIVATE -std=c++14 -g -Wall -Wextra -Werror -Wpedantic -Wconversion -Wswitch-default -Wswitch-enum -Wunreachable-code -Wwrite-strings -Wcast-align -Wshadow -Wundef)
find_package (Threads)
target_link_libraries(unit_tests
    PRIVATE gtest ${CMAKE_THREAD_LIBS_INIT} litestore sqlite3 dl)