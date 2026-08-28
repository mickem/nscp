// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "realtime_thread.hpp"

#include <boost/filesystem.hpp>
#include <cerrno>
#include <error/error.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <simple_timer.hpp>
#include <str/utils.hpp>
#include <vector>

#include "filter.hpp"
#include "realtime_data.hpp"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#endif

#ifndef WIN32
#include <poll.h>
#include <sys/inotify.h>
#endif

typedef parsers::where::realtime_filter_helper<runtime_data, filters::filter_config_object> filter_helper;

void real_time_thread::thread_proc() {
  filter_helper helper(core, plugin_id);
  std::list<std::string> logs;

  for (std::shared_ptr<filters::filter_config_object> object : filters_.get_object_list()) {
    runtime_data data;
    data.set_split(object->line_split, object->column_split);
    data.set_read_from_start(object->read_from_start);
    for (const std::string &file : object->files) {
      boost::filesystem::path path = file;
      data.add_file(path);
#ifdef WIN32
      if (boost::filesystem::is_directory(path)) {
        logs.push_back(path.string());
      } else {
        path = path.remove_filename();
        if (boost::filesystem::is_directory(path)) {
          logs.push_back(path.string());
        } else {
          NSC_LOG_ERROR("Failed to find folder for " + object->get_alias() + ": " + path.string());
          continue;
        }
      }
#else
      if (boost::filesystem::is_regular_file(path)) {
        logs.push_back(path.string());
      } else {
        NSC_LOG_ERROR("Failed to find folder for " + object->get_alias() + ": " + path.string());
        continue;
      }
#endif
    }
    helper.add_item(object, data, "logfile");
  }

  logs.sort();
  logs.unique();
  NSC_DEBUG_MSG_STD("Subscribing to folders: " + str::utils::joinEx(logs, ", "));
  std::vector<std::string> files_list(logs.begin(), logs.end());
#ifdef WIN32
  // handles[0] is the stop event; handles[i+1] watches watched_folders[i].
  // A folder that cannot be watched (deleted, renamed or ACL'd away between
  // the is_directory() check above and here) yields INVALID_HANDLE_VALUE.
  // Such a handle makes every WaitForMultipleObjects on the whole array
  // return WAIT_FAILED - including the stop event in slot 0 - so it must be
  // dropped rather than waited on.
  std::vector<HANDLE> handles;
  std::vector<std::string> watched_folders;
  handles.push_back(stop_signal_.native_handle());
  for (const std::string &folder : files_list) {
    const HANDLE handle = FindFirstChangeNotification(utf8::cvt<std::wstring>(folder).c_str(), TRUE, FILE_NOTIFY_CHANGE_SIZE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
      NSC_LOG_ERROR("Failed to watch folder (it will not be monitored): " + folder + ": " + error::lookup::last_error());
      continue;
    }
    handles.push_back(handle);
    watched_folders.push_back(folder);
  }
  if (watched_folders.empty()) {
    // Still worth running: the loop's timeout path drives the filter's
    // ok/stale processing, and the stop event has to stay waitable.
    NSC_LOG_ERROR("No log folders could be watched, only time-based processing will run");
  }
  unsigned int wait_failures = 0;
#else

  struct pollfd pollfds[2] = {{inotify_init(), POLLIN | POLLPRI, 0}, {stop_signal_.wait_fd(), POLLIN, 0}};
  if (pollfds[0].fd < 0) {
    // Most likely fs.inotify.max_user_instances exhausted - which is exactly
    // what the leak fixed below used to cause. Keep polling the stop fd so the
    // thread stays joinable and time-based processing still runs.
    NSC_LOG_ERROR("Failed to create inotify instance, only time-based processing will run: " + error::lookup::last_error(errno));
  }

  std::vector<int> wds(files_list.size(), -1);
  for (std::size_t i = 0; i < files_list.size(); i++) {
    if (pollfds[0].fd < 0) break;
    wds[i] = inotify_add_watch(pollfds[0].fd, files_list[i].c_str(), IN_MODIFY);
    if (wds[i] < 0) {
      NSC_LOG_ERROR("Failed to watch file (it will not be monitored): " + files_list[i] + ": " + error::lookup::last_error(errno));
    }
  }

#endif

  helper.touch_all();

  // `run on startup` submissions are delivered from inside the loop rather
  // than as a one-shot here: this thread starts while later plugins (for
  // instance the SimpleCache behind a `destination = cache`) may still be
  // loading, so failed attempts are retried across the loop's waits - which
  // are shortened while priming is outstanding - until the destination's
  // channel exists.
  bool startup_done = !helper.has_pending_startup();

  while (true) {
    if (!startup_done) startup_done = helper.process_startup();
    filter_helper::op_duration dur = helper.find_minimum_timeout();
    if (!startup_done && (!dur || *dur > boost::posix_time::milliseconds(500))) dur = boost::posix_time::milliseconds(500);
    std::string trigger_folder;
#ifdef WIN32
    DWORD dwWaitTime = INFINITE;
    if (dur && dur->total_milliseconds() < 0)
      dwWaitTime = 0;
    else if (dur)
      dwWaitTime = static_cast<DWORD>(dur->total_milliseconds());
    const DWORD dwWaitReason = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, dwWaitTime);
    if (dwWaitReason == WAIT_TIMEOUT) {
      // we take care of this below...
    } else if (dwWaitReason == WAIT_OBJECT_0) {
      break;
    } else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + watched_folders.size())) {
      const std::size_t id = dwWaitReason - WAIT_OBJECT_0;
      FindNextChangeNotification(handles[id]);
      trigger_folder = watched_folders[id - 1];
    } else if (dwWaitReason == WAIT_FAILED) {
      // Nothing in the array is waitable any more (a watched folder was
      // removed under us, or a handle went stale). Without this branch the
      // wait returns immediately every iteration and the loop spins at 100%
      // CPU on a core - and because the stop event is in the same failing
      // array, its signal is never reported, so stop()'s join() never
      // returns and shutdown hangs. Bail out on a budget, mirroring
      // CheckEventLog's loop.
      if (++wait_failures == 1) {
        NSC_LOG_ERROR("Failed to wait for log folder changes: " + error::lookup::last_error());
      }
      if (wait_failures > 100) {
        NSC_LOG_ERROR("Too many wait failures in the logfile loop, giving up on realtime monitoring");
        break;
      }
      continue;
    }
