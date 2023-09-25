add_subdirectory(block)
add_subdirectory(metadata)
add_subdirectory(filesystem)

add_library(chfs STATIC ${ALL_OBJECT_FILES})

set(CHFS_THIRDPARTY_LIBS
        fmt
        )

target_link_libraries(
        chfs
        ${CHFS_THIRDPARTY_LIBS})
