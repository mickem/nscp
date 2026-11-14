// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <cstdint>
#include <memory>
#include <nscapi/protobuf/metrics.hpp>
#include <utility>

#include "metrics_prune.hpp"

namespace nsclient {
namespace core {

// One metrics snapshot as the core collected it: every module's bundles plus
// the core's own, the same message the submitters (the WEBServer's
// `/api/v2/metrics`, Graphite, collectd) were handed.
//
// Immutable once stored, and shared: whoever wants to keep "the system at the
// time" - the failed-check log, for one - holds a shared_ptr to it rather
// than copying the message, so every failure recorded inside the same metrics
// interval costs one snapshot, not one each.
struct metrics_snapshot {
  // Monotonic and 1-based for the lifetime of the process, so two entries
  // can tell whether they were recorded against the same snapshot.
  std::uint64_t id = 0;
  // When the core finished collecting it, seconds since the unix epoch.
  std::int64_t captured_at = 0;
  PB::Metrics::MetricsMessage message;
  // The serialised size, which is what the failed-check log budgets in.
  std::size_t wire_size = 0;
  // What `prune metrics` took out before it was stored, and under which rules.
  metrics_prune::rules prune_rules;
  metrics_prune::summary pruned;
};
typedef std::shared_ptr<const metrics_snapshot> metrics_snapshot_ptr;

// The latest metrics snapshot. The scheduler collects one every
// `[/settings/core] metrics interval` (NSClientT::process_metrics) and stores
// it here once the submitters have run; a check thread reads it with
// latest(). Holding on to it is the reader's business: a store() replaces the
// pointer, it never touches a snapshot someone else still holds.
//
// Thread safety: store/latest/reset can be called from any thread, and the
// lock only covers a pointer swap.
class metrics_snapshot_store {
 public:
  metrics_snapshot_store() : next_id_(1) {}

  // Keep `message` as the latest snapshot, pruned by `rules`, and return it.
  metrics_snapshot_ptr store(PB::Metrics::MetricsMessage message, const std::int64_t now, const metrics_prune::rules &rules = metrics_prune::rules()) {
    std::shared_ptr<metrics_snapshot> snapshot = std::make_shared<metrics_snapshot>();
    snapshot->captured_at = now;
    snapshot->prune_rules = rules;
    snapshot->pruned = metrics_prune::apply(message, rules);
    snapshot->wire_size = message.ByteSizeLong();
    snapshot->message = std::move(message);
    boost::unique_lock<boost::mutex> lock(mutex_);
    snapshot->id = next_id_++;
    latest_ = snapshot;
    return latest_;
  }

  // The latest snapshot; null before the first one, or after a reset().
  metrics_snapshot_ptr latest() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return latest_;
  }

  // Forget the latest snapshot (nothing wants one any more). The id keeps
  // counting.
  void reset() {
    boost::unique_lock<boost::mutex> lock(mutex_);
    latest_.reset();
  }

 private:
  mutable boost::mutex mutex_;
  metrics_snapshot_ptr latest_;
  std::uint64_t next_id_;
};

typedef std::shared_ptr<metrics_snapshot_store> metrics_snapshot_store_instance;

}  // namespace core
}  // namespace nsclient
