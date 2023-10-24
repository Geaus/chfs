add_library(
  chfs_distributed
  OBJECT
  dataserver.cc
  metadata_server.cc
  client.cc
  commit_log.cc
)

set(ALL_OBJECT_FILES
  ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:chfs_distributed>
  PARENT_SCOPE)