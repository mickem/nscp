// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>

namespace nscapi {
namespace settings_helper {

// A module's settings struct as one immutable snapshot.
//
// loadModuleEx runs on the loading thread - on every settings reload, not
// only at start - while checks, metrics and facts rounds read the same
// struct on theirs, and a std::string being rewritten under a reader is a
// data race. So a load binds its keys to a fresh struct, and once notify()
// has filled it, set() swaps it in whole; every reader calls get() once and
// reads the copy it was handed for as long as it needs it. Never empty: a
// module that has not loaded yet reads a default-constructed T.
//
// std::atomic_load / atomic_store on shared_ptr is what C++17 has for this;
// C++20 deprecates the pair in favour of std::atomic<std::shared_ptr>, and
// keeping the two calls here means that move lands in one place.
template <class T>
class snapshot {
 public:
  snapshot() : current_(std::make_shared<const T>()) {}

  std::shared_ptr<const T> get() const { return std::atomic_load(&current_); }
  void set(const T &fresh) { std::atomic_store(&current_, std::make_shared<const T>(fresh)); }

 private:
  std::shared_ptr<const T> current_;
};

}  // namespace settings_helper
}  // namespace nscapi
