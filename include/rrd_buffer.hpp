// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/nsclient_exception.hpp>

#include <algorithm>
#include <cstddef>

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

/**
 * Rolling average buffer for the 1 Hz collectors: one second of samples, one
 * minute of per-second averages, one hour of per-minute averages.
 *
 * The three buffers are sized up front so the newest sample is always at
 * end() - 1. That also means they start out full of value-initialised entries,
 * which are not samples: averaging over them reported a saturated host as 0 %
 * for as long as it took the window to fill, and the "no data yet" guard the
 * checks wanted could never fire because the buffer was never empty. So the
 * number of real samples in each tier is counted, `has_data()` answers from
 * that, and `get_average()` never averages in a slot nothing was written to.
 *
 * A window longer than what has been sampled is answered from what there is
 * rather than padded with zeros: 30 seconds after a start, `time=5m` is the
 * average of those 30 seconds. Short of a window, not wrong about it.
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 */
template <class T>
struct rrd_buffer {
  typedef T value_type;
  typedef boost::circular_buffer<T> list_type;
  typedef typename list_type::const_iterator const_iterator;
  list_type seconds;
  list_type minutes;
  list_type hours;
  int second_counter;
  int minute_counter;

 public:
  rrd_buffer() : second_counter(0), minute_counter(0), filled_seconds_(0), filled_minutes_(0), filled_hours_(0) {
    seconds.resize(60);
    minutes.resize(60);
    hours.resize(24);
  }

  // True once at least one sample has been pushed. Checks report the
  // documented "no data yet" answer while this is false rather than an average
  // of nothing.
  bool has_data() const { return filled_seconds_ > 0; }

  value_type get_average(long time) const {
    value_type ret;
    // A negative window is nonsense: keep reporting the zero-initialised value.
    if (time < 0) return ret;
    // A zero-second window has no samples to average; report the newest one.
    if (time == 0) time = 1;
    if (static_cast<std::size_t>(time) <= seconds.size()) return average_of(seconds, std::min<long>(time, filled_seconds_), ret);
    time /= 60;
    if (static_cast<std::size_t>(time) <= minutes.size()) {
      // Nothing has been averaged into the minute tier yet - the first entry
      // lands a minute after the collector starts. Answer from the seconds we
      // do have rather than from sixty slots nothing was ever written to.
      if (filled_minutes_ > 0) return average_of(minutes, std::min<long>(time, filled_minutes_), ret);
      return average_of(seconds, filled_seconds_, ret);
    }
    time /= 60;
    // Out of range whatever has been sampled: the buffer simply does not go
    // back that far, so this stays an error rather than a short answer.
    if (static_cast<std::size_t>(time) >= hours.size()) throw nsclient::nsclient_exception("Size larger than buffer");
    if (filled_hours_ > 0) return average_of(hours, std::min<long>(time, filled_hours_), ret);
    if (filled_minutes_ > 0) return average_of(minutes, filled_minutes_, ret);
    return average_of(seconds, filled_seconds_, ret);
  }

  value_type calculate_avg(const list_type &buffer, const long samples) const {
    value_type ret;
    return average_of(buffer, samples, ret);
  }

  void push(const value_type &value) {
    seconds.push_back(value);
    if (filled_seconds_ < static_cast<long>(seconds.size())) filled_seconds_++;
    if (second_counter++ >= 59) {
      second_counter = 0;
      minutes.push_back(calculate_avg(seconds, filled_seconds_));
      if (filled_minutes_ < static_cast<long>(minutes.size())) filled_minutes_++;
      if (minute_counter++ >= 59) {
        minute_counter = 0;
        hours.push_back(calculate_avg(minutes, filled_minutes_));
        if (filled_hours_ < static_cast<long>(hours.size())) filled_hours_++;
      }
    }
  }

 private:
  // Average the newest `samples` entries of `buffer`, which is where the real
  // data sits: push_back drops the oldest, so the slots never written to are
  // the ones at the front.
  static value_type &average_of(const list_type &buffer, const long samples, value_type &ret) {
    if (samples <= 0) return ret;
    const long count = std::min<long>(samples, static_cast<long>(buffer.size()));
    for (const_iterator cit = buffer.end() - count; cit != buffer.end(); ++cit) {
      ret.add(*cit);
    }
    ret.normalize(static_cast<double>(count));
    return ret;
  }

  // Real samples in each tier, capped at that tier's capacity.
  long filled_seconds_;
  long filled_minutes_;
  long filled_hours_;
};
