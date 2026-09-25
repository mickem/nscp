// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "vm_refresher.hpp"

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <map>
#include <nscapi/macros.hpp>
#include <string>
#include <threads/guarded_thread.hpp>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_vms.hpp"

void vm_refresher::ensure_started(const long long counted_vms) {
  const boost::lock_guard<boost::mutex> lock(mutex_);
  counted_vms_ = counted_vms;
  if (thread_) return;
  stop_requested_ = false;
  // A walk that cannot be aborted cannot be joined either, so without the
  // primitive no thread is started (see threads::stop_signal); the next
  // interval tries again.
  std::string error;
  if (!abort_signal_.create(error)) {
    NSC_LOG_ERROR("Failed to create the Hyper-V refresh abort signal, the virtual machines are not refreshed: " + error);
    return;
  }
  thread_ = threads::start_guarded_thread("checkhyperv vm refresh", [this]() { this->thread_proc(); }, NSC_THREAD_REPORTER);
}

void vm_refresher::stop() {
  std::shared_ptr<boost::thread> thread;
  {
    const boost::lock_guard<boost::mutex> lock(mutex_);
    stop_requested_ = true;
    // The walk in flight watches abort_signal_, the wait between walks the CV.
    abort_signal_.signal();
    thread.swap(thread_);
  }
  stop_cv_.notify_all();
  if (thread) thread->join();
  // After the join, so a stop/start cycle gets a fresh, unsignalled primitive;
  // unless a start that raced this stop owns the signal by now.
  const boost::lock_guard<boost::mutex> lock(mutex_);
  if (!thread_) abort_signal_.close();
}

std::shared_ptr<const vm_refresher::snapshot> vm_refresher::get() {
  const boost::lock_guard<boost::mutex> lock(mutex_);
  return snapshot_;
}

void vm_refresher::thread_proc() {
  const com_helper::mta_scope com;
  while (true) {
    try {
      refresh();
    } catch (const threads::stop_requested &) {
      return;  // stop() aborted the walk
    } catch (const std::exception &e) {
      // Anything but a WMI error (which refresh() turns into an unusable
      // snapshot): log it and try again next interval rather than let the
      // guard end the thread for good.
      NSC_LOG_ERROR("Failed to refresh Hyper-V virtual machines: " + std::string(e.what()));
    }
    boost::unique_lock<boost::mutex> lock(mutex_);
    const boost::posix_time::seconds wait(static_cast<long>(interval_.count()));
    if (stop_cv_.timed_wait(lock, wait, [this]() { return stop_requested_; })) return;
  }
}

void vm_refresher::refresh() {
  long long counted = -1;
  {
    const boost::lock_guard<boost::mutex> lock(mutex_);
    counted = counted_vms_;
  }
  auto next = std::make_shared<snapshot>();
  try {
    next->vms = check_hyperv::check_hyperv_internal::build_records(check_hyperv::fetch_vm_rows(abort_signal_.native_handle()));
    next->usable = check_hyperv::vms_hidden_from_caller(next->vms.size(), counted).empty();
  } catch (const wmi_impl::wmi_exception &) {
    // No role or a stopped service; the check reports it where it is visible.
    next->vms.clear();
  }
  const boost::lock_guard<boost::mutex> lock(mutex_);
  snapshot_ = next;
}
