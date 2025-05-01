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
  // UNIMPLEMENTED();
  u8 arg_type;
  
  if (type == FileType::REGULAR) {
    arg_type = RegularFileType;
  } else if (type == FileType::DIRECTORY) {
    arg_type = DirectoryType;
  } else {
    return ChfsResult<inode_id_t>(ErrorType::INVALID_ARG);
  }
  auto res = metadata_server_->call("mknode", arg_type, parent, name);
  if (res.is_err()) {
    return ChfsResult<inode_id_t>(res.unwrap_error());
  } 
  return ChfsResult<inode_id_t>(res.unwrap()->as<inode_id_t>());
}

// {Your code here}
auto ChfsClient::unlink(inode_id_t parent, std::string const &name)
    -> ChfsNullResult {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = metadata_server_->call("unlink", parent, name);
  if (res.is_err()) {
    return ChfsNullResult(res.unwrap_error());
  } 

  return KNullOk;
}

// {Your code here}
auto ChfsClient::lookup(inode_id_t parent, const std::string &name)
    -> ChfsResult<inode_id_t> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = metadata_server_->call("unlink", parent, name);
  if (res.is_err()) {
    return ChfsResult<inode_id_t>(res.unwrap_error());
  } 

  return ChfsResult<inode_id_t>(res.unwrap()->as<inode_id_t>());
}

// {Your code here}
auto ChfsClient::readdir(inode_id_t id)
    -> ChfsResult<std::vector<std::pair<std::string, inode_id_t>>> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = metadata_server_->call("readdir", id);
  if (res.is_err()) {
    return ChfsResult<std::vector<std::pair<std::string, inode_id_t>>>(res.unwrap_error());
  } 

  return ChfsResult<std::vector<std::pair<std::string, inode_id_t>>>(res.unwrap()->as<std::vector<std::pair<std::string, inode_id_t>>>());
}

// {Your code here}
auto ChfsClient::get_type_attr(inode_id_t id)
    -> ChfsResult<std::pair<InodeType, FileAttr>> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = metadata_server_->call("get_type_attr", id);
  if (res.is_err()) {
    return ChfsResult<std::pair<InodeType, FileAttr>>(res.unwrap_error());
  } 
  auto type_attr = res.unwrap()->as<std::tuple<u64, u64, u64, u64, u8>>();
  InodeType itype = std::get<4>(type_attr) == RegularFileType ? InodeType::FILE : InodeType::Directory;
  FileAttr attr;
  attr.size = std::get<0>(type_attr);
  attr.atime = std::get<1>(type_attr);
  attr.mtime = std::get<2>(type_attr);
  attr.ctime = std::get<3>(type_attr);

  return ChfsResult<std::pair<InodeType, FileAttr>>({itype, attr});
}

/**
 * Read and Write operations are more complicated.
 */
// {Your code here}
auto ChfsClient::read_file(inode_id_t id, usize offset, usize size)
    -> ChfsResult<std::vector<u8>> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto get_res = metadata_server_->call("get_block_map", id);
  if (get_res.is_err()) {
    return ChfsResult<std::vector<u8>>(get_res.unwrap_error());
  }
  auto block_info_vec = get_res.unwrap()->as<std::vector<BlockInfo>>();
  int tot_blocks = block_info_vec.size();

  // 边界
  std::vector<u8> buffer;
  size = std::min(size, tot_blocks * BLOCK_SIZE - offset);
  buffer.reserve(size);

  while (size > 0) {
    auto n = std::min(size, BLOCK_SIZE - offset % BLOCK_SIZE);
    auto [bid, mid, version] = block_info_vec[offset / BLOCK_SIZE];
    auto read_res = data_servers_[mid]->call("read_data", bid, offset % BLOCK_SIZE, n, version);
    if (read_res.is_err()) {
      return ChfsResult<std::vector<u8>>(read_res.unwrap_error());
    }
    auto data = read_res.unwrap()->as<std::vector<u8>>();
    std::copy(data.begin(), data.end(), std::back_inserter(buffer));

    size -= n;
    offset += n;
  }

  return ChfsResult<std::vector<u8>>(buffer);
}

// {Your code here}
auto ChfsClient::write_file(inode_id_t id, usize offset, std::vector<u8> data)
    -> ChfsNullResult {
  // TODO: Implement this function.
  // UNIMPLEMENTED(); 
  auto get_res = metadata_server_->call("get_block_map", id);
  if (get_res.is_err()) {
    return ChfsNullResult(get_res.unwrap_error());
  }
  auto block_info_vec = get_res.unwrap()->as<std::vector<BlockInfo>>();
  int tot_blocks = block_info_vec.size();

  {
    // 按需扩容文件
    usize n = data.size();
    BlockInfo binfo;
    while (tot_blocks * BLOCK_SIZE < offset + n) {
      auto alloc_res = metadata_server_->call("alloc_block", id);
      if (alloc_res.is_err()) {
        return ChfsNullResult(alloc_res.unwrap_error());
      }
      if (std::get<0>(binfo = alloc_res.unwrap()->as<BlockInfo>()) == KInvalidBlockID) {
        return ChfsNullResult(ErrorType::OUT_OF_RESOURCE);
      }

      block_info_vec.push_back(binfo);
      ++tot_blocks;
    }
  }
  // 边界
  std::vector<u8> buffer(BLOCK_SIZE);
  usize idx = 0;
  usize target = data.size();

  while (idx < target) {
    auto n = std::min(target - idx, BLOCK_SIZE - offset % BLOCK_SIZE);
    buffer.resize(n);
    std::copy(data.begin() + idx, data.begin() + idx + n, buffer.begin());
    auto [bid, mid, version] = block_info_vec[offset / BLOCK_SIZE];
    // NOTE: 可能存在中间块被释放的情况，但是目前没有方式指定块索引分配块
    // free_file_block使用时不能释放中间块
    auto write_res = data_servers_[mid]->call("write_data", bid, offset % BLOCK_SIZE, buffer);
    if (write_res.is_err()) {
      return ChfsNullResult(write_res.unwrap_error());
    }
    if (write_res.unwrap()->as<bool>() == false) {
      return ChfsNullResult(ErrorType::DONE); 
    }
    idx += n;
    offset += n;
  }

  return KNullOk;
}

// {Your code here}
auto ChfsClient::free_file_block(inode_id_t id, block_id_t block_id,
                                 mac_id_t mac_id) -> ChfsNullResult {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = metadata_server_->call("free_block", id, block_id, mac_id);
  if (res.is_err()) {
    return ChfsNullResult(res.unwrap_error());
  } 

  return KNullOk;
}

} // namespace chfs