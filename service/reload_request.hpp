// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace nsclient {
namespace core {

// What a reload request asks for, and what the core does with it.
//
// Both are decisions rather than work, and both used to sit inline in
// NSClientT::reload() where nothing could reach them without a running agent.
// Getting the second one wrong is not a cosmetic failure: a reload run inline
// on a thread that is inside the module being reloaded unloads the code the
// call returns into.
namespace reload_request {

// What was asked for: `task` is "settings", "service" or a module, and
// `delayed` says the caller wants it handed to the scheduler rather than run
// before the call returns.
struct request {
  std::string task;
  bool delayed = false;
};

namespace detail {
// A prefix only counts when something follows it: "delayed," on its own names
// nothing to reload, so it is left alone and fails as the unknown module it is.
inline bool strip_prefix(const std::string &text, const std::string &prefix, std::string &rest) {
  if (text.size() <= prefix.size() || text.compare(0, prefix.size(), prefix) != 0) return false;
  rest = text.substr(prefix.size());
  return true;
}
}  // namespace detail

// Read the optional "delayed,", "delay," or "instant," in front of the task.
// Without one the reload is instant.
inline request parse(const std::string &text) {
  request result;
  result.task = text;
  if (detail::strip_prefix(text, "delayed,", result.task) || detail::strip_prefix(text, "delay,", result.task)) {
    result.delayed = true;
  } else {
    detail::strip_prefix(text, "instant,", result.task);
  }
  return result;
}

// Where the request came from, as far as it matters.
struct caller {
  // The calling thread is inside a call the core dispatched into a module -
  // any module, not necessarily the one being reloaded.
  bool inside_any_module = false;
  // The calling thread is inside the module the request names.
  bool inside_target = false;
  // The scheduler is there to take a deferred reload.
  bool scheduler_running = false;
};

enum class action {
  // Reload now, on the calling thread.
  run_inline,
  // Queue it on the scheduler; the call returns before the reload has applied.
  defer,
  // Neither is possible.
  refuse,
};

// A reload requested from inside a call the core dispatched into a module
// cannot be run inline on that thread. The thread may belong to a module other
// than the one it is dispatching: a Lua check served over NRPE runs on an NRPE
// pool thread while dispatching LUAScript, so core.reload("NRPEServer") - or
// "service", which reloads every module - would have the NRPE server stop and
// join its own pool from one of its members. Which module owns the thread is
// not something the core can see, so any reload of a module is deferred
// whenever the thread is inside any module; "settings" touches no module at
// all. The scheduler's worker is not inside any module, so the reload is
// handed over exactly as "delayed," does.
//
// With no scheduler to hand it to (`nscp unit`, `nscp client` without --boot),
// reloading the module the thread is inside would unload the code - a script
// and its interpreter state - that the call returns into, so that is refused.
// Reloading some other module from there is what the unit-test scripts do to
// apply the configuration they have just set, and none of those run on a
// thread the reloaded module owns, so that still applies inline.
inline action decide(const request &what, const caller &from) {
  if (what.delayed) return action::defer;
  if (what.task == "settings" || !from.inside_any_module) return action::run_inline;
  if (from.scheduler_running) return action::defer;
  if (what.task == "service" || from.inside_target) return action::refuse;
  return action::run_inline;
}

}  // namespace reload_request
}  // namespace core
}  // namespace nsclient
