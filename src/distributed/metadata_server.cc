#include "distributed/metadata_server.h"
#include "common/util.h"
#include "filesystem/directory_op.h"
#include <fstream>

namespace chfs {

inline auto MetadataServer::bind_handlers() {
  server_->bind("mknode",
                [this](u8 type, inode_id_t parent, std::string const &name) {
                  return this->mknode(type, parent, name);
                });
  server_->bind("unlink", [this](inode_id_t parent, std::string const &name) {
    return this->unlink(parent, name);
  });
  server_->bind("lookup", [this](inode_id_t parent, std::string const &name) {
    return this->lookup(parent, name);
  });
  server_->bind("get_block_map",
                [this](inode_id_t id) { return this->get_block_map(id); });
  server_->bind("alloc_block",
                [this](inode_id_t id) { return this->allocate_block(id); });
  server_->bind("free_block",
                [this](inode_id_t id, block_id_t block, mac_id_t machine_id) {
                  return this->free_block(id, block, machine_id);
                });
  server_->bind("readdir", [this](inode_id_t id) { return this->readdir(id); });
  server_->bind("get_type_attr",
                [this](inode_id_t id) { return this->get_type_attr(id); });
}

inline auto MetadataServer::init_fs(const std::string &data_path) {
  /**
   * Check whether the metadata exists or not.
   * If exists, we wouldn't create one from scratch.
   */
  bool is_initialed = is_file_exist(data_path);

  auto block_manager = std::shared_ptr<BlockManager>(nullptr);
  if (is_log_enabled_) {
    block_manager =
        std::make_shared<BlockManager>(data_path, KDefaultBlockCnt, true);
  } else {
    block_manager = std::make_shared<BlockManager>(data_path, KDefaultBlockCnt);
  }

  CHFS_ASSERT(block_manager != nullptr, "Cannot create block manager.");

  if (is_initialed) {
    auto origin_res = FileOperation::create_from_raw(block_manager);
    std::cout << "Restarting..." << std::endl;
    if (origin_res.is_err()) {
      std::cerr << "Original FS is bad, please remove files manually."
                << std::endl;
      exit(1);
    }

    operation_ = origin_res.unwrap();
  } else {
    operation_ = std::make_shared<FileOperation>(block_manager,
                                                 DistributedMaxInodeSupported);
    std::cout << "We should init one new FS..." << std::endl;
    /**
     * If the filesystem on metadata server is not initialized, create
     * a root directory.
     */
    auto init_res = operation_->alloc_inode(InodeType::Directory);
    if (init_res.is_err()) {
      std::cerr << "Cannot allocate inode for root directory." << std::endl;
      exit(1);
    }

    CHFS_ASSERT(init_res.unwrap() == 1, "Bad initialization on root dir.");
  }

  running = false;
  num_data_servers =
      0; // Default no data server. Need to call `reg_server` to add.

  if (is_log_enabled_) {
    if (may_failed_)
      operation_->block_manager_->set_may_fail(true);
    commit_log = std::make_shared<CommitLog>(operation_->block_manager_,
                                             is_checkpoint_enabled_);
  }

  bind_handlers();

  /**
   * The metadata server wouldn't start immediately after construction.
   * It should be launched after all the data servers are registered.
   */
}

MetadataServer::MetadataServer(u16 port, const std::string &data_path,
                               bool is_log_enabled, bool is_checkpoint_enabled,
                               bool may_failed)
    : is_log_enabled_(is_log_enabled), may_failed_(may_failed),
      is_checkpoint_enabled_(is_checkpoint_enabled) {
  server_ = std::make_unique<RpcServer>(port);
  init_fs(data_path);
  if (is_log_enabled_) {
    commit_log = std::make_shared<CommitLog>(operation_->block_manager_,
                                             is_checkpoint_enabled);
  }
}

MetadataServer::MetadataServer(std::string const &address, u16 port,
                               const std::string &data_path,
                               bool is_log_enabled, bool is_checkpoint_enabled,
                               bool may_failed)
    : is_log_enabled_(is_log_enabled), may_failed_(may_failed),
      is_checkpoint_enabled_(is_checkpoint_enabled) {
  server_ = std::make_unique<RpcServer>(address, port);
  init_fs(data_path);
  if (is_log_enabled_) {
    commit_log = std::make_shared<CommitLog>(operation_->block_manager_,
                                             is_checkpoint_enabled);
  }
}

// {Your code here}
auto MetadataServer::mknode(u8 type, inode_id_t parent, const std::string &name)
    -> inode_id_t {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  InodeType itype = InodeType::Unknown;
  if (type == RegularFileType) {
    itype = InodeType::FILE;
  } else if (type == DirectoryType) {
    itype = InodeType::Directory;
  }
  
  if (itype == InodeType::Unknown) {
    return KInvalidInodeID;
  }

  auto res = operation_->mk_helper(parent, name.c_str(), itype);
  if (res.is_err()) {
    return KInvalidInodeID;
  }
  auto f = operation_->lookup(parent, name.c_str());
  assert (f.unwrap() != 0);
  return res.unwrap();
}

// {Your code here}
auto MetadataServer::unlink(inode_id_t parent, const std::string &name)
    -> bool {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto lookup_res = operation_->lookup(parent, name.c_str());
  if (lookup_res.is_err()) {
    return false;
  }
  operation_->inode_manager_->free_inode(lookup_res.unwrap());
  auto file_block_vec = get_block_map(lookup_res.unwrap());
  for (const auto & info : file_block_vec) {
    clients_[std::get<1>(info)]->call("free_block", std::get<0>(info));
  }
  auto read_res = operation_->read_file(parent);
  if (read_res.is_err()) {
    return false;
  }
  auto src = std::string(reinterpret_cast<char *>(read_res.unwrap().data()), read_res.unwrap().size());
  src = rm_from_directory(src, name);
  std::vector<u8> buffer(src.length());
  memcpy(buffer.data(), src.c_str(), src.length());

  return operation_->write_file(parent, buffer).is_ok();
}

// {Your code here}
auto MetadataServer::lookup(inode_id_t parent, const std::string &name)
    -> inode_id_t {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = operation_->lookup(parent, name.c_str());
  if (res.is_err()) {
    return KInvalidInodeID;
  }
  return res.unwrap();
}

// {Your code here}
auto MetadataServer::get_block_map(inode_id_t id) -> std::vector<BlockInfo> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  std::vector<BlockInfo> blockInfo;
  std::vector<u8> buffer(operation_->block_manager_->block_size());
  auto res = operation_->inode_manager_->read_inode(id, buffer);
  if (res.is_err()) {
    return {};
  }
  Inode * inode_p = reinterpret_cast<Inode *>(buffer.data());
  BlockInfo * blockInfo_p = reinterpret_cast<BlockInfo *>(inode_p->blocks);
  auto n = inode_p->get_block_info_num(sizeof(BlockInfo));
  for (int i = 0; i < n && std::get<0>(blockInfo_p[i]) != KInvalidBlockID; ++i) {
    blockInfo.push_back(blockInfo_p[i]);
  }
  return blockInfo;
}

