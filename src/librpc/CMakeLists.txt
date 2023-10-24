add_library(
  chfs_rpc
  OBJECT
  server.cc
  client.cc
)

set(ALL_OBJECT_FILES
  ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:chfs_rpc>
  PARENT_SCOPE)