add_library(
        chfs_mr
        OBJECT
        basic_mr.cc
        mr_coordinator.cc
        mr_sequential.cc
        mr_worker.cc
)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:chfs_mr>
        PARENT_SCOPE)