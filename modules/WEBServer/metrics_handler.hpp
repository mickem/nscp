// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <string>

// The latched renderings of the last metrics snapshot: the nested JSON of
// `/metrics`, the flat JSON of `/api/v2/metrics`, and the two text expositions
// `/api/v2/openmetrics` negotiates between. Each is a finished body, rendered
// once by the metrics thread and handed to any number of HTTP workers verbatim.
//
// The OpenMetrics rendering used to be a list of lines that the reader joined
// with newlines. It is a string now because the exposition is no longer a set
// of independent lines: `# TYPE` applies to the samples that follow it and the
// body has to end with `# EOF`, so whoever renders it owns the whole document,
// not a line at a time.
//
// There are two of those strings because the two specifications disagree about
// what the metadata lines of a counter name - the family in OpenMetrics 1.0,
// the sample in the Prometheus text format - so one body served to both loses
// the type of every counter for one of them. They are latched together and the
// controller serves whichever matches the `Content-Type` it answers with.
struct metrics_handler {
  void set(const std::string &metrics);
  void set_list(const std::string &metrics);
  void set_openmetrics(const std::string &openmetrics, const std::string &prometheus_text);
  // `application/openmetrics-text; version=1.0.0`.
  std::string get_openmetrics();
  // `text/plain; version=0.0.4`.
  std::string get_prometheus_text();
  std::string get();
  std::string get_list();

 private:
  std::string metrics_;
  std::string metrics_list_;
  std::string open_metrics_;
  std::string prometheus_text_;
  boost::timed_mutex mutex_;
};
