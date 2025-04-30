#include "distributed/dataserver.h"
#include "common/util.h"
#include "metadata/inode.h"

namespace chfs {

auto DataServer::initialize(std::string const &data_path) {
  /**
   * At first check whether the file exists or not.
   * If so, which means the distributed chfs has
   * already been initialized and can be rebuilt from
   * existing data.
   */
  bool is_initialized = is_file_exist(data_path);

  auto bm = std::shared_ptr<BlockManager>(
      new BlockManager(data_path, KDefaultBlockCnt));
  version_block_cnt = bm->total_blocks() / (bm->block_size() / sizeof(version_t));
  if (is_initialized) {
    block_allocator_ =
        std::make_shared<BlockAllocator>(bm, version_block_cnt, false);
  } else {
    // We need to reserve some blocks for storing the version of each block
    block_allocator_ = std::shared_ptr<BlockAllocator>(
        new BlockAllocator(bm, version_block_cnt, true));
    // 初始化版本号为0
    for (block_id_t bid = 0; bid < version_block_cnt; ++bid) {
      bm->zero_block(bid);
    }
  }

  // Initialize the RPC server and bind all handlers
  server_->bind("read_data", [this](block_id_t block_id, usize offset,
                                    usize len, version_t version) {
    return this->read_data(block_id, offset, len, version);
  });
  server_->bind("write_data", [this](block_id_t block_id, usize offset,
                                     std::vector<u8> &buffer) {
    return this->write_data(block_id, offset, buffer);
  });
  server_->bind("alloc_block", [this]() { return this->alloc_block(); });
  server_->bind("free_block", [this](block_id_t block_id) {
    return this->free_block(block_id);
  });

  // Launch the rpc server to listen for requests
  server_->run(true, num_worker_threads);
}

DataServer::DataServer(u16 port, const std::string &data_path)
    : server_(std::make_unique<RpcServer>(port)) {
  initialize(data_path);
}

DataServer::DataServer(std::string const &address, u16 port,
                       const std::string &data_path)
    : server_(std::make_unique<RpcServer>(address, port)) {
  initialize(data_path);
}

DataServer::~DataServer() { server_.reset(); }

// {Your code here}
auto DataServer::read_data(block_id_t block_id, usize offset, usize len,
                           version_t version) -> std::vector<u8> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  std::vector<u8> buffer(block_allocator_->bm->block_size());
  auto res = block_allocator_->bm->read_block(block_id, buffer.data());
  if (res.is_err()) {
    return {};
  }
  std::copy(buffer.begin() + offset, buffer.begin() + offset + len, buffer.begin());
  buffer.resize(len);

  return buffer;
}

// {Your code here}
auto DataServer::write_data(block_id_t block_id, usize offset,
                            std::vector<u8> &buffer) -> bool {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto len = buffer.size();
  if (len > block_allocator_->bm->block_size() - offset) {
    len = block_allocator_->bm->block_size() - offset;
  }
  auto res = block_allocator_->bm->write_partial_block(block_id, buffer.data(), offset, len);

  return res.is_ok();
}

// {Your code here}
auto DataServer::alloc_block() -> std::pair<block_id_t, version_t> {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  block_id_t bid = KInvalidBlockID;
  version_t version = 0;

  auto res = block_allocator_->allocate();
  if (res.is_err()) {
    return {bid, version};
  }
  bid = res.unwrap();

  // 递增版本号
  version = increment_version(bid);
  assert(version > 0);

  return {bid, version};
}

// {Your code here}
auto DataServer::free_block(block_id_t block_id) -> bool {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto res = block_allocator_->deallocate(block_id);
  if (res.is_err()) {
    return false;
  }
  (void)increment_version(block_id);

  return true;
}

auto DataServer::increment_version(block_id_t block_id) -> version_t {
  version_t version = 0;

  std::vector<version_t> buffer(block_allocator_->bm->block_size() / sizeof(version_t));
  if (block_allocator_->bm->read_block(block_id / (block_allocator_->bm->block_size() / sizeof(version_t)), reinterpret_cast<u8*>(buffer.data())).is_err()) {
    return 0;
  }
  version = ++buffer[block_id % (block_allocator_->bm->block_size() / sizeof(version_t))];
  if (block_allocator_->bm->write_block(block_id / (block_allocator_->bm->block_size() / sizeof(version_t)), reinterpret_cast<u8*>(buffer.data())).is_err()) {
    return 0;
  }
  return version;
}
} // namespace chfs