cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy.
set(BUILD_SHARED_LIBS OFF) # We expect external libraries to be linked statically.
set(CMAKE_CXX_STANDARD 17) # Compile as C++17.
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Require C++17 support.

project(ChFS
        VERSION 2023.1
        DESCRIPTION "An educational distributed filesystem for computing system engineering course."
        LANGUAGES C CXX
)


if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to `Debug` as none was specified.")
        set(CMAKE_BUILD_TYPE "Debug")
endif()

# People keep running CMake in the wrong folder, completely nuking their project or creating weird bugs.
# This checks if you're running CMake from a folder that already has CMakeLists.txt.
# Importantly, this catches the common case of running it from the root directory.
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" PATH_TO_CMAKELISTS_TXT)

if(EXISTS "${PATH_TO_CMAKELISTS_TXT}")
        message(FATAL_ERROR "Run CMake from a build subdirectory! \"mkdir build ; cd build ; cmake ..\" \
    Some junk files were created in this folder (CMakeCache.txt, CMakeFiles); you should delete those.")
endif()

set(CHFS_BUILD_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/build_support")
set(CHFS_CLANG_SEARCH_PATH "/usr/local/bin" "/usr/bin" "/usr/local/opt/llvm/bin" "/usr/local/opt/llvm@14/bin"
        "/opt/homebrew/opt/llvm@14/bin/")

#message(WARNING "check ${CMAKE_CXX_COMPILER_ID}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION MATCHES "^14.")
        message(STATUS "You're using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    else()
        message(WARNING "!! We recommend that you use clang-14 for developing Chfs. You're using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}, a different version.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        message(STATUS "You're using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
else()
        message(WARNING "!! We recommend that you use clang-14 for developing Chfs. You're using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}, which is not clang.")
endif()        

# CTest
enable_testing()

# clang-format

# attempt to find the binary if user did not specify
find_program(CLANG_FORMAT_BIN
        NAMES clang-format clang-format-14
        HINTS ${CHFS_CLANG_SEARCH_PATH})

if("${CLANG_FORMAT_BIN}" STREQUAL "CLANG_FORMAT_BIN-NOTFOUND")
        message(WARNING "Chfs/main couldn't find clang-format.")
else()
        message(STATUS "Chfs/main found clang-format at ${CLANG_FORMAT_BIN}")
endif()

# #####################################################################################################################
# COMPILER SETUP
# #####################################################################################################################

if(NOT DEFINED CHFS_SANITIZER)
        set(CHFS_SANITIZER address)
endif()

message("Build mode: ${CMAKE_BUILD_TYPE} at root ${PROJECT_SOURCE_DIR}")
message("${CHFS_SANITIZER} sanitizer will be enabled in debug mode.")

# Co

# Compiler flags.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Werror -Wno-sign-compare")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-unused-parameter -Wno-attributes")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fsanitize=${CHFS_SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")

# Output directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Includes.
set(CHFS_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
set(CHFS_TEST_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/test/include)
set(CHFS_THIRD_PARTY_INCLUDE_DIR
        ${PROJECT_SOURCE_DIR}/third_party
        ${PROJECT_SOURCE_DIR}/third_party/fmt/include
#        ${PROJECT_SOURCE_DIR}/third_party/libpg_query/include
        ${PROJECT_SOURCE_DIR}/third_party/argparse/include
#        ${PROJECT_SOURCE_DIR}/third_party/cpp_random_distributions
        ${PROJECT_SOURCE_DIR}/third_party/rpclib/include
)

include_directories(${CHFS_SRC_INCLUDE_DIR} ${CHFS_TEST_INCLUDE_DIR} ${CHFS_THIRD_PARTY_INCLUDE_DIR})

function(disable_target_warnings NAME)
        target_compile_options(${NAME} PRIVATE "-w")
endfunction()

# #####################################################################################################################
# Other CMake modules
# #####################################################################################################################
add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(stress-test)
add_subdirectory(daemons)



# #####################################################################################################################
# MAKE TARGETS
# #####################################################################################################################

# #########################################
# "make format"
# "make check-format"
# #########################################
string(CONCAT CHFS_FORMAT_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src,"
        "${CMAKE_CURRENT_SOURCE_DIR}/test,"
        "${CMAKE_CURRENT_SOURCE_DIR}/stress-test,"
        "${CMAKE_CURRENT_SOURCE_DIR}/daemons,"
)

# Runs clang format and updates files in place.
add_custom_target(format ${CHFS_BUILD_SUPPORT_DIR}/run_clang_format.py
        ${CLANG_FORMAT_BIN}
        ${CHFS_BUILD_SUPPORT_DIR}/clang_format_exclusions.txt
        --source_dirs
        ${CHFS_FORMAT_DIRS}
        --fix
        --quiet
)

# Runs clang format and exits with a non-zero exit code if any files need to be reformatted
add_custom_target(check-format ${CHFS_BUILD_SUPPORT_DIR}/run_clang_format.py
        ${CLANG_FORMAT_BIN}
        ${CHFS_BUILD_SUPPORT_DIR}/clang_format_exclusions.txt
        --source_dirs
        ${CHFS_FORMAT_DIRS}
        --quiet
)

## clang-tidy

# attempt to find the binary if user did not specify
find_program(CLANG_TIDY_BIN
        NAMES clang-tidy clang-tidy-14
        HINTS ${CHFS_CLANG_SEARCH_PATH})

set(LAB1A_FILES
        "${PROJECT_SOURCE_DIR}/src/include/common/*.h"
        "${PROJECT_SOURCE_DIR}/src/include/block/*.h"
        "${PROJECT_SOURCE_DIR}/src/block/*.cc"
        "${PROJECT_SOURCE_DIR}/src/include/metadata/*.h"
        "${PROJECT_SOURCE_DIR}/src/include/metadata/*.cc"        
        ${LAB1A_FILES}
)

set(LAB2A_FILES
        "${PROJECT_SOURCE_DIR}/src/include/librpc/*.h"
        "${PROJECT_SOURCE_DIR}/src/librpc/*.cc"
        ${LAB1A_FILES}
)

set(LAB2B_FILES
        "${PROJECT_SOURCE_DIR}/src/include/distributed/*.h"
        "${PROJECT_SOURCE_DIR}/src/distributed/*.cc"
        ${LAB2A_FILES}
)

add_custom_target(check-clang-tidy-lab2b
        ${CHFS_BUILD_SUPPORT_DIR}/run_clang_tidy.py # run LLVM's clang-tidy script
        -clang-tidy-binary ${CLANG_TIDY_BIN} # using our clang-tidy binary
        -p ${CMAKE_BINARY_DIR} # using cmake's generated compile commands
        -checks=-*,clang-diagnostic-*,-clang-diagnostic-unused-value,clang-analyzer-*,-*,bugprone-*,performance-*,readability-*,-readability-magic-numbers,-readability-braces-around-statements,-readability-inconsistent-declaration-parameter-name,-readability-named-parameter
        ${LAB2B_FILES}
)

add_custom_target(clean-fs
        rm -rf /tmp/inode_* /tmp/data_block_* /tmp/meta_block* /tmp/test_* /tmp/chfs.log
)     