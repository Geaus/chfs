cmake_minimum_required(VERSION 3.10)

include_directories(${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include/ )

add_executable(allocator_stress_test
    EXCLUDE_FROM_ALL
    allocator.cc
)
add_dependencies(build-tests allocator_stress_test)
add_dependencies(check-tests allocator_stress_test)

target_link_libraries(allocator_stress_test chfs gtest gmock_main) 

include(GoogleTest)
gtest_discover_tests(allocator_stress_test)

# Set test target properties and dependencies.
set_target_properties(allocator_stress_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stress-test"
        COMMAND allocator_stress_test
        )
