include(GoogleTest)
find_package(GTest REQUIRED)

function(add_bchk_test test_name)
    add_executable(${test_name} ${ARGN})
    target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    target_compile_definitions(${test_name} PRIVATE
        BCHK_TEST_INCLUDE_DIR="$<TARGET_PROPERTY:bchk_headers,INTERFACE_INCLUDE_DIRECTORIES>"
    )
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${bchk_core_INCLUDE_DIRS})

    target_link_libraries(${test_name} PRIVATE
        bchk_core
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(${test_name})
endfunction()

add_bchk_test(basic_tests basic_tests.cpp)
