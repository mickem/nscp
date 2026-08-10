// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// A flat ring of (timestamp, value) samples for rate-of-change estimation
// (check_drivesize full_in/rate). One buffer tracks one series (e.g. used
// bytes of one drive):
//
//  - append() subsamples to the configured cadence (the caller may offer
//    samples much more often than they are kept) and trims to retention;
//  - a change in the "context" value (e.g. the filesystem's total size)
//    invalidates the history: a regression window spanning a resize is
//    garbage, so the buffer starts over;
//  - slope_over() runs an ordinary least-squares regression of value over
//    time inside a lookback window. Timestamps are centered on the window
//    mean before summing so epoch-sized values do not eat the double
//    precision. Irregular gaps need no interpolation - OLS weighs whatever
//    samples exist;
//  - the estimate is valid only with >= 3 samples spanning >= 3x the
//    sampling interval (15 minutes at the default 5-minute cadence);
//  - encode()/decode() give a compact printable form (base + deltas) for
//    persistence, with optional downsampling on encode so the stored form
//    can be coarser than the in-memory ring.
//
// Pure logic, no clock: every entry point takes the current time as an
// argument, so tests inject timestamps.

#include <boost/optional.hpp>
#include <deque>
#include <sstream>
#include <string>

namespace trend {

struct sample {
  long long ts;     // seconds since epoch
  long long value;  // observed value (e.g. used bytes)
};

// Result of slope_over(): least-squares slope in value-units per second.
struct slope_result {
  bool valid;        // enough data to trust the slope
  double slope;      // units per second (negative = shrinking)
  long long span;    // seconds between oldest and newest sample used
  long long samples; // number of samples used

  slope_result() : valid(false), slope(0.0), span(0), samples(0) {}
};

class trend_buffer {
  std::deque<sample> samples_;
  long long interval_;   // minimum seconds between kept samples
  long long retention_;  // seconds of history to keep
  long long context_;    // e.g. total size; a change resets the history
  bool has_context_;

 public:
  trend_buffer() : interval_(300), retention_(7 * 24 * 3600), context_(0), has_context_(false) {}
  trend_buffer(const long long interval_sec, const long long retention_sec)
      : interval_(interval_sec > 0 ? interval_sec : 1), retention_(retention_sec > 0 ? retention_sec : 1), context_(0), has_context_(false) {}

  long long interval() const { return interval_; }
  long long retention() const { return retention_; }
  long long context() const { return context_; }
  std::size_t size() const { return samples_.size(); }
  bool empty() const { return samples_.empty(); }
  long long newest_ts() const { return samples_.empty() ? 0 : samples_.back().ts; }

  // Minimum span (seconds) a window must cover before a slope is trusted.
  long long min_span() const { return 3 * interval_; }

  // Offer a sample. Kept only if it is at least one interval after the last
  // kept sample; a timestamp at or before the last kept one (clock stepped
  // back) is ignored and simply leaves a gap. A context change (filesystem
  // resized) discards the history and starts over from this sample.
  void append(const long long ts, const long long value, const long long context) {
    if (has_context_ && context != context_) samples_.clear();
    context_ = context;
    has_context_ = true;
    if (!samples_.empty() && ts < samples_.back().ts + interval_) return;
    sample s;
    s.ts = ts;
    s.value = value;
    samples_.push_back(s);
    trim(ts);
  }

