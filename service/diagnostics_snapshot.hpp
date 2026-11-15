// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <cstdint>
#include <memory>
#include <nscapi/protobuf/diagnostics.hpp>
#include <string>
#include <utility>
#include <vector>

namespace nsclient {
namespace core {

// One diagnostics snapshot as the core assembled it: every producing module's
// sections plus the core's own (failures.recent), capped, redacted and pruned
// (diagnostics_round.hpp).
//
// Immutable once stored and shared, exactly like metrics_snapshot: every
// failure recorded within one diagnostics interval refers to the same one,
// and the failed-check log accounts for it once.
struct diagnostics_snapshot {
  // Monotonic and 1-based for the lifetime of the process.
  std::uint64_t id = 0;
  // When the core finished the round, seconds since the unix epoch. Each
  // section carries its own `captured_at`, which can be older.
  std::int64_t captured_at = 0;
  PB::Diagnostics::DiagnosticsMessage message;
  // The serialised size, which is what the failed-check log budgets in.
  std::size_t wire_size = 0;
  // Section names `prune pattern` left out.
  std::vector<std::string> excluded;
  // Sections dropped because the snapshot was full or held one of the same
  // name.
  std::size_t dropped = 0;
  // The row cap that was applied.
  std::size_t max_rows = 0;
};
typedef std::shared_ptr<const diagnostics_snapshot> diagnostics_snapshot_ptr;

// The latest diagnostics snapshot. The core's diagnostics round
// (NSClientT::process_diagnostics) stores one every `diagnostics interval`
// while `capture diagnostics` is on; a check thread recording a failure and a
// reader of /api/v2/diagnostics take it with latest(). Nothing here ever
// collects: a reader gets what the last round left.
//
// Thread safety: store/latest/reset can be called from any thread, and the
// lock only covers a pointer swap.
class diagnostics_snapshot_store {
 public:
  diagnostics_snapshot_store() : next_id_(1) {}

  // Keep `snapshot` as the latest, giving it the next id and its wire size,
  // and return it.
  diagnostics_snapshot_ptr store(diagnostics_snapshot snapshot) {
    std::shared_ptr<diagnostics_snapshot> kept = std::make_shared<diagnostics_snapshot>(std::move(snapshot));
    kept->wire_size = kept->message.ByteSizeLong();
    boost::unique_lock<boost::mutex> lock(mutex_);
    kept->id = next_id_++;
    latest_ = kept;
    return latest_;
  }

  // The latest snapshot; null before the first round, or after a reset().
  diagnostics_snapshot_ptr latest() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return latest_;
  }

  // Forget the latest snapshot (capture was turned off). The id keeps
  // counting.
  void reset() {
    boost::unique_lock<boost::mutex> lock(mutex_);
    latest_.reset();
  }

 private:
  mutable boost::mutex mutex_;
  diagnostics_snapshot_ptr latest_;
  std::uint64_t next_id_;
};

typedef std::shared_ptr<diagnostics_snapshot_store> diagnostics_snapshot_store_instance;

}  // namespace core
}  // namespace nsclient
