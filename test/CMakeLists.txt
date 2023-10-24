cmake_minimum_required(VERSION 3.10)

include(GoogleTest)

file(GLOB_RECURSE CHFS_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*/*test.cc")

# #####################################################################################################################
# MAKE TARGETS
# #####################################################################################################################

# #########################################
# "make check-tests"
# #########################################
add_custom_target(build-tests COMMAND ${CMAKE_CTEST_COMMAND} --show-only)
add_custom_target(check-tests COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

# #########################################
# "make XYZ_test"
# "make run_XYZ_test"
# #########################################
foreach (chfs_test_source ${CHFS_TEST_SOURCES})
    # Create a human readable name.
    get_filename_component(chfs_test_filename ${chfs_test_source} NAME)
    string(REPLACE ".cc" "" chfs_test_name ${chfs_test_filename})

    # Add the test target separately and as part of "make check-tests".
    add_executable(${chfs_test_name} EXCLUDE_FROM_ALL ${chfs_test_source})
    add_dependencies(build-tests ${chfs_test_name})
    add_dependencies(check-tests ${chfs_test_name})

    gtest_discover_tests(${chfs_test_name}
            EXTRA_ARGS
            --gtest_color=auto
            --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${chfs_test_name}.xml
            --gtest_catch_exceptions=0
            DISCOVERY_TIMEOUT 120
            PROPERTIES
            TIMEOUT 120
            )

    target_link_libraries(${chfs_test_name} chfs gtest gmock_main)

    # Set test target properties and dependencies.
    set_target_properties(${chfs_test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
            COMMAND ${chfs_test_name}
            )

    # Add target for running a single test
    add_custom_target(run_${chfs_test_name}
            COMMAND ./${chfs_test_name}
            DEPENDS ${chfs_test_name}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test
            )
endforeach ()


