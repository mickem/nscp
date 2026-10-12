// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <memory>
#include <string>
#include <win/pdh/pdh_counters.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_resolver.hpp>

namespace PDH {
class PDHQuery : public subscriber {
 public:
  typedef std::shared_ptr<PDHCounter> counter_type;
  typedef std::list<counter_type> counter_list_type;
  counter_list_type counters_;
  PDH_HQUERY hQuery_;
  bool has_displayed_invalid_counter_;
  bool listener_registered_;

  PDHQuery() : hQuery_(nullptr), has_displayed_invalid_counter_(false), listener_registered_(false) {}
  virtual ~PDHQuery() noexcept;

  // Adds the counter (every instance of a wildcard one). On a query that is
  // already open it is added to PDH right away, and a counter PDH rejects
  // throws and is not kept; otherwise it is added when the query is opened.
  void addCounter(const pdh_instance& instance);
  void removeAllCounters();

  bool has_counters() const;

  void on_unload() override;
  void on_reload() override;

  bool is_open() const;
  void open();
  void close();

  // ignore_errors: skip a counter that has no value this tick instead of
  // throwing. retry_missing: when the query or a counter has no data because
  // an instance is missing right now (PDH_NO_DATA, PDH_CSTATUS_NO_INSTANCE),
  // wait a second and sample once more before giving up. Opt-in: it costs a
  // second, which the 1 Hz collector cannot spend, and only counters whose
  // instances come and go (check_pdh's) need it.
  void gatherData(bool ignore_errors = false, bool retry_missing = false);
  void collect() const;

  PDH_HQUERY getQueryHandle() const;

 private:
  std::shared_ptr<impl_interface> impl_;
  std::string describe_counters() const;
  std::shared_ptr<impl_interface> impl() const;
};
}  // namespace PDH