#else

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

    int timeout = 1000 * 60;
    if (dur) timeout = dur->total_milliseconds();
    char buffer[BUF_LEN];
    int length = poll(pollfds, 2, timeout);
    if (!length) {
      continue;
    } else if (length < 0) {
      NSC_LOG_ERROR("read failed!");
      continue;
    } else if (pollfds[1].revents != 0) {
      // break, not return: the cleanup after the loop removes the watches and
      // closes the inotify fd. Returning here leaked one inotify instance per
      // stop/start cycle, and after fs.inotify.max_user_instances cycles
      // (default 128) inotify_init() starts failing for good.
      break;
    } else if (pollfds[0].revents != 0) {
      length = read(pollfds[0].fd, buffer, BUF_LEN);
      for (int j = 0; j < length;) {
        struct inotify_event *event = (struct inotify_event *)&buffer[j];
        trigger_folder = event->name;
        j += EVENT_SIZE + event->len;
      }
    } else {
      NSC_LOG_ERROR("Strange, please report this...");
    }
#endif
    helper.process_items(std::shared_ptr<runtime_data::transient_data_impl>(new runtime_data::transient_data_impl(trigger_folder)));
  }

#ifdef WIN32
  // handles[0] is the stop event, which stop() owns and closes; the rest are
  // change-notification handles created here and never released before.
  for (std::size_t i = 1; i < handles.size(); i++) {
    FindCloseChangeNotification(handles[i]);
  }
#else
  if (pollfds[0].fd >= 0) {
    for (std::size_t i = 0; i < wds.size(); i++) {
      if (wds[i] >= 0) inotify_rm_watch(pollfds[0].fd, wds[i]);
    }
    close(pollfds[0].fd);
  }
  // pollfds[1].fd is the stop_signal read end, which stop() owns and closes
  // after the join - closing it here would leave stop() operating on a stale
  // (possibly reused) descriptor.
#endif
  return;
}

bool real_time_thread::start() {
  if (!enabled_) return true;
  // Never spawn the monitor thread unless the stop primitive exists. Without it
  // the thread cannot be signalled: on POSIX poll() silently ignores a -1 fd, on
  // Windows a null handle makes every WaitForMultipleObjects return WAIT_FAILED.
  // Either way stop()'s join() would block forever and take service shutdown
  // with it, so a failure here has to disable realtime monitoring rather than
  // start something unstoppable.
  // See threads::stop_signal for why the Windows event is unnamed and why a
  // failure here must not start a thread.
  std::string error;
  if (!stop_signal_.create(error)) {
    NSC_LOG_ERROR("Failed to create stop signal, realtime log monitoring is disabled: " + error);
    return false;
  }
  thread_ = std::shared_ptr<boost::thread>(new boost::thread([this]() { this->thread_proc(); }));
  return true;
}
bool real_time_thread::stop() {
  if (!enabled_) return true;
  stop_signal_.signal();
  if (thread_) {
    thread_->join();
    thread_.reset();
  }
  // Release the signal primitive after the join so a stop/start cycle gets a
  // fresh one instead of leaking a handle (or a pipe fd pair) per cycle.
  stop_signal_.close();
  return true;
}

void real_time_thread::add_realtime_filter(nscapi::settings_helper::settings_impl_interface_ptr proxy, std::string key, std::string query) {
  try {
    filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
  }
}

void real_time_thread::ensure_default(nscapi::settings_helper::settings_impl_interface_ptr proxy) { filters_.ensure_default(proxy); }
