#include <algorithm>

#include "common/bitmap.h"
#include "distributed/commit_log.h"
#include "distributed/metadata_server.h"
#include "filesystem/directory_op.h"
#include "metadata/inode.h"
// #include "metadata/superblock.h"
#include <chrono>

namespace chfs {
/**
 * `CommitLog` part
 */
// {Your code here}
CommitLog::CommitLog(std::shared_ptr<BlockManager> bm,
                     bool is_checkpoint_enabled)
    : is_checkpoint_enabled_(is_checkpoint_enabled), bm_(bm) {
      // super_block_ = std::make_shared<// superBlock>(bm, 0);
      // super_block_->inner.log_entry_offset = 0;
      // super_block_->inner.log_update_block_offset = 0;
}

CommitLog::~CommitLog() {}

// {Your code here}
auto CommitLog::get_log_entry_num() -> usize {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  return log_entry_num_;
}

// {Your code here}
auto CommitLog::append_log(txn_id_t txn_id,
                           std::vector<std::shared_ptr<BlockOperation>> ops)
    -> void {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  assert (log_entries_.count(txn_id) == 0);
  log_entries_[txn_id].second = ops;

  // 写入更新块
  // 构造log entry
  auto & entry = log_entries_[txn_id].first;
  entry.txn_id = txn_id;
  // entry.update_block_begin = // super_block_->inner.log_update_block_offset;
  entry.update_block_end = entry.update_block_begin;
  int i = 0;
  for (auto & op : ops) {
    entry.update_blocks[i++] = op->block_id_;
    block_id_t bid = 1 + (LOG_ENTRY_ARR_SIZE + entry.update_block_end) / bm_->block_size();
    bm_->write_block_safe(bid, op->new_block_state_.data());
    assert(bm_->sync(bid).is_ok());
    ++entry.update_block_end;
  }
  if (entry.update_block_end >= TOT_LOG_REGION_BLOCK) {
    std::cerr << entry.update_block_end << " update block overflow" << std::endl;
    assert(false);
  }
  // super_block_->inner.log_update_block_offset = entry.update_block_end;
  // flush super block
  // super_block_->flush(0);
  assert(bm_->sync(0).is_ok());
}

// {Your code here}
auto CommitLog::commit_log(txn_id_t txn_id) -> void {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto & entry = log_entries_[txn_id].first;
  usize tot_size = sizeof(entry) + (entry.update_block_end - entry.update_block_begin) * sizeof(block_id_t);
  // if (super_block_->inner.log_entry_offset + tot_size >= LOG_ENTRY_ARR_SIZE) {
    // std::cerr << super_block_->inner.log_entry_offset + tot_size << " update log entry overflow" << std::endl;
    // assert(false);
  // }
  std::vector<u8> log_buffer(tot_size);
  // std::memcpy(log_buffer.data(), &entry, sizeof(entry));
  // std::memcpy(log_buffer.data() + sizeof(entry), entry.update_blocks, tot_size - sizeof(entry));

  // u64 start = 1 * bm_->block_size() + super_block_->inner.log_entry_offset; // super block.
  // u64 end = 1 * bm_->block_size() + (super_block_->inner.log_entry_offset + tot_size);
  // block_id_t bid = 0;
  // std::vector<u8> buffer(bm_->block_size());
  // u64 n = 0;
  // u64 idx = 0;
  // while (start < end) {
  //   bid = start / bm_->block_size();
  //   n = bm_->block_size() - start % bm_->block_size();
  //   std::memcpy(buffer.data(), log_buffer.data() + idx, n);
  //   bm_->write_partial_block_safe(bid, buffer.data(), start % bm_->block_size(), n);
  //   assert(bm_->sync(bid).is_ok());
  //   start += n;
  //   idx += n;
  // }
  // super_block_->inner.log_entry_offset += tot_size;
  // // flush super block
  // super_block_->flush(0);
  assert(bm_->sync(0).is_ok());
}

// {Your code here}
auto CommitLog::checkpoint() -> void {
  // TODO: Implement this function.
  UNIMPLEMENTED();
}

// {Your code here}
auto CommitLog::recover() -> void {
  // TODO: Implement this function.
  UNIMPLEMENTED();
}
}; // namespace chfs