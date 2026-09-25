// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <memory>
#include <threads/stop_signal.hpp>
#include <vector>

#include "check_hyperv_internal.hpp"

// The virtual machines the metrics publish, read on a thread of their own.
//
// fetchMetrics runs on the core's one metrics thread, shared by every module,
// and the VM walk is seven WMI queries that grow with every VM and checkpoint
// and can stall for as long as the Hyper-V management provider takes to
// start - the reason the startup facts round stays off the namespace too.
// So the walk runs here, once per interval, and fetchMetrics only takes the
// last snapshot, which costs a lock and a shared_ptr copy.
//
// Started by the first fetchMetrics rather than by loadModuleEx, so a module
// whose metrics nobody collects never walks the namespace in the background,
// and a settings reload (which re-enters loadModuleEx) has nothing to stop.
class vm_refresher {
 public:
  struct snapshot {
    std::vector<check_hyperv::check_hyperv_internal::vm_record> vms;
    // False when the walk failed (no role, a stopped service) or the list
    // cannot be vouched for (VMs hidden from this account): publish no
    // per-VM metrics and no vms.total rather than a 0 that is not true.
    bool usable = false;
  };

  explicit vm_refresher(std::chrono::seconds interval) : interval_(interval) {}
  ~vm_refresher() { stop(); }
  vm_refresher(const vm_refresher &) = delete;
  vm_refresher &operator=(const vm_refresher &) = delete;

  // Start the thread unless it is running. Cheap to call on every interval.
  void ensure_started();
  // Stop and join. The walk in flight is aborted between rows; one blocked
  // connecting to the namespace is not, and holds the join until it returns.
  void stop();

  // The last walk's result, or null before the first one has finished.
  std::shared_ptr<const snapshot> get();

 private:
  void thread_proc();
  void refresh();

  const std::chrono::seconds interval_;
  std::shared_ptr<boost::thread> thread_;
  boost::mutex mutex_;  // guards stop_requested_ and snapshot_
  boost::condition_variable stop_cv_;
  bool stop_requested_ = false;
  threads::stop_signal abort_signal_;
  std::shared_ptr<const snapshot> snapshot_;
};
