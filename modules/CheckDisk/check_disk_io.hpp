// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <map>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>
#ifdef WIN32
#include <win/wmi/wmi_query.hpp>
#endif

namespace disk_io_check {

#ifdef WIN32
struct helper {
  // Win32_PerfFormattedData_PerfDisk_LogicalDisk provides pre-computed per-second rates.
  static std::string perf_query;
  static std::string perf_namespace;
};
#endif

struct disk_io {
  std::string name;
  long long read_bytes_per_sec;
  long long write_bytes_per_sec;
  long long reads_per_sec;
  long long writes_per_sec;
  long long queue_length;
  long long percent_disk_time;
  long long percent_idle_time;
  long long split_io_per_sec;

  disk_io()
      : read_bytes_per_sec(0),
        write_bytes_per_sec(0),
        reads_per_sec(0),
        writes_per_sec(0),
        queue_length(0),
        percent_disk_time(0),
        percent_idle_time(0),
        split_io_per_sec(0) {}
  disk_io(const disk_io &other) = default;
  disk_io &operator=(const disk_io &other) = default;

#ifdef WIN32
  void read_wmi(const wmi_impl::row &r);
#endif
  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  long long get_read_bytes_per_sec() const { return read_bytes_per_sec; }
  long long get_write_bytes_per_sec() const { return write_bytes_per_sec; }
  long long get_total_bytes_per_sec() const { return read_bytes_per_sec + write_bytes_per_sec; }
  long long get_reads_per_sec() const { return reads_per_sec; }
  long long get_writes_per_sec() const { return writes_per_sec; }
  long long get_iops() const { return reads_per_sec + writes_per_sec; }
  long long get_queue_length() const { return queue_length; }
  long long get_percent_disk_time() const { return percent_disk_time; }
  long long get_percent_idle_time() const { return percent_idle_time; }
  long long get_split_io_per_sec() const { return split_io_per_sec; }

  std::string show() const { return name; }
};

typedef std::list<disk_io> disks_type;

class disk_io_data {
  boost::shared_mutex mutex_;
  bool fetch_disk_io_;
  disks_type disks_;

 public:
  disk_io_data() : fetch_disk_io_(true) {}

  void fetch();
  disks_type get();
  void set(const disks_type &disks);

 private:
#ifdef WIN32
  static disks_type query_perf();
#else
  // Previous raw /proc/diskstats counters (per device) + timestamp, used to
  // compute per-second rates from the cumulative counters (Unix).
  std::map<std::string, std::vector<unsigned long long>> prev_raw_;
  long long prev_time_ms_ = 0;
#endif
};

namespace check {
void check_disk_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, disks_type data);
}  // namespace check

}  // namespace disk_io_check

namespace disk_free_check {

struct disk_free {
  std::string name;
  // Backing physical block device this filesystem lives on (e.g. "sda" for a
  // filesystem mounted from /dev/sda1). Populated on Unix so check_disk_health
  // can join a mountpoint's free space with the I/O of its underlying disk
  // (disk_io rows are keyed by physical device, disk_free rows by mountpoint).
  // Left empty on Windows, where disk_io and disk_free already share the same
  // logical-disk key (e.g. "C:") and join directly by name.
  std::string device;
  long long total;
  long long free;
  long long user_free;

  disk_free() : total(0), free(0), user_free(0) {}
  disk_free(const disk_free &other) = default;
  disk_free &operator=(const disk_free &other) = default;

  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  std::string get_device() const { return device; }
  long long get_total() const { return total; }
  long long get_free() const { return free; }
  long long get_used() const { return total - free; }
  long long get_user_free() const { return user_free; }
  long long get_free_pct() const { return total == 0 ? 0 : (free * 100 / total); }
  long long get_used_pct() const { return total == 0 ? 0 : ((total - free) * 100 / total); }

  std::string show() const { return name; }
};

typedef std::list<disk_free> drives_type;

class disk_free_data {
  boost::shared_mutex mutex_;
  drives_type drives_;

 public:
  disk_free_data() {}

  void fetch();
  drives_type get();
  void set(const drives_type &drives);
};

}  // namespace disk_free_check
