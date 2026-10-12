// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <exception>
#include <memory>
#include <mutex>
#include <string>
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
  if (instance->has_instances()) {
    for (const pdh_instance& child : instance->get_instances()) {
      added.push_back(std::make_shared<PDHCounter>(child));
    }
  } else
    added.push_back(std::make_shared<PDHCounter>(instance));

  // A query that was never opened adds the counters when it is. Once it has
  // been, they go in now, all of them or none, and only a counter PDH
  // accepted is kept: the counters already in the query keep their handles,
  // and with them the previous sample a rate counter is computed from. The
  // check is made under the implementation's lock, as a reload changes
  // hQuery_ under it.
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
  }
  counters_.splice(counters_.end(), added);
}

bool PDHQuery::has_counters() const { return !counters_.empty(); }

void PDHQuery::removeAllCounters() {
  try {
    close();
  } catch (...) {
    // Caller asked to drop everything; close failures here are non-fatal.
  }
  counters_.clear();
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
  if (unload_error) std::rethrow_exception(unload_error);
}

void PDHQuery::gatherData(const bool ignore_errors, const bool retry_missing) {
  // A concurrent reload closes and reopens this query's handles, so each
  // read of one happens under the implementation's lock (as in collect()).
  // Step by step rather than across the loop: the sleeps below must not hold
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
  // (#648). One extra sample per gather, whichever counter needed it.
  bool retried_missing = false;
  const auto resample_missing = [&]() {
    retried_missing = true;
    Sleep(1000);
    collect();
  };
  try {
    collect();
  } catch (const pdh_no_data_exception&) {
    if (!retry_missing) throw;
    resample_missing();
  }
  for (const counter_type c : counters_) {
    pdh_error status = collect_counter(c);
    if (status.is_no_instance() && retry_missing && !retried_missing) {
      resample_missing();
      status = collect_counter(c);
    }
    if (status.is_invalid_data()) {
      // First call after open() routinely returns INVALID_DATA for derived
      // counters (e.g. percentages need two samples). Give PDH a second
      // sample and retry.
      Sleep(1000);
      collect();
      status = collect_counter(c);
      if (status.is_invalid_data()) {
        // Still no data. This is noise on percentage counters under heavy
        // fluctuation (#642, #906) — skip this counter for this tick rather
        // than failing the whole gather.
        continue;
      }
    }
    if (status.is_negative_denominator()) {
      Sleep(500);
      collect();
      status = collect_counter(c);
    }
    if (status.is_negative_denominator()) {
      // Still negative after the retry. Some counters simply cannot be
      // computed at times (HTTP Service Request Queues\MaxQueueItemAge on an
      // idle queue is a known one): with ignore_errors the caller asked for
      // best effort, so skip this counter for this tick - it keeps no
      // formatted value and readers fall back to their default - instead of
      // failing the whole gather, which checks then misreport as the object
      // not existing at all.
      if (ignore_errors) continue;
      if (!has_displayed_invalid_counter_) {
        has_displayed_invalid_counter_ = true;
        throw pdh_exception(c->getName() + " Negative denominator issue (check FAQ for ways to solve this): ", status);
      }
    } else if (!ignore_errors && status.is_error()) {
      throw pdh_exception(c->getName() + " Failed to poll counter " + c->get_path(), status);
    }
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
