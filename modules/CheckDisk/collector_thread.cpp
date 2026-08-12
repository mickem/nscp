// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collector_thread.hpp"

#include <ctime>
#include <functional>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/xtos.hpp>
#include <string>

namespace {
// Granularity of the persisted form: coarse enough to keep nsclient.db small,
// fine enough that a long-window trend survives a restart usefully. The
// in-memory 5-minute resolution rebuilds within the hour after a restart.
const long long trend_save_granularity = 30 * 60;
const long long trend_save_interval = 3600;
}  // namespace

disk_io_check::disks_type collector_thread::get_disk_io() { return disk_io_.get(); }

disk_free_check::drives_type collector_thread::get_disk_free() { return disk_free_.get(); }

collector_thread::trend_snapshot collector_thread::get_drive_trends() {
  const boost::lock_guard<boost::mutex> lock(trends_mutex_);
  return snapshot_;
}

bool collector_thread::start() {
  {
    const boost::lock_guard<boost::mutex> lock(stop_mutex_);
    stop_requested_ = false;
  }
  thread_ = std::make_shared<boost::thread>([this]() { this->thread_proc(); });
  return true;
}

bool collector_thread::stop() {
  {
    const boost::lock_guard<boost::mutex> lock(stop_mutex_);
    stop_requested_ = true;
  }
  stop_cv_.notify_all();
  if (thread_) {
    thread_->join();
    thread_.reset();
  }
  return true;
}

void collector_thread::update_trends(const long long now) {
  const disk_free_check::drives_type drives = disk_free_.get();
  const boost::lock_guard<boost::mutex> lock(trends_mutex_);
  for (const disk_free_check::disk_free &d : drives) {
    // A drive that momentarily reports no size at all is not a resize: taking
    // it at face value would flip the trend context to 0, discard the whole
    // history, and flip back on the next tick.
    if (d.total <= 0) continue;
    trend_map::iterator it = trends_.find(d.name);
    if (it == trends_.end()) it = trends_.insert(trend_map::value_type(d.name, trend::trend_buffer(trend_interval, trend_retention))).first;
    // Track used bytes; the total is the reset context (a resize invalidates
    // the history).
    it->second.append(now, d.total - d.free, d.total);
  }
  // Drives that have come and gone age out with their samples.
  for (trend_map::iterator it = trends_.begin(); it != trends_.end();) {
    if (it->second.empty() || it->second.newest_ts() < now - trend_retention)
      it = trends_.erase(it);
    else
      ++it;
  }
  publish_trends();
}

// Republish the immutable snapshot handed to checks. Rebuilt once per
// collector tick rather than copied per check: at a 5-minute cadence and 7-day
// retention one drive carries ~2000 samples, and a busy poller would otherwise
// deep-copy every drive's history (under the collector's lock) on every single
// check_drivesize invocation.
void collector_thread::publish_trends() { snapshot_ = std::make_shared<const trend_map>(trends_); }

void collector_thread::load_trends() {
  try {
    nscapi::core_helper core(core_, plugin_id_);
    const long long now = static_cast<long long>(std::time(nullptr));
    trend_map loaded;
    for (const nscapi::core_helper::storage_map::value_type &e : core.get_storage_strings("disk.trends")) {
      // Remember every key we saw, including the ones that decode to nothing:
      // save_trends() clears the rows that no longer back a live drive.
      saved_keys_.insert(e.first);
      trend::trend_buffer buf = trend::trend_buffer::decode(e.second, trend_interval, trend_retention, now);
      if (!buf.empty()) loaded[e.first] = buf;
    }
    if (loaded.empty()) return;
    const boost::lock_guard<boost::mutex> lock(trends_mutex_);
    trends_.swap(loaded);
    publish_trends();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to load drive trends: " + std::string(e.what()));
  } catch (...) {
    NSC_LOG_ERROR("Failed to load drive trends");
  }
}

