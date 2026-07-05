// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
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

  void addCounter(const pdh_instance& instance);
  void removeAllCounters();

  bool has_counters() const;

  void on_unload() override;
  void on_reload() override;

  bool is_open() const;
  void open();
  void close();

  void gatherData(bool ignore_errors = false);
  inline void collect() const;

  PDH_HQUERY getQueryHandle() const;
};
}  // namespace PDH