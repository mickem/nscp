// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "metrics_handler.hpp"

#include <boost/thread/locks.hpp>

void metrics_handler::set(const std::string &metrics) {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  metrics_ = metrics;
}
void metrics_handler::set_list(const std::string &metrics) {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  metrics_list_ = metrics;
}

void metrics_handler::set_openmetrics(const std::string &openmetrics, const std::string &prometheus_text) {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  // Latched together: a scrape must never be able to see one body from this
  // snapshot and the other from the previous one.
  open_metrics_ = openmetrics;
  prometheus_text_ = prometheus_text;
}

std::string metrics_handler::get() {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return "";
  return metrics_;
}

std::string metrics_handler::get_list() {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return "";
  return metrics_list_;
}

std::string metrics_handler::get_openmetrics() {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return "";
  return open_metrics_;
}

std::string metrics_handler::get_prometheus_text() {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return "";
  return prometheus_text_;
}