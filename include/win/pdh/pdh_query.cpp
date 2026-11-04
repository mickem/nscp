// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <set>
#include <str/utf8.hpp>
#include <string>
#include <utility>
#include <win/pdh/pdh_collectors.hpp>
#include <win/pdh/pdh_query.hpp>

namespace PDH {
PDHQuery::~PDHQuery() noexcept {
  try {
    close();
  } catch (...) {
    // Destructors must not throw. Any error during shutdown is swallowed here;
    // callers that care about close errors should call close() explicitly first.
  }
}

void PDHQuery::addCounter(const pdh_instance& instance) {
  counter_list_type added;
  // Kept so its instances can be expanded again - once its counters are in,
  // not before: a rejected add is retried, and must not leave it listed.
  container_type parent;
  if (instance->has_instances()) {
    for (const pdh_instance& child : instance->get_instances()) {
      added.push_back(std::make_shared<PDHCounter>(child));
    }
    parent = std::dynamic_pointer_cast<instance_providers::container>(instance);
  } else
    added.push_back(std::make_shared<PDHCounter>(instance));

  // A query that was never opened adds the counters when it is. Once it has
  // been, they go in now, all of them or none, and only a counter PDH
  // accepted is kept: the counters already in the query keep their handles,
  // and with them the previous sample a rate counter is computed from. The
  // check is made under the implementation's lock, as a reload changes
  // hQuery_ under it.
  //
  // The list is changed under the lock too: a reload walks it from another
  // thread, and must see the new counters with their handles or not at all.
  if (listener_registered_ || hQuery_ != nullptr) {
    const std::shared_ptr<impl_interface> impl = this->impl();
    std::lock_guard<impl_interface> guard(*impl);
    // A reload that could not reopen the query left it closed but still
    // subscribed: reopen it first, as gatherData() does, rather than keep a
    // counter nobody validated, which every later reload would then fail on.
    if (listener_registered_ && hQuery_ == nullptr) on_reload();
    try {
      for (const counter_type& c : added) c->addToQuery(impl, hQuery_);
    } catch (...) {
      for (const counter_type& c : added) {
        try {
          c->remove();
        } catch (...) {
          // Best effort: the counter is not kept either way.
        }
      }
      throw;
    }
    counters_.splice(counters_.end(), added);
    if (parent) containers_.push_back(parent);
    return;
  }
  counters_.splice(counters_.end(), added);
  if (parent) containers_.push_back(parent);
}

bool PDHQuery::has_counters() const { return !counters_.empty(); }

void PDHQuery::removeAllCounters() {
  try {
    close();
  } catch (...) {
    // Caller asked to drop everything; close failures here are non-fatal.
  }
  counters_.clear();
  containers_.clear();
}

// The implementation this query was opened against, so the lock it holds and
// the calls it makes always go to the same object (open() binds it; until
// then the factory's current one).
std::shared_ptr<impl_interface> PDHQuery::impl() const { return impl_ ? impl_ : factory::get_impl(); }

void PDHQuery::on_unload() {
  if (hQuery_ == nullptr) return;
  for (const auto& c : counters_) {
    try {
      c->remove();
    } catch (...) {
      // Continue tearing down the rest; one bad counter must not strand the query.
    }
  }
  const PDH_HQUERY h = hQuery_;
  hQuery_ = nullptr;
  const pdh_error status = impl()->PdhCloseQuery(h);
  if (status.is_error()) throw pdh_exception("PdhCloseQuery failed", status);
}
void PDHQuery::on_reload() {
  if (hQuery_ != nullptr) return;
  const std::shared_ptr<impl_interface> impl = this->impl();
  const pdh_error status = impl->PdhOpenQuery(nullptr, 0, &hQuery_);
  if (status.is_error()) {
    hQuery_ = nullptr;
    throw pdh_exception("PdhOpenQuery failed", status);
  }
  try {
    for (const auto& c : counters_) {
      c->addToQuery(impl, getQueryHandle());
    }
  } catch (...) {
    const PDH_HQUERY h = hQuery_;
    hQuery_ = nullptr;
    for (const auto& c : counters_) {
      try {
        c->remove();
      } catch (...) {
      }
    }
    try {
      impl->PdhCloseQuery(h);
    } catch (...) {
    }
    throw;
  }
}

bool PDHQuery::is_open() const { return hQuery_ != nullptr; }

void PDHQuery::open() {
  // Under the implementation's lock, so a reload cannot run between opening
  // the query and subscribing it: the query would keep handles into the
  // library that reload freed, and never be called back to replace them.
  if (!listener_registered_) impl_ = factory::get_impl();
  const std::shared_ptr<impl_interface> impl = this->impl();
  std::lock_guard<impl_interface> guard(*impl);
  if (hQuery_ != nullptr) throw pdh_exception("query was already opened when trying to open query!");
  on_reload();
  // A query a reload left closed is still subscribed: reopening it is all
  // there is to do, and subscribing it again would call it back twice.
  if (listener_registered_) return;
  try {
    impl->add_listener(this);
    listener_registered_ = true;
  } catch (...) {
    try {
      on_unload();
    } catch (...) {
    }
    throw;
  }
}

void PDHQuery::close() {
  if (!listener_registered_ && hQuery_ == nullptr) {
    counters_.clear();
    containers_.clear();
    return;
  }
  // Unsubscribing and closing are one step for a reload, for the same reason
  // as in open().
  const std::shared_ptr<impl_interface> impl = this->impl();
  std::lock_guard<impl_interface> guard(*impl);
  if (listener_registered_) {
    try {
      impl->remove_listener(this);
    } catch (...) {
      // Best-effort: if the factory is gone or mutex is poisoned, we still
      // want close() to finish freeing local state.
    }
    listener_registered_ = false;
  }
  std::exception_ptr unload_error;
  if (hQuery_ != nullptr) {
    try {
      on_unload();
    } catch (...) {
      unload_error = std::current_exception();
    }
  }
  counters_.clear();
  containers_.clear();
  if (unload_error) std::rethrow_exception(unload_error);
}

void PDHQuery::gatherData(const bool ignore_errors, const bool retry_missing) {
  // A concurrent reload closes and reopens this query's handles, so each
  // read of one happens under the implementation's lock (as in collect()).
  // Step by step rather than across the loop: the sleep below must not hold
  // up every other PDH user.
  const std::shared_ptr<impl_interface> impl = this->impl();
  {
    // A reload that could not reopen this query (PDH failed to load again,
    // or PdhOpenQuery failed) left it closed but still subscribed. Reopen it
    // here, so it recovers once PDH works again instead of failing every
    // sample until the next reload.
    std::lock_guard<impl_interface> guard(*impl);
    if (listener_registered_ && hQuery_ == nullptr) on_reload();
  }
  const auto collect_counter = [&impl](const counter_type& c) {
    std::lock_guard<impl_interface> guard(*impl);
    return c->collect();
  };
  // A missing instance (a physical disk whose instance name changed, a RAID
  // volume that dropped out for a moment) is often back on the next sample
  // (#648), when the caller asks for that (retry_missing).
  bool resampled = false;
  try {
    collect();
  } catch (const pdh_no_data_exception&) {
    if (!retry_missing) throw;
    Sleep(1000);
    collect();
    resampled = true;
  }
  // Counters worth a second sample this tick. They are retried together,
  // after one shared sleep and re-collect, so a gather takes at most one
  // extra sample, whichever counters (and however many) need it. Retrying
  // each on its own cost a second per counter, and an RDS host with User
  // Profile Disks collects a PhysicalDisk instance per signed-in user: every
  // one that signed out left a counter that returns INVALID_DATA for good,
  // and added a second to every tick of the 1 Hz collector.
  std::list<std::pair<counter_type, pdh_error> > retry;
  bool retry_long = false;
  for (const counter_type& c : counters_) {
    const pdh_error status = collect_counter(c);
    if (status.is_invalid_data() && c->missing_ticks() == 0) {
      // First call after open() routinely returns INVALID_DATA for derived
      // counters (e.g. percentages need two samples), and so do percentage
      // counters under heavy fluctuation (#642, #906): give PDH a second
      // sample. Not for a counter that had no data on the previous gather
      // either - a second sample a second later is what the next tick is, so
      // a retry would only stall every other counter.
      retry.emplace_back(c, status);
      retry_long = true;
    } else if (status.is_no_instance() && retry_missing) {
      retry.emplace_back(c, status);
      retry_long = true;
    } else if (status.is_negative_denominator()) {
      retry.emplace_back(c, status);
    } else {
      finish_counter(c, status, ignore_errors);
    }
  }
  if (retry.empty()) return;
  if (!resampled) {
    // Already resampled for PDH_NO_DATA above: what was read is the second
    // sample, and another one would be a second extra second.
    Sleep(retry_long ? 1000 : 500);
    collect();
    for (auto& r : retry) r.second = collect_counter(r.first);
  }
  for (const auto& r : retry) {
    finish_counter(r.first, r.second, ignore_errors);
  }
}

void PDHQuery::finish_counter(const counter_type& c, const pdh_error& status, const bool ignore_errors) {
  if (status.is_invalid_data()) {
    // No data for this counter this tick. This is noise on percentage
    // counters under heavy fluctuation (#642, #906), or an instance that has
    // gone: skip it rather than failing the whole gather, and count it, so
    // an owner that re-expands instances can tell it has gone stale.
    c->note_missing();
    return;
  }
  if (status.is_no_instance()) {
    // Counted the same way, but still reported as below unless the caller
    // asked for best effort: check_pdh answers UNKNOWN for an instance that
    // is still missing after its retry (#648).
    c->note_missing();
  } else {
    c->note_present();
  }
  if (status.is_negative_denominator()) {
    // Still negative after the retry. Some counters simply cannot be
    // computed at times (HTTP Service Request Queues\MaxQueueItemAge on an
    // idle queue is a known one): with ignore_errors the caller asked for
    // best effort, so skip this counter for this tick - it keeps no
    // formatted value and readers fall back to their default - instead of
    // failing the whole gather, which checks then misreport as the object
    // not existing at all.
    if (ignore_errors) return;
    if (!has_displayed_invalid_counter_) {
      has_displayed_invalid_counter_ = true;
      throw pdh_exception(c->getName() + " Negative denominator issue (check FAQ for ways to solve this): ", status);
    }
  } else if (!ignore_errors && status.is_error()) {
    throw pdh_exception(c->getName() + " Failed to poll counter " + c->get_path(), status);
  }
}

bool PDHQuery::has_instance_counters() const { return !containers_.empty(); }

bool PDHQuery::has_stale_counters() const {
  return std::any_of(counters_.begin(), counters_.end(), [](const counter_type& c) { return c->missing_ticks() >= stale_after_ticks; });
}

PDHQuery::expansion_list PDHQuery::expand_instances(std::list<std::string>& errors) const {
  expansion_list ret;
  for (const container_type& parent : containers_) {
    try {
      bool no_instances = false;
      instance_expansion e;
      e.parent = parent;
      // An object with no instances at all expands to an empty list, which
      // drops every instance it had: they are all gone.
      e.instances = factory::expand_instances(parent->get_object(), no_instances);
      ret.push_back(e);
    } catch (const std::exception& e) {
      errors.push_back(parent->get_name() + ": " + utf8::utf8_from_native(e.what()));
    }
  }
  return ret;
}

PDHQuery::instance_changes PDHQuery::apply_instances(const expansion_list& expansions) {
  instance_changes changes;
  const std::shared_ptr<impl_interface> impl = this->impl();
  for (const instance_expansion& e : expansions) {
    // Only containers that are still part of this query.
    if (std::find(containers_.begin(), containers_.end(), e.parent) == containers_.end()) continue;
    const std::string parent_name = e.parent->get_name();
    std::set<std::string> listed;
    for (const pdh_object& o : e.instances) listed.insert(o.path);

    std::set<std::string> present;
    for (const pdh_instance& child : e.parent->get_instances()) {
      const std::string path = child->get_counter();
      if (listed.count(path) == 0 || present.count(path) > 0) {
        remove_instance_counters(impl, child);
        e.parent->remove_instance(child);
        changes.removed.emplace_back(parent_name, child->get_name());
        continue;
      }
      present.insert(path);
      const bool stale = std::any_of(counters_.begin(), counters_.end(),
                                     [&child](const counter_type& c) { return c->get_instance() == child && c->missing_ticks() >= stale_after_ticks; });
      if (!stale) continue;
      try {
        remove_instance_counters(impl, child);
        add_instance_counter(child);
        changes.reopened.emplace_back(parent_name, child->get_name());
      } catch (const std::exception& ex) {
        // Not in the query any more, so drop it from the container too: a
        // child the query does not sample would publish its last value for
        // good. The next expansion that lists it adds it back.
        e.parent->remove_instance(child);
        present.erase(path);
        changes.removed.emplace_back(parent_name, child->get_name());
        changes.errors.push_back(child->get_name() + ": " + utf8::utf8_from_native(ex.what()));
      }
    }

    for (const pdh_object& o : e.instances) {
      if (present.count(o.path) > 0) continue;
      try {
        const pdh_instance child = factory::create(o);
        add_instance_counter(child);
        e.parent->add_instance(child);
        present.insert(o.path);
        changes.added.emplace_back(parent_name, child->get_name());
      } catch (const std::exception& ex) {
        // Typically an instance that went away again between the expansion
        // and now; the next expansion tries again if it is still listed.
        changes.errors.push_back(o.alias + ": " + utf8::utf8_from_native(ex.what()));
      }
    }
  }
  return changes;
}

void PDHQuery::add_instance_counter(const pdh_instance& instance) {
  // Through addCounter(): straight into the open query, under the lock a
  // reload takes, reopening a query a reload left closed; a counter PDH
  // rejects throws and is not kept.
  addCounter(instance);
  // It has no sample yet: the next gather is its first, so a derived counter
  // reads INVALID_DATA once. Counted as one gather without data, so that
  // gather skips it instead of stalling every other counter for a retry; the
  // tick after has its second sample.
  counters_.back()->note_missing();
}

void PDHQuery::remove_instance_counters(const std::shared_ptr<impl_interface>& impl, const pdh_instance& instance) {
  std::lock_guard<impl_interface> guard(*impl);
  for (auto it = counters_.begin(); it != counters_.end();) {
    if ((*it)->get_instance() != instance) {
      ++it;
      continue;
    }
    try {
      (*it)->remove();
    } catch (...) {
      // The handle is forgotten either way (PDHCounter::remove), and the
      // instance it counted is most likely gone: nothing to recover.
    }
    it = counters_.erase(it);
  }
}

void PDHQuery::collect() const {
  // PdhCollectQueryData on a query without counters fails with PDH_NO_DATA,
  // which tells the caller nothing it did not already know; an empty query
  // simply has nothing to sample.
  if (counters_.empty()) return;
  const std::shared_ptr<impl_interface> impl = this->impl();
  std::lock_guard<impl_interface> guard(*impl);
  const pdh_error status = impl->PdhCollectQueryData(hQuery_);
  if (status.is_no_data()) throw pdh_no_data_exception("No data for " + describe_counters() + " (does the instance exist right now?)", status);
  if (status.is_error()) throw pdh_exception("PdhCollectQueryData failed for " + describe_counters(), status);
}

// The counter paths of this query, for error messages: a collect failure is
// about the query as a whole, and without the paths the report did not say
// which counter it was about. A wildcard can expand to many instances, so
// only the first few are listed.
std::string PDHQuery::describe_counters() const {
  constexpr std::size_t max_listed = 3;
  std::string ret;
  std::size_t listed = 0;
  for (const counter_type& c : counters_) {
    if (listed == max_listed) {
      ret += " and " + std::to_string(counters_.size() - max_listed) + " more";
      break;
    }
    if (listed++ > 0) ret += ", ";
    ret += c->get_path();
  }
  return ret;
}

PDH_HQUERY PDHQuery::getQueryHandle() const { return hQuery_; }
}  // namespace PDH