void collector_thread::save_trends() {
  try {
    const trend_snapshot snapshot = get_drive_trends();
    if (!snapshot) return;
    nscapi::core_helper core(core_, plugin_id_);
    for (const trend_map::value_type &v : *snapshot) {
      if (v.second.empty()) continue;
      core.put_storage("disk.trends", v.first, v.second.encode(trend_save_granularity), false, false);
      saved_keys_.insert(v.first);
    }
    // Drives that have aged out of memory keep their stored row forever
    // otherwise: on a host with churning mounts (containers, removable media)
    // nsclient.db would grow without bound. An empty value decodes to an empty
    // buffer, which load_trends() drops.
    for (std::set<std::string>::iterator it = saved_keys_.begin(); it != saved_keys_.end();) {
      if (snapshot->find(*it) != snapshot->end()) {
        ++it;
        continue;
      }
      core.put_storage("disk.trends", *it, "", false, false);
      it = saved_keys_.erase(it);
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to save drive trends: " + std::string(e.what()));
  } catch (...) {
    NSC_LOG_ERROR("Failed to save drive trends");
  }
}

void collector_thread::thread_proc() {
  bool disable_disk_io = disable_.find("disk_io") != std::string::npos;
  if (disable_disk_io) {
    NSC_LOG_MESSAGE("WARNING: disk I/O checking is disabled");
  }
  bool disable_disk_free = disable_.find("disk_free") != std::string::npos;
  if (disable_disk_free) {
    NSC_LOG_MESSAGE("WARNING: disk free checking is disabled");
  }
  // Trends ride on the disk_free fetch, so disabling that disables them too.
  bool disable_trend = disable_disk_free || disable_.find("trend") != std::string::npos;
  if (disable_trend && !disable_disk_free) {
    NSC_LOG_MESSAGE("WARNING: disk trend tracking is disabled");
  }

  if (!disable_trend) load_trends();

  // A failed fetch is retried on the next tick; only a run of consecutive
  // failures gives up on a collection (see collector_failure_tracker). The
  // fetchers keep their own permanent-disable paths for sources that answer
  // "this query is not supported here" (WBEM_E_INVALID_QUERY / _NOT_FOUND),
  // which is a different thing from a source that failed to answer.
  collector_failure_tracker io_failures(max_collection_errors);
  collector_failure_tracker free_failures(max_collection_errors);

  // Runs one fetch, logging the error and updating the tracker. `fetch` returns
  // false for "could not collect at all"; exceptions count the same, except
  // when `collected_anyway` says the source stored data before raising the
  // error - a partial failure degrades one part of the data, not the
  // collection, and must not count towards giving up on it.
  const auto run_fetch = [](collector_failure_tracker &failures, const char *what, const std::function<bool()> &fetch,
                            const std::function<bool()> &collected_anyway) -> bool {
    bool fetched = false;
    try {
      fetched = fetch();
      if (!fetched) NSC_LOG_ERROR(std::string("Failed to get ") + what + ": no data returned");
    } catch (const nsclient::nsclient_exception &e) {
      NSC_LOG_ERROR(std::string("Failed to get ") + what + ": " + e.reason());
      fetched = collected_anyway && collected_anyway();
    } catch (const std::exception &e) {
      NSC_LOG_ERROR(std::string("Failed to get ") + what + ": " + e.what());
      fetched = collected_anyway && collected_anyway();
    } catch (...) {
      NSC_LOG_ERROR(std::string("Failed to get ") + what);
      fetched = collected_anyway && collected_anyway();
    }
    if (fetched) {
      failures.succeeded();
    } else if (failures.failed()) {
      NSC_LOG_ERROR(std::string("Giving up on ") + what + " after " + str::xtos(failures.consecutive()) +
                    " consecutive failures, it will not be collected again until NSClient++ is restarted (see the 'max collection errors' setting)");
    }
    return fetched;
  };

  const std::function<bool()> fetch_disk_io = [this]() { return disk_io_.fetch(); };
  // The Windows fetch stores the rates before raising a latency error, so the
  // rates keep flowing and only latency degrades; that must not accumulate
  // towards giving up on disk I/O altogether.
  const std::function<bool()> disk_io_stored_data = [this]() { return disk_io_.stored_data(); };
  const std::function<bool()> fetch_disk_free = [this]() { return disk_free_.fetch(); };

  // Initial fetch to populate data immediately.
  if (!disable_disk_io) {
    run_fetch(io_failures, "disk I/O metrics", fetch_disk_io, disk_io_stored_data);
  }
  if (!disable_disk_free) {
    run_fetch(free_failures, "disk free metrics", fetch_disk_free, nullptr);
  }

  const long long started = static_cast<long long>(std::time(nullptr));
  long long last_save = started;
  for (;;) {
    if (!disable_disk_io && !io_failures.given_up()) {
      run_fetch(io_failures, "disk I/O metrics", fetch_disk_io, disk_io_stored_data);
    }
    if (!disable_disk_free && !free_failures.given_up()) {
      // A failed fetch leaves the previous snapshot in place. Timestamping it
      // as a fresh sample would feed the regression a fabricated flat segment,
      // so the trend simply skips the tick and leaves a gap (which OLS over
      // irregular timestamps handles natively).
      const bool fetched = run_fetch(free_failures, "disk free metrics", fetch_disk_free, nullptr);
      if (!disable_trend && fetched) {
        const long long now = static_cast<long long>(std::time(nullptr));
        update_trends(now);
        if (now - last_save >= trend_save_interval) {
          save_trends();
          last_save = now;
        }
      }
    }
    // Everything this thread collects is either disabled or has been given up
    // on: keep the trends we have, but stop waking up to do nothing.
    if ((disable_disk_io || io_failures.given_up()) && (disable_disk_free || free_failures.given_up())) {
      NSC_LOG_MESSAGE("WARNING: nothing left to collect, stopping the disk collector");
      break;
    }
    // Sleep until the next interval, waking early if stop() was requested.
    boost::unique_lock<boost::mutex> lock(stop_mutex_);
    if (stop_cv_.timed_wait(lock, boost::posix_time::seconds(collection_interval), [this]() { return stop_requested_; })) {
      break;  // stop requested
    }
  }
  // Skip the shutdown save when the collector ran for less than one trend
  // interval: nothing worth persisting can have accumulated, and one-shot
  // client runs (nscp client --boot) would otherwise attempt (and, without a
  // writable data path, noisily fail) a storage write on every invocation.
  if (!disable_trend && static_cast<long long>(std::time(nullptr)) - started >= trend_interval) save_trends();
}
