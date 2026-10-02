// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "realtime_thread.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <chrono>
#include <fstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <sstream>
#include <stdexcept>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <thread>
#include <threads/guarded_thread.hpp>

#include "realtime_data.hpp"

typedef parsers::where::realtime_filter_helper<checks::check_cpu_filter::runtime_data, filters::cpu::filter_config_object> cpu_filter_helper;
typedef parsers::where::realtime_filter_helper<check_memory::check_mem_filter::runtime_data, filters::mem::filter_config_object> mem_filter_helper;
typedef parsers::where::realtime_filter_helper<check_proc::check_proc_filter::runtime_data, filters::proc::filter_config_object> proc_filter_helper;

/**
 * Thread that collects the data every second and evaluates real-time filters.
 */
void pdh_thread::thread_proc() {
  // Initial read to establish baseline. A failure here must not end the
  // thread: the loop below keeps trying, and the first tick that reads
  // successfully simply has no previous sample to diff against.
  try {
    last_cpu_times_ = collector_source::read_cpu_times();
    last_net_ = collector_source::read_network();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read the initial system sample: " + std::string(e.what()));
  }
  auto last_net_sample = std::chrono::steady_clock::now();

  // Build real-time filter helpers from the configured objects
  cpu_filter_helper cpu_helper(core_, plugin_id_);
  mem_filter_helper mem_helper(core_, plugin_id_);
  proc_filter_helper proc_helper(core_, plugin_id_);

  for (const std::shared_ptr<filters::cpu::filter_config_object> &object : cpu_filters_.get_object_list()) {
    try {
      checks::check_cpu_filter::runtime_data data;
      if (object->data.empty()) {
        data.add("1m");
      } else {
        for (const std::string &d : object->data) {
          data.add(d);
        }
      }
      cpu_helper.add_item(object, data, "system.cpu");
    } catch (const std::exception &e) {
      NSC_LOG_ERROR_EXR("Skipping CPU filter '" + object->get_alias() + "' (invalid time spec): ", e);
    }
  }
  for (const std::shared_ptr<filters::mem::filter_config_object> &object : mem_filters_.get_object_list()) {
    check_memory::check_mem_filter::runtime_data data;
    if (object->data.empty()) {
      data.add("physical");
    } else {
      for (const std::string &d : object->data) {
        data.add(d);
      }
    }
    mem_helper.add_item(object, data, "system.memory");
  }
  for (const std::shared_ptr<filters::proc::filter_config_object> &object : proc_filters_.get_object_list()) {
    check_proc::check_proc_filter::runtime_data data;
    for (const std::string &d : object->data) {
      data.add(d);
    }
    proc_helper.add_item(object, data, "system.process");
  }

  cpu_helper.touch_all();
  mem_helper.touch_all();
  proc_helper.touch_all();

  // `run on startup` submissions are delivered from inside the 1 Hz loop
  // below rather than here: this thread starts while later plugins (the
  // destinations of the filters) may still be loading, so the first attempt
  // waits a tick and failed submissions are retried until the destination's
  // channel exists.
  bool startup_done = false;

  const bool has_cpu_realtime = !cpu_filters_.empty();
  const bool has_mem_realtime = !mem_filters_.empty();
  const bool has_proc_realtime = !proc_filters_.empty();
  if (has_cpu_realtime || has_mem_realtime || has_proc_realtime) {
    NSC_DEBUG_MSG("Real time checks enabled");
  }

  while (!stop_requested_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (stop_requested_) break;

    try {
      // Each source is read and recorded on its own, so one unreadable file
      // (an empty or unreadable /proc/stat in a locked-down container) does
      // not mark the others as failed. The readers report an unreadable
      // source as an empty result rather than by throwing, so emptiness is
      // what is checked: storing it would present zeros as a measurement.
      boost::optional<cpu_load> load;
      std::string cpu_error;
      bool cpu_attempted = true;
      try {
        auto current_times = collector_source::read_cpu_times();
        if (current_times.empty()) throw std::runtime_error("no CPU times could be read");
        if (last_cpu_times_.empty()) {
          // Only the baseline so far: there is no delta to store yet, which
          // is still the warm-up, not a sample and not a failure.
          cpu_attempted = false;
        } else {
          load = collector_calc::calculate_cpu_load(last_cpu_times_, current_times);
        }
        last_cpu_times_ = current_times;
      } catch (const std::exception &e) {
        cpu_error = e.what();
      }

      boost::optional<memory_info> mem;
      std::string memory_error;
      try {
        const collector_source::memory_sample sample = collector_source::read_memory();
        if (sample.physical_total == 0) throw std::runtime_error("no memory information could be read");
        mem = collector_calc::to_memory_info(sample);
      } catch (const std::exception &e) {
        memory_error = e.what();
      }

      // Collect network data (rates over the measured sampling interval — the
      // loop target is 1s but scheduling delays and suspend/resume stretch it).
      boost::optional<network_check::nics_type> nics;
      std::string network_error;
      try {
        auto net_now = collector_source::read_network();
        // Every host lists at least its loopback interface.
        if (net_now.empty()) throw std::runtime_error("no network interfaces could be read");
        const auto net_sample_time = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(net_sample_time - last_net_sample).count();
        nics = collector_calc::calculate_network(last_net_, net_now, dt);
        last_net_ = net_now;
        last_net_sample = net_sample_time;
      } catch (const std::exception &e) {
        network_error = e.what();
      }

      log_sampling_error("CPU", cpu_error, last_logged_cpu_error);
      log_sampling_error("memory", memory_error, last_logged_memory_error);
      log_sampling_error("network", network_error, last_logged_network_error);

      // Collect process history (only when explicitly enabled — it enumerates
      // every process every second).
      std::set<std::string> running_exes;
      const bool track_history = process_history_enabled;
      if (track_history) running_exes = collector_source::read_running_exes();

      {
        boost::unique_lock lock(mutex_);
        if (load) {
          cpu_buffer_.push(load.value());
          cpu_sampling_.succeeded();
        } else if (cpu_attempted) {
          cpu_sampling_.failed(cpu_error);
        }
        if (mem) {
          memory_buffer_.push(mem.value());
          memory_sampling_.succeeded();
        } else {
          memory_sampling_.failed(memory_error);
        }
        if (nics) {
          network_ = nics.value();
          network_sampling_.succeeded();
        } else {
          network_sampling_.failed(network_error);
        }
        if (track_history) {
          static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
          const long long now_ts = (boost::posix_time::second_clock::universal_time() - epoch).total_seconds();
          update_process_history(running_exes, now_ts);
        }
      }
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to collect system data: " + std::string(e.what()));
    } catch (...) {
      NSC_LOG_ERROR("Failed to collect system data: unknown error");
    }

    // Evaluate real-time filters once we have collected data
    try {
      if (!startup_done) {
        const bool cpu_done = cpu_helper.process_startup();
        const bool mem_done = mem_helper.process_startup();
        const bool proc_done = proc_helper.process_startup();
        startup_done = cpu_done && mem_done && proc_done;
      }
      if (has_cpu_realtime) cpu_helper.process_items(this);
      if (has_mem_realtime) mem_helper.process_items(this);
      if (has_proc_realtime) proc_helper.process_items(this);
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to evaluate real-time filters: " + std::string(e.what()));
    }
  }
}

