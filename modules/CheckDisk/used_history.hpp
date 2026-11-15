// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// How much the used space of each volume changed over the last hour and the
// last day: the `used_change_1h` / `used_change_24h` metrics in the disk.free
// family and the check_drivesize keywords of the same names.
//
// The CheckDisk collector feeds every disk-free sample it takes (one per
// `collection interval`, 10 seconds by default) into a tracker, which keeps a
// small in-memory history per volume:
//
//   - a minute ring: one sample a minute, back to the newest sample that is at
//     least an hour old (about 61 samples);
//   - an hour ring: one sample an hour, back to the newest sample that is at
//     least a day old (about 25 samples).
//
// A change is the latest sample's used bytes minus those of the newest kept
// sample that is at least the window old - two real readings, nothing
// interpolated or extrapolated. Until such a sample exists (the agent started,
// the volume appeared, or its size changed less than a window ago) there is
// no change at all, which the metric shows by being absent and the keyword by
// rendering `unknown`. A sample that is far older than the window (the
// collector stopped sampling for a while) does not stand in for it either: the
// reference must be at most `slack_*` older than the window, so the 1h value
// always covers 60 to 70 minutes and the 24h value 24 to 26 hours.
//
// Sample times come from a monotonic clock in whole seconds and are passed in,
// so the logic is tested with a fake clock. The wall clock is no use for
// spacing samples: it steps (NTP, a VM's time sync), and a step back would
// make an hour look like a day or the other way round. Nothing is persisted:
// after a restart the values come back once the agent has been up for the
// window.
//
// Platform neutral: the collector keys volumes the way its disk-free rows are
// keyed (`C:` on Windows, the mount point on Unix).

#include <boost/optional.hpp>
#include <cstddef>
#include <deque>
#include <map>
#include <str/format.hpp>
#include <string>
#include <vector>

namespace used_history {

const long long seconds_per_minute = 60;
const long long seconds_per_hour = 3600;
const long long window_1h = seconds_per_hour;
const long long window_24h = 24 * seconds_per_hour;
// How much older than its window the reference sample may be. The rings
// sample once per spacing, so the reference is normally up to one spacing
// (plus one collection interval) older than the window; anything beyond the
// slack means the collector was not sampling and the value would describe a
// longer period than its name says.
const long long slack_1h = 10 * seconds_per_minute;
const long long slack_24h = 2 * seconds_per_hour;
// The most volumes tracked at once. A host with more (a container host
// mounting overlays by the hundred) tracks the first ones seen; the rest
// have no change values.
const std::size_t max_volumes = 64;

struct sample {
  long long at;  // monotonic seconds
  long long used;
};

// The change values of one volume; unset while not enough history exists.
struct changes {
  boost::optional<long long> change_1h;
  boost::optional<long long> change_24h;
};

typedef std::map<std::string, changes> change_map;

// Samples at least `spacing` apart, oldest first, reaching back to the newest
// sample that is at least `window` old and no further.
class ring {
 public:
  ring(const long long spacing, const long long window) : spacing_(spacing), window_(window) {}

  void add(const sample &s) {
    if (!samples_.empty() && s.at < samples_.back().at) samples_.clear();  // the clock went back: start over
    if (samples_.empty() || s.at - samples_.back().at >= spacing_) samples_.push_back(s);
    // Drop the oldest while the next one is old enough to be the reference
    // on its own.
    while (samples_.size() >= 2 && s.at - samples_[1].at >= window_) samples_.pop_front();
    // A hard cap on top, should the spacing ever be undercut.
    while (samples_.size() > capacity()) samples_.pop_front();
  }

  // latest.used minus the used bytes of the newest sample at least `window`
  // older than `latest`, provided that sample is at most `slack` older than
  // the window.
  boost::optional<long long> change(const sample &latest, const long long slack) const {
    for (std::deque<sample>::const_reverse_iterator it = samples_.rbegin(); it != samples_.rend(); ++it) {
      const long long age = latest.at - it->at;
      if (age < window_) continue;
      if (age > window_ + slack) return boost::none;
      return latest.used - it->used;
    }
    return boost::none;
  }

