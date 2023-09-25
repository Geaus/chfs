add_library(
  chfs_block
  OBJECT
  manager.cc
  allocator.cc
)

set(ALL_OBJECT_FILES
  ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:chfs_block>
  PARENT_SCOPE)