cmake_minimum_required(VERSION 3.10)

include_directories(${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include/ )

add_executable(allocator_stress_test
    EXCLUDE_FROM_ALL
    allocator.cc
)

add_executable(concurrent_stress_test
    EXCLUDE_FROM_ALL
    concurrent.cc
)

target_link_libraries(allocator_stress_test chfs gtest gmock_main) 
target_link_libraries(concurrent_stress_test chfs gtest gmock_main)

include(GoogleTest)
gtest_discover_tests(allocator_stress_test)
gtest_discover_tests(concurrent_stress_test)

add_custom_target(run_concurrent_stress_test
    COMMAND concurrent_stress_test
    DEPENDS concurrent_stress_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)