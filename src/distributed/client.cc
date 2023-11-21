#include "distributed/client.h"
#include "common/macros.h"
#include "common/util.h"
#include "distributed/metadata_server.h"

namespace chfs {

ChfsClient::ChfsClient() : num_data_servers(0) {}

auto ChfsClient::reg_server(ServerType type, const std::string &address,
                            u16 port, bool reliable) -> ChfsNullResult {
  switch (type) {
  case ServerType::DATA_SERVER:
    num_data_servers += 1;
    data_servers_.insert({num_data_servers, std::make_shared<RpcClient>(
                                                address, port, reliable)});
    break;
  case ServerType::METADATA_SERVER:
    metadata_server_ = std::make_shared<RpcClient>(address, port, reliable);
    break;
  default:
    std::cerr << "Unknown Type" << std::endl;
    exit(1);
  }

  return KNullOk;
}

// {Your code here}
auto ChfsClient::mknode(FileType type, inode_id_t parent,
                        const std::string &name) -> ChfsResult<inode_id_t> {
  // TODO: Implement this function.
//  UNIMPLEMENTED();
  if(type == FileType::REGULAR){
      auto reg_res = metadata_server_->call("mknode",RegularFileType,parent,name);
      if(reg_res.is_ok()){
          auto reg_inode_id = reg_res.unwrap()->as<inode_id_t>();
          if(reg_inode_id == 0){
              return ChfsResult<inode_id_t>(ErrorType::AlreadyExist);
          }
          else{
              return ChfsResult<inode_id_t>(reg_inode_id);
          }
      }
  }
  else if(type == FileType::DIRECTORY){
      auto dir_res = metadata_server_->call("mknode",DirectoryType,parent,name);
      if(dir_res.is_ok()){
          auto dir_inode_id = dir_res.unwrap()->as<inode_id_t>();
          if(dir_inode_id == 0){
              return ChfsResult<inode_id_t>(ErrorType::AlreadyExist);
          }
          else{
              return ChfsResult<inode_id_t>(dir_inode_id);
          }
      }
  }

  return ChfsResult<inode_id_t>(0);
}

// {Your code here}
auto ChfsClient::unlink(inode_id_t parent, std::string const &name)
    -> ChfsNullResult {
  // TODO: Implement this function.
  auto res = metadata_server_->call("unlink",parent,name);

  if(res.is_ok()){
      if(res.unwrap()->as<bool>()){
          return KNullOk;
      }
      else{
          return ChfsNullResult (ErrorType::NotExist);
      }
  }
//  UNIMPLEMENTED();
return KNullOk;
}

// {Your code here}
auto ChfsClient::lookup(inode_id_t parent, const std::string &name)
    -> ChfsResult<inode_id_t> {
  // TODO: Implement this function.
//  UNIMPLEMENTED();
  auto res = metadata_server_->call("lookup",parent,name);

  if(res.is_ok()){
      auto inode_id = res.unwrap()->as<inode_id_t>();
      return ChfsResult<inode_id_t>(inode_id);
  }

  return ChfsResult<inode_id_t>(0);
}

// {Your code here}
auto ChfsClient::readdir(inode_id_t id)
    -> ChfsResult<std::vector<std::pair<std::string, inode_id_t>>> {
  // TODO: Implement this function.
  auto res = metadata_server_->call("readdir",id);

  if(res.is_ok()){
      auto list = res.unwrap()->as<std::vector<std::pair<std::string, inode_id_t>>>();
      return ChfsResult<std::vector<std::pair<std::string, inode_id_t>>>(list);
  }

//  UNIMPLEMENTED();
  return ChfsResult<std::vector<std::pair<std::string, inode_id_t>>>({});
}

// {Your code here}
auto ChfsClient::get_type_attr(inode_id_t id)
    -> ChfsResult<std::pair<InodeType, FileAttr>> {
  // TODO: Implement this function.
  auto res = metadata_server_->call("get_type_attr",id);

  if(res.is_ok()){
      auto tuple=
              res.unwrap()->as<std::tuple<u64, u64, u64, u64, u8>>();

      FileAttr attr;
      attr.atime = std::get<0>(tuple);
      attr.mtime = std::get<1>(tuple);
      attr.ctime = std::get<2>(tuple);
      attr.size = std::get<3>(tuple);
      InodeType type = (std::get<4>(tuple) == RegularFileType) ? InodeType::Regular : InodeType::Directory;
      auto result = std::make_pair(type,attr);
      return ChfsResult<std::pair<InodeType, FileAttr>>(result);
  }
//  UNIMPLEMENTED();
  return ChfsResult<std::pair<InodeType, FileAttr>>({});
}

/**
 * Read and Write operations are more complicated.
 */
// {Your code here}
auto ChfsClient::read_file(inode_id_t id, usize offset, usize size)
    -> ChfsResult<std::vector<u8>> {
  // TODO: Implement this function.
    std::vector<u8> result;

    auto block_map =
            metadata_server_->call("get_block_map",id).unwrap()->as<std::vector<BlockInfo>>();

    auto total_bytes = offset + size;
    auto total_block_num = total_bytes % DiskBlockSize==0 ? (total_bytes/DiskBlockSize) : (total_bytes/DiskBlockSize + 1);

    if(block_map.size() <  total_block_num){
        return ChfsResult<std::vector<u8>>({});
    }

    auto offset_block_index = offset / DiskBlockSize;
    auto block_offset = offset % DiskBlockSize;

    usize total_sz = size;
    usize read_sz = 0;

    for(usize i = offset_block_index; i < total_block_num ; i++){

        if( (DiskBlockSize - block_offset) >= total_sz ){
            block_id_t bid = std::get<0>(block_map[offset_block_index]);
            mac_id_t mid = std::get<1>(block_map[offset_block_index]);
            version_t version = std::get<2>(block_map[offset_block_index]);

            auto content =
                    data_servers_[mid]->call("read_data",bid,block_offset,total_sz,version).unwrap()->as<std::vector<u8>>();

            result.insert(result.end(),content.begin(),content.begin()+total_sz);
            return ChfsResult<std::vector<u8>>(result);
        }

        usize sz = 0;
        if(i == offset_block_index){
            sz = DiskBlockSize-block_offset;
        }
        else{
            sz = ((total_sz - read_sz) > DiskBlockSize)
                 ? DiskBlockSize
                 : (total_sz - read_sz);
        }

        block_id_t bid = std::get<0>(block_map[i]);
        mac_id_t mid = std::get<1>(block_map[i]);
        version_t version = std::get<2>(block_map[i]);

        auto content =
                data_servers_[mid]->call("read_data",bid,0,sz,version).unwrap()->as<std::vector<u8>>();

        result.insert(result.end(),content.begin(),content.begin()+sz);

        read_sz += sz;

        if(read_sz >= total_sz){
            break;
        }
    }

    return ChfsResult<std::vector<u8>>(result);
//  UNIMPLEMENTED();
  return ChfsResult<std::vector<u8>>({});
}

// {Your code here}
auto ChfsClient::write_file(inode_id_t id, usize offset, std::vector<u8> data)
    -> ChfsNullResult {
  // TODO: Implement this function.
    auto block_map =
          metadata_server_->call("get_block_map",id).unwrap()->as<std::vector<BlockInfo>>();



    auto total_bytes = offset + data.size();
    auto total_block_num = total_bytes % DiskBlockSize==0 ? (total_bytes/DiskBlockSize) : (total_bytes/DiskBlockSize + 1);

    while(block_map.size() < total_block_num){
        auto block_info =
                metadata_server_->call("alloc_block",id).unwrap()->as<BlockInfo>();
        block_map.push_back(block_info);
    }

    auto offset_block_index = offset / DiskBlockSize;
    auto block_offset = offset % DiskBlockSize;

    usize total_sz = data.size();
    usize write_sz = 0;

    for(usize i = offset_block_index; i < total_block_num ; i++){

        if( (DiskBlockSize - block_offset) >= total_sz ){
            block_id_t bid = std::get<0>(block_map[offset_block_index]);
            mac_id_t mid = std::get<1>(block_map[offset_block_index]);
            data_servers_[mid]->call("write_data",bid,block_offset,data);

            return KNullOk;
        }

        usize sz = 0;
        if(i == offset_block_index){
            sz = DiskBlockSize-block_offset;
        }
        else{
            sz = ((total_sz - write_sz) > DiskBlockSize)
                 ? DiskBlockSize
                 : (total_sz - write_sz);
        }

        block_id_t bid = std::get<0>(block_map[i]);
        mac_id_t mid = std::get<1>(block_map[i]);

        std::vector<u8> content;
        content.insert(content.end(),data.begin()+write_sz,
                       data.begin()+write_sz+sz);

        data_servers_[mid]->call("write_data",bid,0,content);

        write_sz += sz;

        if(write_sz >= total_sz){
            break;
        }
    }

//  UNIMPLEMENTED();
  return KNullOk;
}

// {Your code here}
auto ChfsClient::free_file_block(inode_id_t id, block_id_t block_id,
                                 mac_id_t mac_id) -> ChfsNullResult {
  // TODO: Implement this function.
  auto res = metadata_server_->call("free_block",id,block_id,mac_id);

  if(res.is_ok() && res.unwrap()->as<bool>()){
      return KNullOk;
  }

//  UNIMPLEMENTED();
  return KNullOk;
}

} // namespace chfs