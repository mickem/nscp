// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/nsclient_exception.hpp>

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

#include <cstddef>

/**
 * Round-robin sample store behind the 1 Hz collectors: the last 60 seconds,
 * the last 60 minutes and the last 24 hours, each level averaged from the one
 * below as it fills.
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
  list_type seconds;
  list_type minutes;
  list_type hours;
  int second_counter;
  int minute_counter;
  // How many samples push() has received, saturating at the span the buffers
  // cover. The rings are created full - resize() value-initialises every slot
  // - so their size says nothing about how much has actually been sampled;
  // this does, and get_average() limits its window to it.
  std::size_t sample_count;

 public:
  rrd_buffer() : second_counter(0), minute_counter(0), sample_count(0) {
    seconds.resize(60);
    minutes.resize(60);
    hours.resize(24);
  }
  // True once at least one sample has been pushed. Before that every slot is
  // a value-initialised zero, and averaging those reports an idle machine.
  bool has_data() const { return sample_count > 0; }
  // How many seconds get_average() can answer for: the samples pushed so far,
  // up to the span of the buffers.
  std::size_t sampled_seconds() const { return sample_count; }

  value_type get_average(long time) const {
    value_type ret;
    // A negative window is nonsense: keep reporting the zero-initialised value.
    if (time < 0) return ret;
    // A zero-second window has no samples to average; report the newest one.
    if (time == 0) time = 1;
    // Beyond what the buffers could ever hold, sampled or not.
    if (static_cast<std::size_t>(time) >= hours.size() * 60 * 60) throw nsclient::nsclient_exception("Size larger than buffer");
    // Nothing has been sampled: there is no average to report.
    if (sample_count == 0) return ret;
    // A window longer than what has been sampled so far is answered from what
    // has. Right after a start or a reload a five-minute average was otherwise
    // one real minute averaged with four minutes of empty slots, and read 0 %
    // on a saturated host until the ring had filled.
    if (static_cast<std::size_t>(time) > sample_count) time = static_cast<long>(sample_count);
    if (static_cast<size_t>(time) <= seconds.size()) {
      for (typename list_type::const_iterator cit = seconds.end() - time; cit != seconds.end(); ++cit) {
        ret.add(*cit);
      }
      ret.normalize(time);
      return ret;
    }
    time /= 60;
    if (static_cast<size_t>(time) <= minutes.size()) {
      for (typename list_type::const_iterator cit = minutes.end() - time; cit != minutes.end(); ++cit) {
        ret.add(*cit);
      }
      ret.normalize(time);
      return ret;
    }
    time /= 60;
    if (static_cast<size_t>(time) >= hours.size()) throw nsclient::nsclient_exception("Size larger than buffer");
    for (typename list_type::const_iterator cit = hours.end() - time; cit != hours.end(); ++cit) {
      ret.add(*cit);
    }
    ret.normalize(time);
    return ret;
  }
  value_type calculate_avg(list_type &buffer) const {
    value_type ret;
    for (const value_type &entry : buffer) {
      ret.add(entry);
    }
    ret.normalize(static_cast<double>(buffer.size()));
    return ret;
  }

  void push(const value_type &value) {
    if (sample_count < hours.size() * 60 * 60) ++sample_count;
    seconds.push_back(value);
    if (second_counter++ >= 59) {
      second_counter = 0;
      minutes.push_back(calculate_avg(seconds));
      if (minute_counter++ >= 59) {
        minute_counter = 0;
        hours.push_back(calculate_avg(minutes));
      }
    }
  }
};
