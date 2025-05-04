//===----------------------------------------------------------------------===//
//
//                         Chfs
//
// commit_log.h
//
// Identification: src/include/distributed/commit_log.h
//
//
//===----------------------------------------------------------------------===//
#pragma once

#include "block/manager.h"
#include "common/config.h"
#include "common/macros.h"
#include "filesystem/operations.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace chfs {
/**
 * `BlockOperation` is an entry indicates an old block state and
 * a new block state. It's used to redo the operation when
 * the system is crashed.
 */
class BlockOperation {
public:
  explicit BlockOperation(block_id_t block_id, std::vector<u8> new_block_state)
      : block_id_(block_id), new_block_state_(new_block_state) {
    CHFS_ASSERT(new_block_state.size() == DiskBlockSize, "invalid block state");
  }

  block_id_t block_id_;
  std::vector<u8> new_block_state_;
};

/**
 * `CommitLog` is a class that records the block edits into the
 * commit log. It's used to redo the operation when the system
 * is crashed.
 */
class SuperBlock;
class CommitLog {
protected:
  struct LogEntry {
    txn_id_t txn_id;
    block_id_t update_block_begin;
    block_id_t update_block_end;
    block_id_t update_blocks[0];
  };
  static const u64 TOT_LOG_REGION_BLOCK = 1024u;
  static const u64 LOG_ENTRY_ARR_SIZE = 4096u;

public:
  explicit CommitLog(std::shared_ptr<BlockManager> bm,
                     bool is_checkpoint_enabled);
  ~CommitLog();
  auto append_log(txn_id_t txn_id,
                  std::vector<std::shared_ptr<BlockOperation>> ops) -> void;
  auto commit_log(txn_id_t txn_id) -> void;
  auto checkpoint() -> void;
  auto recover() -> void;
  auto get_log_entry_num() -> usize;

  /**
   * 管理事务id
   */
  auto get_txn_id() -> txn_id_t {
    return next_txn_id_.fetch_add(1);
  }

  bool is_checkpoint_enabled_;
  std::shared_ptr<BlockManager> bm_;
  /**
   * {Append anything if you need}
   */
  std::shared_ptr<SuperBlock> super_block_;
  usize log_entry_num_{}; // 磁盘上事务数
  std::atomic<txn_id_t> next_txn_id_{};
  using block_op_array = std::vector<std::shared_ptr<BlockOperation>>;
  std::unordered_map<txn_id_t, std::pair<LogEntry, block_op_array>> log_entries_{};
};

} // namespace chfs