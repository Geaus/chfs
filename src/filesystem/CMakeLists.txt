add_library(
  chfs_fs
  OBJECT
  control_op.cc
  data_op.cc 
  directory_op.cc
)

set(ALL_OBJECT_FILES
  ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:chfs_fs>
  PARENT_SCOPE)