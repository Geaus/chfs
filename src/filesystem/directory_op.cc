#include <algorithm>
#include <sstream>

#include "filesystem/directory_op.h"

namespace chfs {

/**
 * Some helper functions
 */
auto string_to_inode_id(std::string &data) -> inode_id_t {
  std::stringstream ss(data);
  inode_id_t inode;
  ss >> inode;
  return inode;
}

auto inode_id_to_string(inode_id_t id) -> std::string {
  std::stringstream ss;
  ss << id;
  return ss.str();
}

// {Your code here}
auto dir_list_to_string(const std::list<DirectoryEntry> &entries)
    -> std::string {
  std::ostringstream oss;
  usize cnt = 0;
  for (const auto &entry : entries) {
    oss << entry.name << ':' << entry.id;
    if (cnt < entries.size() - 1) {
      oss << '/';
    }
    cnt += 1;
  }
  return oss.str();
}

// {Your code here}
auto append_to_directory(std::string src, std::string filename, inode_id_t id)
    -> std::string {

  // TODO: Implement this function.
  //       Append the new directory entry to `src`.
  std::ostringstream oss;
  oss << src;
  if(src== ""){
      oss << filename << ':' << inode_id_to_string(id);
  }
  else{
      oss << '/' << filename << ':' << inode_id_to_string(id);
  }

  src = oss.str();
//  std::cerr<<src<<" ";
//  UNIMPLEMENTED();
  
  return src;
}

// {Your code here}
void parse_directory(std::string &src, std::list<DirectoryEntry> &list) {

  // TODO: Implement this function.
    std::istringstream iss(src);
    std::string line;

    while (std::getline(iss, line, '/')) {
        std::istringstream iss_line(line);
        std::string name;
        std::string id_str;
        if (std::getline(iss_line, name, ':') && std::getline(iss_line, id_str)) {
            DirectoryEntry entry{name, string_to_inode_id(id_str)};
            list.push_back(entry);
        }
    }

//  UNIMPLEMENTED();

}

// {Your code here}
auto rm_from_directory(std::string src, std::string filename) -> std::string {

  auto res = std::string("");

  // TODO: Implement this function.
  //       Remove the directory entry from `src`.
  std::list<DirectoryEntry> list;
  list.clear();

  parse_directory(src,list);
  for (auto it = list.begin(); it != list.end(); ++it) {
      if (it->name == filename) {
            list.erase(it);
            break;
      }
  }

  res = dir_list_to_string(list);
//  UNIMPLEMENTED();
//  std::cerr<<res;

  return res;
}

/**
 * { Your implementation here }
 */
auto read_directory(FileOperation *fs, inode_id_t id,
                    std::list<DirectoryEntry> &list) -> ChfsNullResult {
  
  // TODO: Implement this function.

  auto content = fs->read_file(id).unwrap();
  std::string str(reinterpret_cast<char*>(content.data()), content.size());
  parse_directory(str,list);

//  UNIMPLEMENTED();
  return KNullOk;
}

// {Your code here}
auto FileOperation::lookup(inode_id_t id, const char *name)
    -> ChfsResult<inode_id_t> {
  std::list<DirectoryEntry> list;

  // TODO: Implement this function.
  read_directory(this,id,list);
  for (auto it = list.begin(); it != list.end(); ++it) {
      if (it->name == name) {
            return ChfsResult<inode_id_t>(it->id);
      }
  }
//  UNIMPLEMENTED();

  return ChfsResult<inode_id_t>(ErrorType::NotExist);
}

// {Your code here}
auto FileOperation::mk_helper(inode_id_t id, const char *name, InodeType type)
    -> ChfsResult<inode_id_t> {

  // TODO:
  // 1. Check if `name` already exists in the parent.
  //    If already exist, return ErrorType::AlreadyExist.
  // 2. Create the new inode.
  // 3. Append the new entry to the parent directory.
  std::list<DirectoryEntry> list;

  auto res = lookup(id,name);
  if(res.is_ok()){
      return ChfsResult<inode_id_t>(ErrorType::AlreadyExist);
  }

  auto new_node_id = alloc_inode(type).unwrap();
//  std::cerr<<"node"<<new_node_id<<" ";

  read_directory(this,id,list);
  std::string content = dir_list_to_string(list);
  content=append_to_directory(content,name,new_node_id);

//  std::vector <u8> vec(content.size());
//  std::cerr<<content.size();
//  for(int i=0;i < content.size();i++){
//      vec[i]=content[i];
//  }
//  vec.assign(content.begin(),content.end());
  std::vector<u8> vec(content.begin(), content.end());

  this->write_file(id,vec);
//  UNIMPLEMENTED();

  return ChfsResult<inode_id_t>(new_node_id);
}

// {Your code here}
auto FileOperation::unlink(inode_id_t parent, const char *name)
    -> ChfsNullResult {

  // TODO: 
  // 1. Remove the file, you can use the function `remove_file`
  // 2. Remove the entry from the directory.

  std::string content;
  std::list<DirectoryEntry> list;
  auto res = lookup(parent,name);
  if(res.is_ok()){
      remove_file(res.unwrap());
  }

  read_directory(this,parent,list);
  content = dir_list_to_string(list);
  std::string new_directory =  rm_from_directory(content,name);
  std::vector<u8> vec(new_directory.begin(), new_directory.end());
  this->write_file(parent,vec);

//  UNIMPLEMENTED();
  
  return KNullOk;
}

} // namespace chfs