  std::size_t size() const { return samples_.size(); }
  bool empty() const { return samples_.empty(); }
  void clear() { samples_.clear(); }
  std::size_t capacity() const { return static_cast<std::size_t>(window_ / spacing_) + 2; }

 private:
  long long spacing_;
  long long window_;
  std::deque<sample> samples_;
};

class volume_history {
 public:
  volume_history() : total_(0), has_latest_(false), minutes_(seconds_per_minute, window_1h), hours_(seconds_per_hour, window_24h) {}

  // One reading. A volume whose size changed is a different volume (a resize,
  // or another disk mounted in the same place): its history starts over.
  void add(const long long at, const long long used, const long long total) {
    if (has_latest_ && total != total_) {
      minutes_.clear();
      hours_.clear();
    }
    total_ = total;
    latest_.at = at;
    latest_.used = used;
    has_latest_ = true;
    minutes_.add(latest_);
    hours_.add(latest_);
  }

  changes get() const {
    changes out;
    if (!has_latest_) return out;
    out.change_1h = minutes_.change(latest_, slack_1h);
    out.change_24h = hours_.change(latest_, slack_24h);
    return out;
  }

  const ring &minutes() const { return minutes_; }
  const ring &hours() const { return hours_; }

 private:
  long long total_;
  bool has_latest_;
  sample latest_;
  ring minutes_;
  ring hours_;
};

// One volume as the collector read it.
struct reading {
  std::string name;
  long long used;
  long long total;
};

// The history of every volume, fed once per successful disk-free fetch.
class tracker {
 public:
  // `readings` is the whole volume list of one fetch: a volume missing from
  // it has gone and loses its history. One that reports no size at all is
  // kept but not sampled - a volume that momentarily answers with nothing is
  // not a volume that was emptied.
  void update(const long long at, const std::vector<reading> &readings) {
    std::map<std::string, volume_history> next;
    for (const reading &r : readings) {
      std::map<std::string, volume_history>::iterator it = volumes_.find(r.name);
      if (it == volumes_.end()) {
        if (next.size() >= max_volumes || next.find(r.name) != next.end()) continue;
        if (r.total <= 0) continue;
        volume_history fresh;
        fresh.add(at, r.used, r.total);
        next[r.name] = fresh;
        continue;
      }
      volume_history &kept = next[r.name];
      kept = it->second;
      if (r.total > 0) kept.add(at, r.used, r.total);
    }
    volumes_.swap(next);
  }

  change_map get() const {
    change_map out;
    for (const std::map<std::string, volume_history>::value_type &v : volumes_) out[v.first] = v.second.get();
    return out;
  }

  std::size_t size() const { return volumes_.size(); }
  const volume_history *find(const std::string &name) const {
    const std::map<std::string, volume_history>::const_iterator it = volumes_.find(name);
    return it == volumes_.end() ? nullptr : &it->second;
  }

 private:
  std::map<std::string, volume_history> volumes_;
};

// The `total` row of check_drivesize: the changes of the drives that have one,
// added up (as `rate` pools), and unset while none has.
struct change_total {
  boost::optional<long long> change_1h;
  boost::optional<long long> change_24h;

  void append(const changes &c) {
    if (c.change_1h) change_1h = change_1h.get_value_or(0) + c.change_1h.value();
    if (c.change_24h) change_24h = change_24h.get_value_or(0) + c.change_24h.value();
  }
};

// A signed, auto-scaled rendering ("+1.5GB", "-300MB", "0B"), or "unknown".
inline std::string format_change(const boost::optional<long long> &v, const str::number_format &fmt = str::number_format()) {
  if (!v) return "unknown";
  // format_byte_units already signs a negative value.
  const long long value = v.value();
  return (value > 0 ? "+" : "") + str::format::format_byte_units(value, fmt);
}

}  // namespace used_history