  // OLS slope of value over time across samples inside [now - window, now].
  slope_result slope_over(const long long now, const long long window) const {
    slope_result r;
    const long long cutoff = now - window;
    long long first_ts = 0, last_ts = 0;
    double sum_t = 0, sum_y = 0;
    for (const sample &s : samples_) {
      if (s.ts < cutoff || s.ts > now) continue;
      if (r.samples == 0) first_ts = s.ts;
      last_ts = s.ts;
      sum_t += static_cast<double>(s.ts);
      sum_y += static_cast<double>(s.value);
      r.samples++;
    }
    r.span = last_ts - first_ts;
    if (r.samples < 3 || r.span < min_span()) return r;
    const double mean_t = sum_t / static_cast<double>(r.samples);
    const double mean_y = sum_y / static_cast<double>(r.samples);
    double stt = 0, sty = 0;
    for (const sample &s : samples_) {
      if (s.ts < cutoff || s.ts > now) continue;
      const double dt = static_cast<double>(s.ts) - mean_t;
      stt += dt * dt;
      sty += dt * (static_cast<double>(s.value) - mean_y);
    }
    if (stt <= 0) return r;
    r.slope = sty / stt;
    r.valid = true;
    return r;
  }

  // Seconds until `current` (e.g. free bytes) reaches zero if consumed at
  // `r.slope` units/second. No value when the trend is invalid or the series
  // is not growing - "never" is an absence, not a number.
  static boost::optional<long long> project_zero(const long long current, const slope_result &r) {
    if (!r.valid || r.slope <= 0) return boost::none;
    if (current <= 0) return 0;
    return static_cast<long long>(static_cast<double>(current) / r.slope);
  }

  // Printable compact form: "1|context|ts:value|dt:dv|dt:dv...". With a
  // granularity, intermediate samples closer than `granularity` to the last
  // emitted one are skipped (the newest sample is always kept) so the
  // persisted form stays small.
  std::string encode(const long long granularity = 0) const {
    std::ostringstream ss;
    ss << "1|" << context_;
    long long prev_ts = 0, prev_value = 0;
    bool first = true;
    for (std::size_t i = 0; i < samples_.size(); ++i) {
      const sample &s = samples_[i];
      const bool last = i + 1 == samples_.size();
      if (!first && !last && s.ts - prev_ts < granularity) continue;
      if (first)
        ss << "|" << s.ts << ":" << s.value;
      else
        ss << "|" << (s.ts - prev_ts) << ":" << (s.value - prev_value);
      prev_ts = s.ts;
      prev_value = s.value;
      first = false;
    }
    return ss.str();
  }

  // Parse an encode()d string. Anything malformed yields an empty buffer
  // (starting fresh is always safe for a trend). Samples in the future
  // (clock stepped back since the save) or beyond retention are discarded;
  // non-monotonic timestamps abort the parse.
  static trend_buffer decode(const std::string &data, const long long interval_sec, const long long retention_sec, const long long now) {
    trend_buffer ret(interval_sec, retention_sec);
    std::istringstream ss(data);
    std::string field;
    if (!std::getline(ss, field, '|') || field != "1") return ret;
    if (!std::getline(ss, field, '|')) return ret;
    try {
      ret.context_ = std::stoll(field);
    } catch (...) {
      return trend_buffer(interval_sec, retention_sec);
    }
    ret.has_context_ = true;
    long long ts = 0, value = 0;
    bool first = true;
    while (std::getline(ss, field, '|')) {
      const std::size_t sep = field.find(':');
      if (sep == std::string::npos) return trend_buffer(interval_sec, retention_sec);
      long long f1 = 0, f2 = 0;
      try {
        f1 = std::stoll(field.substr(0, sep));
        f2 = std::stoll(field.substr(sep + 1));
      } catch (...) {
        return trend_buffer(interval_sec, retention_sec);
      }
      if (first) {
        ts = f1;
        value = f2;
        first = false;
      } else {
        if (f1 <= 0) return trend_buffer(interval_sec, retention_sec);
        ts += f1;
        value += f2;
      }
      if (ts > now || ts < now - retention_sec) continue;
      sample s;
      s.ts = ts;
      s.value = value;
      ret.samples_.push_back(s);
    }
    return ret;
  }

 private:
  void trim(const long long newest) {
    const long long cutoff = newest - retention_;
    while (!samples_.empty() && samples_.front().ts < cutoff) samples_.pop_front();
  }
};

}  // namespace trend