// {Your code here}
auto MetadataServer::allocate_block(inode_id_t id) -> BlockInfo {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  std::vector<u8> buffer(operation_->block_manager_->block_size());
  auto read_res = operation_->inode_manager_->read_inode(id, buffer);
  if (read_res.is_err()) {
    return {};
  }
  Inode * inode_p = reinterpret_cast<Inode *>(buffer.data());
  BlockInfo * blockInfo_p = reinterpret_cast<BlockInfo *>(inode_p->blocks);
  auto n = inode_p->get_block_info_num(sizeof(BlockInfo));
  int i = 0;
  while (i < n && std::get<0>(blockInfo_p[i]) != KInvalidBlockID) {
    ++i;
  }
  if (i == n) {
    return {};
  }

  // 选择一个data server 
  auto iter = clients_.begin();
  std::advance(iter, generator.rand(0, num_data_servers - 1)); 
  auto cli = iter->second;
  auto alloc_res = cli->call("alloc_block");
  if (alloc_res.is_err()) {
    return {};
  }
  auto [block_id, version] =
      alloc_res.unwrap()->as<std::pair<block_id_t, version_t>>();
  blockInfo_p[i] = {block_id, iter->first, version};
  auto wb_res = operation_->block_manager_->write_block(read_res.unwrap(), buffer.data());
  if (wb_res.is_err()) {
    return {};
  }
  return {block_id, iter->first, version};
}


// {Your code here}
auto MetadataServer::free_block(inode_id_t id, block_id_t block_id,
                                mac_id_t machine_id) -> bool {
  // TODO: Implement this function.
  UNIMPLEMENTED();

  return false;
}

// {Your code here}
auto MetadataServer::readdir(inode_id_t node)
    -> std::vector<std::pair<std::string, inode_id_t>> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto read_res = operation_->read_file(node);
  if (read_res.is_err()) {
    return {};
  }
  std::list<DirectoryEntry> list;
  auto content = std::string(reinterpret_cast<char *>(read_res.unwrap().data()), read_res.unwrap().size());
  parse_directory(content, list);
  std::vector<std::pair<std::string, inode_id_t>> res;
  res.reserve(list.size());
  for (auto & entry : list) {
    res.push_back({entry.name, entry.id});
  }

  return res;
}

// {Your code here}
auto MetadataServer::get_type_attr(inode_id_t id)
    -> std::tuple<u64, u64, u64, u64, u8> {
  // TODO: Implement this function.
  UNIMPLEMENTED();

  return {};
}

auto MetadataServer::reg_server(const std::string &address, u16 port,
                                bool reliable) -> bool {
  num_data_servers += 1;
  auto cli = std::make_shared<RpcClient>(address, port, reliable);
  clients_.insert(std::make_pair(num_data_servers, cli));

  return true;
}

auto MetadataServer::run() -> bool {
  if (running)
    return false;

  // Currently we only support async start
  server_->run(true, num_worker_threads);
  running = true;
  return true;
}

} // namespace chfs