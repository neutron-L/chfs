#include <algorithm>

#include "common/bitmap.h"
#include "distributed/commit_log.h"
#include "distributed/metadata_server.h"
#include "filesystem/directory_op.h"
#include "metadata/inode.h"
#include "metadata/superblock.h"
#include <chrono>

namespace chfs {
/**
 * `CommitLog` part
 */
// {Your code here}
CommitLog::CommitLog(std::shared_ptr<BlockManager> bm,
                     bool is_checkpoint_enabled)
    : is_checkpoint_enabled_(is_checkpoint_enabled), bm_(bm) {
    superblock_ = std::make_shared<SuperBlock>(bm, 0);
    superblock_->inner.log_entry_offset = 0;
    superblock_->inner.log_update_block_offset = 0;
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
  entry.update_block_begin = 1 + kMaxLogEntryArrSize / bm_->block_size() + superblock_->inner.log_update_block_offset;
  entry.update_block_end = entry.update_block_begin;
  int i = 0;
  for (auto & op : ops) {
    entry.update_blocks[i++] = op->block_id_;
    bm_->write_block_safe(entry.update_block_end, op->new_block_state_.data());
    assert(bm_->sync(entry.update_block_end).is_ok());
    ++entry.update_block_end;
  }
  if (entry.update_block_end > 1 + kMaxLogBlockSize) {
    std::cerr << entry.update_block_end << " update block overflow" << std::endl;
    assert(false);
  }
  superblock_->inner.log_update_block_offset = entry.update_block_end;
  // flush super block
  superblock_->flush(0);
  assert(bm_->sync(0).is_ok());

  write_log_entry(txn_id);
}

// {Your code here}
auto CommitLog::commit_log(txn_id_t txn_id) -> void {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  // 不知道上下文中具体需要做什么，仅仅删除其在内存中的信息
  log_entries_.erase(txn_id);
}

// {Your code here}
auto CommitLog::checkpoint() -> void {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  recover();

  // 重置log区域
  superblock_->inner.log_entry_offset = 0;
  superblock_->inner.log_update_block_offset = 0;
  superblock_->flush(0);
  assert(bm_->sync(0).is_ok());
  log_entry_num_ = 0;
}

// {Your code here}
auto CommitLog::recover() -> void {
  // TODO: Implement this function.
  // UNIMPLEMENTED();
  auto superblock_res = SuperBlock::create_from_existing(bm_, 0);
  assert (superblock_res.is_ok());
  superblock_ = superblock_res.unwrap();

  std::vector<u8> buffer(kMaxLogEntryArrSize);
  LogEntry * log_entry = nullptr;
  u64 idx = 0;

  for (int i = 0; i < kMaxLogEntryArrSize / bm_->block_size(); ++i) {
    bm_->read_block(i + 1, buffer.data() + i * bm_->block_size()); // skip superblock
  }
  std::vector<u8> block_buffer(bm_->block_size());
  while (idx < superblock_->inner.log_entry_offset) {
    log_entry = reinterpret_cast<LogEntry *>(buffer.data() + idx);
    for (int i = 0, j = log_entry->update_block_begin; i < (log_entry->update_block_end - log_entry->update_block_begin); ++i, ++j) {
      bm_->read_block(j, block_buffer.data());
      bm_->write_block_safe(log_entry->update_blocks[i], block_buffer.data());
      bm_->sync(log_entry->update_blocks[i]);
    }
    idx += sizeof(log_entry) + (log_entry->update_block_end - log_entry->update_block_begin) * sizeof(block_id_t);
  }
}

void CommitLog::write_log_entry(txn_id_t txn_id) {
  auto & entry = log_entries_[txn_id].first;
  usize tot_size = sizeof(entry) + (entry.update_block_end - entry.update_block_begin) * sizeof(block_id_t);
  if (superblock_->inner.log_entry_offset + tot_size >= kMaxLogEntryArrSize) {
    std::cerr << superblock_->inner.log_entry_offset + tot_size << " update log entry overflow" << std::endl;
    assert(false);
  }
  std::vector<u8> log_buffer(tot_size);
  std::memcpy(log_buffer.data(), &entry, sizeof(entry));
  std::memcpy(log_buffer.data() + sizeof(entry), entry.update_blocks, tot_size - sizeof(entry));

  u64 start = 1 * bm_->block_size() + superblock_->inner.log_entry_offset; // super block.
  u64 end = 1 * bm_->block_size() + (superblock_->inner.log_entry_offset + tot_size);
  block_id_t bid = 0;
  std::vector<u8> buffer(bm_->block_size());
  u64 n = 0;
  u64 idx = 0;
  while (start < end) {
    bid = start / bm_->block_size();
    n = std::min(end - start, bm_->block_size() - start % bm_->block_size());
    std::memcpy(buffer.data(), log_buffer.data() + idx, n);
    bm_->write_partial_block_safe(bid, buffer.data(), start % bm_->block_size(), n);
    assert(bm_->sync(bid).is_ok());
    start += n;
    idx += n;
  }
  superblock_->inner.log_entry_offset += tot_size;
  // flush super block
  superblock_->flush(0);
  assert(bm_->sync(0).is_ok());
}

}; // namespace chfs