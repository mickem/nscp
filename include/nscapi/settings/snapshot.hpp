// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace nscapi {
namespace settings {

/**
 * An immutable value published once per reload and read by worker threads.
 *
 * `loadModuleEx` is re-entered with `reloadStart` on the live module while its
 * checks, submissions, metrics and log handlers are running on other threads.
 * Anything `settings.notify()` assigns there - a CA path, a hostname, an
 * interval, a whole connection config - is therefore written under concurrent
 * readers. A plain `std::string` member assigned in that window is a data
 * race, and for a string it is the kind that hands a reader a pointer into a
 * buffer that has just been freed.
 *
 * The rule this encodes: build the new value into a local, publish it in one
 * store, and have every reader take exactly one snapshot at entry and use that
 * for the whole call. A reader either sees the whole previous value or the
 * whole new one, never a half-applied mixture, and the value it holds stays
 * alive for as long as it holds it - the reload replaces the pointer, it does
 * not touch what the reader is reading.
 *
 * Only the publication is synchronised. Two concurrent writers still race with
 * each other, which is fine here because reloads are serialised by the core.
 *
 * Usage:
 *
 *     nscapi::settings::snapshot<config> config_;           // member
 *     config_.set(std::move(fresh));                        // loadModuleEx
 *     const std::shared_ptr<const config> cfg = config_();  // worker thread
 *     use(cfg->address, cfg->password);
 */
template <class T>
class snapshot {
 public:
  typedef std::shared_ptr<const T> value_type;

  snapshot() : value_(std::make_shared<const T>()) {}
  explicit snapshot(T value) : value_(std::make_shared<const T>(std::move(value))) {}

  // One snapshot per call, taken at entry: re-reading mid-call reintroduces
  // exactly the half-old / half-new mixture this exists to prevent.
  value_type get() const { return std::atomic_load(&value_); }
  value_type operator()() const { return get(); }

  void set(T value) { std::atomic_store(&value_, std::make_shared<const T>(std::move(value))); }
  void set(value_type value) { std::atomic_store(&value_, value ? std::move(value) : std::make_shared<const T>()); }

 private:
  // Never read or written directly: std::atomic_load / std::atomic_store are
  // what make the hand-over well defined. shared_ptr's own refcount is atomic,
  // the pointer itself is not.
  value_type value_;
};

}  // namespace settings
}  // namespace nscapi
