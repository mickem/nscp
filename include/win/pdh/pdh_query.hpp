// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <win/pdh/pdh_counters.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_resolver.hpp>

namespace PDH {
namespace instance_providers {
struct container;
}
class PDHQuery : public subscriber {
 public:
  typedef std::shared_ptr<PDHCounter> counter_type;
  typedef std::list<counter_type> counter_list_type;
  typedef std::shared_ptr<instance_providers::container> container_type;
  counter_list_type counters_;
  // The $INSTANCE$ counters added to this query, whose children are in
  // counters_. Kept so their instance set can be expanded again.
  std::list<container_type> containers_;

  // A counter is stale once this many gathers in a row ended without data
  // for it: its instance has most likely gone.
  static constexpr int stale_after_ticks = 3;

  // What one container expands to right now.
  struct instance_expansion {
    container_type parent;
    std::list<pdh_object> instances;
  };
  typedef std::list<instance_expansion> expansion_list;
  // What apply_instances() changed, as (container alias, instance alias).
  struct instance_changes {
    typedef std::pair<std::string, std::string> entry;
    std::list<entry> added;
    std::list<entry> removed;
    // Still listed, but stale: re-added to the query so a reused instance
    // name (the next disk to attach gets the departed one's number) is bound
    // again.
    std::list<entry> reopened;
    std::list<std::string> errors;
  };
  PDH_HQUERY hQuery_;
  bool has_displayed_invalid_counter_;
  bool listener_registered_;

  PDHQuery() : hQuery_(nullptr), has_displayed_invalid_counter_(false), listener_registered_(false) {}
  virtual ~PDHQuery() noexcept;

  // Adds the counter (every instance of a wildcard one). On a query that has
  // been opened it is added to PDH right away (reopening a query a reload
  // left closed first), and a counter PDH rejects throws and is not kept; on
  // one that has not, it is added when the query is opened.
  void addCounter(const pdh_instance &instance);
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
  // instances come and go (check_pdh's) need it. Whatever needs it, a gather
  // takes at most one extra sample.
  void gatherData(bool ignore_errors = false, bool retry_missing = false);

  // Re-expanding the $INSTANCE$ counters is split in two so the caller can
  // enumerate (PDH calls that may take a while) without holding the lock its
  // readers take, and only hold it to apply the result.
  //
  // expand_instances() changes nothing. A container whose expansion fails is
  // left out of the result (and named in errors), so it keeps the instances
  // it has: a failed enumeration says nothing about which ones are gone.
  expansion_list expand_instances(std::list<std::string> &errors) const;
  // Adds the instances that appeared, drops the ones that are gone (from the
  // container and from the query), and re-adds stale ones that are still
  // listed. Instances that are still there keep their collector, so an rrd
  // counter keeps its history.
  instance_changes apply_instances(const expansion_list &expansions);
  bool has_instance_counters() const;
  // Whether any counter has gone stale_after_ticks gathers without data.
  bool has_stale_counters() const;
  void collect() const;

  PDH_HQUERY getQueryHandle() const;

 private:
  std::shared_ptr<impl_interface> impl_;
  std::string describe_counters() const;
  std::shared_ptr<impl_interface> impl() const;
  void add_instance_counter(const pdh_instance &instance);
  void remove_instance_counters(const std::shared_ptr<impl_interface> &impl, const pdh_instance &instance);
  void finish_counter(const counter_type &c, const pdh_error &status, bool ignore_errors);
};
}  // namespace PDH