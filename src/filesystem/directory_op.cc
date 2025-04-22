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
  // UNIMPLEMENTED();
  std::ostringstream oss;
  oss << filename << ':' << inode_id_to_string(id);
  if (!src.empty()) {
    src.append("/");
  }
  src.append(oss.str());

  return src;
}

// {Your code here}
void parse_directory(std::string &src, std::list<DirectoryEntry> &list) {

  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto len = src.length();
  std::size_t index = 0;
  std::size_t pos = 0;
  bool flag = false;
  while (index < len && !flag) {
    pos = src.find('/', index);
    if (pos == std::string::npos) {
      pos = src.length();
      flag = true;
    }
    std::string entry_str = src.substr(index, pos - index);
    auto colon = entry_str.find(':');

    DirectoryEntry entry;
    entry.name = entry_str.substr(0, colon);
    std::string id_str = entry_str.substr(colon);
    entry.id = string_to_inode_id(id_str);
    list.push_back(entry);

    index = pos + 1;
  }
}

// {Your code here}
auto rm_from_directory(std::string src, std::string filename) -> std::string {
  // TODO: Implement this function.
  //       Remove the directory entry from `src`.
  // UNIMPLEMENTED();

  // 为了效率，直接在原本的字符串上操作
  auto len = src.length();
  std::size_t index = 0;
  std::size_t pos = 0;
  bool flag = false;
  
  while (index < len && !flag) {
    pos = src.find('/', index);
    if (pos == std::string::npos) {
      pos = len;
      flag = true;
    }
    std::string entry_str = src.substr(index, pos - index);
    auto colon = entry_str.find(':');

    auto name = entry_str.substr(0, colon);

    if (name == filename) {
      std::string prefix = "";
      std::string postfix = "";
      if (index > 0) {
        prefix = src.substr(0, index - 1);
      }
      if (pos < len) {
        postfix = src.substr(pos + ((index == 0) ? 1 : 0));
      }
      src = prefix + postfix;
      flag = true;
    }
    index = pos + 1;
  }

  return src;
}

/**
 * { Your implementation here }
 */
auto read_directory(FileOperation *fs, inode_id_t id,
                    std::list<DirectoryEntry> &list) -> ChfsNullResult {
  
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto read_res = fs->read_file(id);
  if (read_res.is_err()) {
    return ChfsNullResult(read_res.unwrap_error());
  }
  auto content = std::string(reinterpret_cast<char *>(read_res.unwrap().data()), read_res.unwrap().size());
  parse_directory(content, list);

  return KNullOk;
}

// {Your code here}
auto FileOperation::lookup(inode_id_t id, const char *name)
    -> ChfsResult<inode_id_t> {
  std::list<DirectoryEntry> list;

  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto read_res = read_file(id);
  if (read_res.is_err()) {
    return ChfsResult<inode_id_t>(read_res.unwrap_error());
  }
  auto content = std::string(reinterpret_cast<char *>(read_res.unwrap().data()), read_res.unwrap().size());
  parse_directory(content, list);
  for (auto & entry : list) {
    if (entry.name == std::string(name)) {
      return ChfsResult<inode_id_t>(entry.id);
    }
  }

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
  // UNIMPLEMENTED();
  if (lookup(id, name).is_ok()) {
    return ChfsResult<inode_id_t>(ErrorType::AlreadyExist);
  }
  auto read_res = read_file(id);
  if (read_res.is_err()) {
    return ChfsResult<inode_id_t>(read_res.unwrap_error());
  }
  std::string src = std::string(reinterpret_cast<char *>(read_res.unwrap().data()), read_res.unwrap().size());
  src = append_to_directory(src, std::string(name), id);
  std::vector<u8> buffer(src.length());
  memcpy(buffer.data(), src.c_str(), src.length());
  auto ret = write_file(id, buffer);

  if (ret.is_err()) {
    return ChfsResult<inode_id_t>(ret.unwrap_error());
  }

  return ChfsResult<inode_id_t>(static_cast<inode_id_t>(0));
}

// {Your code here}
auto FileOperation::unlink(inode_id_t parent, const char *name)
    -> ChfsNullResult {

  // TODO: 
  // 1. Remove the file, you can use the function `remove_file`
  // 2. Remove the entry from the directory.
  // UNIMPLEMENTED();
  auto lookup_res = lookup(parent, name);
  if (lookup_res.is_err()) {
    return ChfsNullResult(lookup_res.unwrap_error());
  }
  remove_file(lookup_res.unwrap());
  auto read_res = read_file(parent);
  if (read_res.is_err()) {
    return ChfsNullResult(read_res.unwrap_error());
  }
  auto src = std::string(reinterpret_cast<char *>(read_res.unwrap().data()), read_res.unwrap().size());
  src = rm_from_directory(src, name);
  std::vector<u8> buffer(src.length());
  memcpy(buffer.data(), src.c_str(), src.length());

  return write_file(parent, buffer);
}

} // namespace chfs