bool pdh_thread::has_cpu_data() const {
  boost::shared_lock lock(mutex_);
  return cpu_buffer_.has_data();
}

std::map<std::string, load_entry> pdh_thread::get_cpu_load(long seconds) {
  std::map<std::string, load_entry> ret;

  const boost::shared_lock lock(mutex_);

  try {
    cpu_load load = cpu_buffer_.get_average(seconds);
    ret["total"] = load.total;
    for (const load_entry &l : load.core) {
      ret["core " + str::xtos(l.core)] = l;
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to get CPU average: " + std::string(e.what()));
  }

  return ret;
}

bool pdh_thread::has_memory_data() const {
  boost::shared_lock lock(mutex_);
  return memory_buffer_.has_data();
}

memory_info pdh_thread::get_memory(long seconds) {
  memory_info ret;

  const boost::shared_lock lock(mutex_);

  try {
    ret = memory_buffer_.get_average(seconds);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to get memory average: " + std::string(e.what()));
  }

  return ret;
}

network_check::nics_type pdh_thread::get_network() const {
  const boost::shared_lock lock(mutex_);
  return network_;
}

bool pdh_thread::has_network_data() const {
  boost::shared_lock lock(mutex_);
  return !network_.empty();
}

sampling::status pdh_thread::cpu_status() const {
  boost::shared_lock lock(mutex_);
  return cpu_sampling_.get(cpu_buffer_.has_data());
}

sampling::status pdh_thread::memory_status() const {
  boost::shared_lock lock(mutex_);
  return memory_sampling_.get(memory_buffer_.has_data());
}

sampling::status pdh_thread::network_status() const {
  boost::shared_lock lock(mutex_);
  return network_sampling_.get(!network_.empty());
}

// Logged when the reason changes rather than every second, so a source that
// stays unreadable does not flood the log; recovery is logged once too.
void pdh_thread::log_sampling_error(const std::string &what, const std::string &error, std::string &last_logged) {
  if (error == last_logged) return;
  if (error.empty()) {
    NSC_LOG_MESSAGE("Reading " + what + " data works again");
  } else {
    NSC_LOG_ERROR("Failed to read " + what + " data: " + error);
  }
  last_logged = error;
}

// Caller must hold the unique (write) lock on mutex_.
void pdh_thread::update_process_history(const std::set<std::string> &running, long long now_ts) {
  std::set<std::string> running_keys;
  for (const std::string &exe : running) {
    const std::string key = boost::algorithm::to_lower_copy(exe);
    running_keys.insert(key);
    auto it = proc_history_.find(key);
    if (it == proc_history_.end()) {
      proc_history_[key] = process_history_check::process_record(exe, now_ts);
    } else {
      it->second.last_seen = now_ts;
      // times_seen counts starts (as on Windows): only bump on a
      // not-running -> running transition, not on every sample.
      if (!it->second.currently_running) it->second.times_seen += 1;
      it->second.currently_running = true;
    }
  }
  for (auto &kv : proc_history_) {
    if (running_keys.count(kv.first) == 0) kv.second.currently_running = false;
  }
}

process_history_check::history_type pdh_thread::get_process_history() const {
  process_history_check::history_type result;
  const boost::shared_lock lock(mutex_);
  for (const auto &kv : proc_history_) {
    result.push_back(kv.second);
  }
  return result;
}

bool pdh_thread::start() {
  stop_requested_ = false;
  thread_ = threads::start_guarded_thread("checksystem collector", [this]() { this->thread_proc(); }, NSC_THREAD_REPORTER);
  return true;
}

bool pdh_thread::stop() {
  stop_requested_ = true;
  if (thread_) {
    thread_->join();
    // Idempotent: the destructor calls stop() again after unloadModule did.
    thread_.reset();
  }
  return true;
}

void pdh_thread::add_realtime_cpu_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    cpu_filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add cpu real-time filter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add cpu real-time filter: " + key);
  }
}

void pdh_thread::add_realtime_mem_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    mem_filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add memory real-time filter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add memory real-time filter: " + key);
  }
}

void pdh_thread::add_realtime_proc_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    proc_filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add process real-time filter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add process real-time filter: " + key);
  }
}

void pdh_thread::set_path(const std::string &cpu_path, const std::string &mem_path, const std::string &proc_path) {
  cpu_filters_.set_path(cpu_path);
  mem_filters_.set_path(mem_path);
  proc_filters_.set_path(proc_path);
}

void pdh_thread::add_samples(std::shared_ptr<nscapi::settings_proxy> settings) {
  cpu_filters_.add_samples(settings);
  mem_filters_.add_samples(settings);
  proc_filters_.add_samples(settings);
}
