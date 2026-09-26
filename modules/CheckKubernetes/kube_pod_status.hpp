// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The kubectl STATUS column, derived from a pod object the way `kubectl get
// pods` does it (printers.go, printPod): Running, Completed, CrashLoopBackOff,
// ImagePullBackOff, OOMKilled, Terminating, Init:1/2, ... Operators recognise
// those words, and `phase` alone hides a crash loop behind "Running".
//
// Header only, on top of the tolerant accessors, so it is unit-tested against
// pod JSON without the rest of the module.

#include <boost/json.hpp>
#include <set>
#include <string>

#include "kube_json.hpp"

namespace kube_checks {

struct pod_state {
  std::string status;        // the kubectl STATUS column
  long long restarts = 0;    // sum of container restart counts
  long long ready = 0;       // containers reporting ready (restartable init containers included)
  long long containers = 0;  // containers in the pod spec (restartable init containers included)
  bool terminating = false;  // deletionTimestamp is set
  bool oom_killed = false;   // a container's current or last termination was OOMKilled
  bool pod_ready = false;    // the Ready condition is True
};

namespace detail {

inline const boost::json::object *state_of(const boost::json::object &container_status, const char *which, const char *key) {
  if (const boost::json::object *state = get_obj(container_status, which)) return get_obj(*state, key);
  return nullptr;
}

inline bool was_oom_killed(const boost::json::object &container_status) {
  for (const char *which : {"state", "lastState"}) {
    if (const boost::json::object *t = state_of(container_status, which, "terminated")) {
      if (get_str(*t, "reason") == "OOMKilled") return true;
    }
  }
  return false;
}

// The names of the init containers declared with restartPolicy: Always -
// native sidecars (Istio, log shippers), which run for the pod's lifetime and
// count as started rather than as unfinished initialisation.
inline std::set<std::string> restartable_init_containers(const boost::json::object &spec) {
  std::set<std::string> names;
  if (const boost::json::array *inits = get_arr(spec, "initContainers")) {
    for (const auto &v : *inits) {
      if (v.is_object() && get_str(v.as_object(), "restartPolicy") == "Always") names.insert(get_str(v.as_object(), "name"));
    }
  }
  return names;
}

inline bool is_terminal_phase(const std::string &phase) { return phase == "Succeeded" || phase == "Failed"; }

}  // namespace detail

inline pod_state derive_pod_state(const boost::json::object &pod) {
  pod_state out;
  const boost::json::object *metadata = get_obj(pod, "metadata");
  const boost::json::object *spec = get_obj(pod, "spec");
  const boost::json::object *status = get_obj(pod, "status");
  static const boost::json::object empty;
  if (!status) status = &empty;
  if (!spec) spec = &empty;

  const std::set<std::string> sidecars = detail::restartable_init_containers(*spec);
  if (const boost::json::array *containers = get_arr(*spec, "containers")) out.containers = static_cast<long long>(containers->size());
  out.containers += static_cast<long long>(sidecars.size());
  out.pod_ready = condition_is_true(*status, "Ready");

  const std::string phase = get_str(*status, "phase");
  std::string reason = phase;
  if (!get_str(*status, "reason").empty()) reason = get_str(*status, "reason");

  // Init containers first: an unfinished one is what the pod is waiting on.
  // A finished one, or a started sidecar, is skipped. Their restarts count
  // only while the pod is initialising; once it is, kubectl's RESTARTS is the
  // main containers plus the sidecars that keep running (a migrate container
  // that retried while waiting for a database is not held against the pod).
  bool initializing = false;
  long long sidecar_restarts = 0;
  long long init_total = 0;
  if (const boost::json::array *inits = get_arr(*spec, "initContainers")) init_total = static_cast<long long>(inits->size());
  if (const boost::json::array *inits = get_arr(*status, "initContainerStatuses")) {
    long long i = 0;
    for (const auto &v : *inits) {
      if (!v.is_object()) {
        ++i;
        continue;
      }
      const boost::json::object &cs = v.as_object();
      out.restarts += get_num(cs, "restartCount");
      if (detail::was_oom_killed(cs)) out.oom_killed = true;
      const boost::json::object *terminated = detail::state_of(cs, "state", "terminated");
      const boost::json::object *waiting = detail::state_of(cs, "state", "waiting");
      if (terminated && get_num(*terminated, "exitCode") == 0) {
        ++i;
        continue;  // this init container finished fine
      }
      if (sidecars.count(get_str(cs, "name")) != 0 && get_bool(cs, "started")) {
        // A native sidecar that is up: it is part of the running pod, not
        // of its initialisation (kubectl reads the same `started` flag).
        if (get_bool(cs, "ready")) ++out.ready;
        sidecar_restarts += get_num(cs, "restartCount");
        ++i;
        continue;
      }
      if (terminated) {
        const std::string term_reason = get_str(*terminated, "reason");
        if (term_reason.empty()) {
          const long long signal = get_num(*terminated, "signal");
          reason = signal != 0 ? "Init:Signal:" + std::to_string(signal) : "Init:ExitCode:" + std::to_string(get_num(*terminated, "exitCode"));
        } else {
          reason = "Init:" + term_reason;
        }
      } else if (waiting && !get_str(*waiting, "reason").empty() && get_str(*waiting, "reason") != "PodInitializing") {
        reason = "Init:" + get_str(*waiting, "reason");
      } else {
        reason = "Init:" + std::to_string(i) + "/" + std::to_string(init_total);
      }
      initializing = true;
      break;
    }
  }

  // The main containers are looked at once initialisation is over - or, as
  // kubectl does, when the Initialized condition says it is even though a
  // sidecar is currently unhealthy: a crashed sidecar on an otherwise
  // healthy pod still reports 2/3 ready.
  if (!initializing || condition_is_true(*status, "Initialized")) {
    out.restarts = sidecar_restarts;
    bool has_running = false;
    if (const boost::json::array *statuses = get_arr(*status, "containerStatuses")) {
      // kubectl walks the list backwards, so the first container's reason wins.
      for (auto it = statuses->rbegin(); it != statuses->rend(); ++it) {
        if (!it->is_object()) continue;
        const boost::json::object &cs = it->as_object();
        out.restarts += get_num(cs, "restartCount");
        if (detail::was_oom_killed(cs)) out.oom_killed = true;
        const boost::json::object *waiting = detail::state_of(cs, "state", "waiting");
        const boost::json::object *terminated = detail::state_of(cs, "state", "terminated");
        const boost::json::object *running = detail::state_of(cs, "state", "running");
        if (waiting && !get_str(*waiting, "reason").empty()) {
          reason = get_str(*waiting, "reason");
        } else if (terminated && !get_str(*terminated, "reason").empty()) {
          reason = get_str(*terminated, "reason");
        } else if (terminated) {
          const long long signal = get_num(*terminated, "signal");
          reason = signal != 0 ? "Signal:" + std::to_string(signal) : "ExitCode:" + std::to_string(get_num(*terminated, "exitCode"));
        } else if (get_bool(cs, "ready") && running) {
          // Ready counts only for a container that is actually running: a
          // status that lags after a crash still says ready for a moment.
          has_running = true;
          ++out.ready;
        }
      }
    }
    // A pod is Completed only when nothing in it is still running.
    if (!initializing && reason == "Completed" && has_running) reason = out.pod_ready ? "Running" : "NotReady";
  }

  // A pod held back by a scheduling gate: kubectl reads the PodScheduled
  // condition's reason rather than leaving it at Pending.
  if (const boost::json::array *conditions = get_arr(*status, "conditions")) {
    for (const auto &c : *conditions) {
      if (!c.is_object()) continue;
      if (get_str(c.as_object(), "type") == "PodScheduled" && get_str(c.as_object(), "reason") == "SchedulingGated") reason = "SchedulingGated";
    }
  }

  if (metadata && !get_str(*metadata, "deletionTimestamp").empty()) {
    out.terminating = true;
    // A finished pod being cleaned up (a job under its TTL) keeps its verdict:
    // Completed or Error say more than Terminating. kubectl does the same.
    if (get_str(*status, "reason") == "NodeLost") {
      reason = "Unknown";
    } else if (!detail::is_terminal_phase(phase)) {
      reason = "Terminating";
    }
  }
  out.status = reason;
  return out;
}

}  // namespace kube_checks
