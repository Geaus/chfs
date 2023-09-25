set(SFS_SOURCES main.cc)
add_executable(fs ${SFS_SOURCES})

target_link_libraries(fs chfs fuse)