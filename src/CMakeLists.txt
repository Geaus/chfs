add_subdirectory(block)
add_subdirectory(metadata)
add_subdirectory(filesystem)
add_subdirectory(librpc)
add_subdirectory(distributed)

add_library(chfs STATIC ${ALL_OBJECT_FILES})

set(CHFS_THIRDPARTY_LIBS
        fmt
        rpc
        )

target_link_libraries(
        chfs
        ${CHFS_THIRDPARTY_LIBS})
