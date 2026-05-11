/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <exception>
#include <memory>
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
  if (instance->has_instances()) {
    for (const pdh_instance& child : instance->get_instances()) {
      counters_.push_back(std::make_shared<PDHCounter>(child));
    }
  } else
    counters_.push_back(std::make_shared<PDHCounter>(instance));
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
  const pdh_error status = factory::get_impl()->PdhCloseQuery(h);
  if (status.is_error()) throw pdh_exception("PdhCloseQuery failed", status);
}
void PDHQuery::on_reload() {
  if (hQuery_ != nullptr) return;
  const pdh_error status = factory::get_impl()->PdhOpenQuery(nullptr, 0, &hQuery_);
  if (status.is_error()) {
    hQuery_ = nullptr;
    throw pdh_exception("PdhOpenQuery failed", status);
  }
  try {
    for (const auto& c : counters_) {
      c->addToQuery(getQueryHandle());
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
      factory::get_impl()->PdhCloseQuery(h);
    } catch (...) {
    }
    throw;
  }
}

bool PDHQuery::is_open() const { return hQuery_ != nullptr; }

void PDHQuery::open() {
  if (hQuery_ != nullptr) throw pdh_exception("query was already opened when trying to open query!");
  on_reload();
  try {
    factory::get_impl()->add_listener(this);
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
  if (listener_registered_) {
    try {
      factory::get_impl()->remove_listener(this);
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

void PDHQuery::gatherData(bool ignore_errors) {
  collect();
  for (counter_type c : counters_) {
    pdh_error status = c->collect();
    if (status.is_invalid_data()) {
      Sleep(1000);
      collect();
      status = c->collect();
    }
    if (status.is_negative_denominator()) {
      Sleep(500);
      collect();
      status = c->collect();
    }
    if (status.is_negative_denominator()) {
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
  const pdh_error status = factory::get_impl()->PdhCollectQueryData(hQuery_);
  if (status.is_error()) throw pdh_exception("PdhCollectQueryData failed: ", status);
}

PDH_HQUERY PDHQuery::getQueryHandle() const { return hQuery_; }
}  // namespace PDH
