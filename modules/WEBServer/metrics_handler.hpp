// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <string>

// The three latched renderings of the last metrics snapshot: the nested JSON
// of `/metrics`, the flat JSON of `/api/v2/metrics` and the OpenMetrics
// exposition of `/api/v2/openmetrics`. Each is a finished body, rendered once
// by the metrics thread and handed to any number of HTTP workers verbatim.
//
// The OpenMetrics rendering used to be a list of lines that the reader joined
// with newlines. It is one string now because the exposition is no longer a
// set of independent lines: `# TYPE` applies to the samples that follow it and
// the body has to end with `# EOF`, so whoever renders it owns the whole
// document, not a line at a time.
struct metrics_handler {
  void set(const std::string &metrics);
  void set_list(const std::string &metrics);
  void set_openmetrics(const std::string &metrics);
  std::string get_openmetrics();
  std::string get();
  std::string get_list();

 private:
  std::string metrics_;
  std::string metrics_list_;
  std::string open_metrics_;
  boost::timed_mutex mutex_;
};
