add_library(
  chfs_metadata
  OBJECT
  superblock.cc
  manager.cc
  inode.cc
)

set(ALL_OBJECT_FILES
  ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:chfs_metadata>
  PARENT_SCOPE)