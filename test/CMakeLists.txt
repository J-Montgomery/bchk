include(GoogleTest)
find_package(GTest REQUIRED)

function(add_bchk_test test_name)
    add_executable(${test_name} ${ARGN})
    target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(${test_name} PRIVATE
        bchk_core
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(${test_name})
endfunction()

add_bchk_test(basic_tests basic_tests.cpp)
