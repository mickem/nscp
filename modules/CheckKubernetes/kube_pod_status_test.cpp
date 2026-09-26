// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "kube_pod_status.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <string>

namespace {

kube_checks::pod_state state_of(const std::string &pod_json) {
  const boost::json::value v = boost::json::parse(pod_json);
  return kube_checks::derive_pod_state(v.as_object());
}

}  // namespace

TEST(KubePodStatus, RunningAndReady) {
  const auto s = state_of(R"({
    "metadata": {"name": "web"},
    "spec": {"containers": [{"name": "nginx"}, {"name": "sidecar"}]},
    "status": {"phase": "Running",
               "conditions": [{"type": "Ready", "status": "True"}],
               "containerStatuses": [
                 {"name": "nginx", "ready": true, "restartCount": 1, "state": {"running": {"startedAt": "2026-01-01T00:00:00Z"}}},
                 {"name": "sidecar", "ready": true, "restartCount": 0, "state": {"running": {}}}]}})");
  EXPECT_EQ(s.status, "Running");
  EXPECT_EQ(s.ready, 2);
  EXPECT_EQ(s.containers, 2);
  EXPECT_EQ(s.restarts, 1);
  EXPECT_TRUE(s.pod_ready);
  EXPECT_FALSE(s.terminating);
  EXPECT_FALSE(s.oom_killed);
}

TEST(KubePodStatus, CrashLoopBackOffWinsOverRunningPhase) {
  const auto s = state_of(R"({
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Running",
               "conditions": [{"type": "Ready", "status": "False"}],
               "containerStatuses": [
                 {"name": "app", "ready": false, "restartCount": 7,
                  "state": {"waiting": {"reason": "CrashLoopBackOff", "message": "back-off 5m0s restarting failed container"}},
                  "lastState": {"terminated": {"exitCode": 1, "reason": "Error"}}}]}})");
  EXPECT_EQ(s.status, "CrashLoopBackOff");
  EXPECT_EQ(s.restarts, 7);
  EXPECT_EQ(s.ready, 0);
  EXPECT_FALSE(s.pod_ready);
}

TEST(KubePodStatus, ImagePullBackOffWhilePending) {
  const auto s = state_of(R"({
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "containerStatuses": [{"name": "app", "ready": false, "restartCount": 0, "state": {"waiting": {"reason": "ImagePullBackOff"}}}]}})");
  EXPECT_EQ(s.status, "ImagePullBackOff");
}

TEST(KubePodStatus, CompletedJobPod) {
  const auto s = state_of(R"({
    "spec": {"containers": [{"name": "job"}]},
    "status": {"phase": "Succeeded",
               "containerStatuses": [{"name": "job", "ready": false, "restartCount": 0, "state": {"terminated": {"exitCode": 0, "reason": "Completed"}}}]}})");
  EXPECT_EQ(s.status, "Completed");
}

TEST(KubePodStatus, OomKilledNowAndBefore) {
  const auto now = state_of(R"({
    "spec": {"containers": [{"name": "job"}]},
    "status": {"phase": "Failed",
               "containerStatuses": [{"name": "job", "ready": false, "restartCount": 0, "state": {"terminated": {"exitCode": 137, "reason": "OOMKilled"}}}]}})");
  EXPECT_EQ(now.status, "OOMKilled");
  EXPECT_TRUE(now.oom_killed);

  const auto before = state_of(R"({
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Running", "conditions": [{"type": "Ready", "status": "True"}],
               "containerStatuses": [{"name": "app", "ready": true, "restartCount": 3, "state": {"running": {}},
                                      "lastState": {"terminated": {"exitCode": 137, "reason": "OOMKilled"}}}]}})");
  EXPECT_EQ(before.status, "Running") << "the pod is back up; the kill is in oom_killed";
  EXPECT_TRUE(before.oom_killed);
}

TEST(KubePodStatus, TerminatingAndNodeLost) {
  const auto terminating = state_of(R"({
    "metadata": {"deletionTimestamp": "2026-09-25T10:00:00Z"},
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Running", "containerStatuses": [{"name": "app", "ready": true, "state": {"running": {}}}]}})");
  EXPECT_EQ(terminating.status, "Terminating");
  EXPECT_TRUE(terminating.terminating);

  const auto lost = state_of(R"({
    "metadata": {"deletionTimestamp": "2026-09-25T10:00:00Z"},
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Running", "reason": "NodeLost"}})");
  EXPECT_EQ(lost.status, "Unknown");
}

TEST(KubePodStatus, InitContainerProgress) {
  const auto waiting = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}, {"name": "seed"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "initContainerStatuses": [
                 {"name": "migrate", "ready": true, "restartCount": 0, "state": {"terminated": {"exitCode": 0, "reason": "Completed"}}},
                 {"name": "seed", "ready": false, "restartCount": 0, "state": {"running": {}}}],
               "containerStatuses": [{"name": "app", "ready": false, "state": {"waiting": {"reason": "PodInitializing"}}}]}})");
  EXPECT_EQ(waiting.status, "Init:1/2");

  const auto crashing = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "initContainerStatuses": [{"name": "migrate", "ready": false, "restartCount": 4, "state": {"waiting": {"reason": "CrashLoopBackOff"}}}]}})");
  EXPECT_EQ(crashing.status, "Init:CrashLoopBackOff");
  EXPECT_EQ(crashing.restarts, 4);

  const auto failed = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "initContainerStatuses": [{"name": "migrate", "ready": false, "state": {"terminated": {"exitCode": 2}}}]}})");
  EXPECT_EQ(failed.status, "Init:ExitCode:2");

  const auto signalled = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "initContainerStatuses": [{"name": "migrate", "ready": false, "state": {"terminated": {"exitCode": 137, "signal": 9}}}]}})");
  EXPECT_EQ(signalled.status, "Init:Signal:9");
}

TEST(KubePodStatus, TerminatedWithoutReason) {
  const auto s = state_of(R"({
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Failed",
               "containerStatuses": [{"name": "app", "ready": false, "state": {"terminated": {"exitCode": 3}}}]}})");
  EXPECT_EQ(s.status, "ExitCode:3");
}

TEST(KubePodStatus, PhaseReasonAndNotReady) {
  const auto evicted = state_of(R"({
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Failed", "reason": "Evicted", "message": "The node was low on resource: memory."}})");
  EXPECT_EQ(evicted.status, "Evicted");

  // Only one of two containers has finished: kubectl shows NotReady, not
  // Completed, while the other is still running without a Ready condition.
  const auto not_ready = state_of(R"({
    "spec": {"containers": [{"name": "app"}, {"name": "helper"}]},
    "status": {"phase": "Running", "conditions": [{"type": "Ready", "status": "False"}],
               "containerStatuses": [
                 {"name": "app", "ready": true, "state": {"running": {}}},
                 {"name": "helper", "ready": false, "state": {"terminated": {"exitCode": 0, "reason": "Completed"}}}]}})");
  EXPECT_EQ(not_ready.status, "NotReady");
}

TEST(KubePodStatus, MissingFieldsDegradeGracefully) {
  const auto s = state_of(R"({"metadata": {"name": "bare"}})");
  EXPECT_EQ(s.status, "");
  EXPECT_EQ(s.containers, 0);
  EXPECT_EQ(s.ready, 0);
  EXPECT_EQ(s.restarts, 0);
  const auto pending = state_of(R"({"spec": {"containers": [{"name": "app"}]}, "status": {"phase": "Pending"}})");
  EXPECT_EQ(pending.status, "Pending");
  EXPECT_EQ(pending.containers, 1);
}

TEST(KubePodStatus, NativeSidecarsCountAsRunningOnceStarted) {
  // An init container with restartPolicy: Always (an Istio-style sidecar)
  // never terminates; once started it is part of the running pod, and
  // kubectl reads its `started` flag rather than reporting Init:0/1 forever.
  const auto running = state_of(R"({
    "spec": {"initContainers": [{"name": "istio-proxy", "restartPolicy": "Always"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Running",
               "conditions": [{"type": "Ready", "status": "True"}],
               "initContainerStatuses": [{"name": "istio-proxy", "ready": true, "started": true, "restartCount": 0, "state": {"running": {}}}],
               "containerStatuses": [{"name": "app", "ready": true, "started": true, "restartCount": 0, "state": {"running": {}}}]}})");
  EXPECT_EQ(running.status, "Running");
  EXPECT_EQ(running.ready, 2) << "the sidecar counts towards READY";
  EXPECT_EQ(running.containers, 2);
  EXPECT_TRUE(running.pod_ready);

  // Not started yet: the pod really is initialising.
  const auto starting = state_of(R"({
    "spec": {"initContainers": [{"name": "istio-proxy", "restartPolicy": "Always"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "initContainerStatuses": [{"name": "istio-proxy", "ready": false, "started": false, "restartCount": 0, "state": {"running": {}}}],
               "containerStatuses": [{"name": "app", "ready": false, "state": {"waiting": {"reason": "PodInitializing"}}}]}})");
  EXPECT_EQ(starting.status, "Init:0/1");

  // A sidecar that is crash-looping is still an initialisation problem.
  const auto crashing = state_of(R"({
    "spec": {"initContainers": [{"name": "istio-proxy", "restartPolicy": "Always"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Running",
               "initContainerStatuses": [{"name": "istio-proxy", "ready": false, "started": false, "restartCount": 5, "state": {"waiting": {"reason": "CrashLoopBackOff"}}}],
               "containerStatuses": [{"name": "app", "ready": true, "started": true, "state": {"running": {}}}]}})");
  EXPECT_EQ(crashing.status, "Init:CrashLoopBackOff");
  EXPECT_EQ(crashing.restarts, 5);

  // A plain init container that is running is not a sidecar: still Init:0/1
  // even with started=true.
  const auto plain = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "initContainerStatuses": [{"name": "migrate", "ready": false, "started": true, "state": {"running": {}}}]}})");
  EXPECT_EQ(plain.status, "Init:0/1");
}

TEST(KubePodStatus, TerminatingDoesNotHideATerminalPhase) {
  // A finished job pod being cleaned up under its TTL keeps Completed (or
  // its failure reason); kubectl only shows Terminating for pods that are
  // still Pending or Running.
  const auto completed = state_of(R"({
    "metadata": {"deletionTimestamp": "2026-09-25T10:00:00Z"},
    "spec": {"containers": [{"name": "job"}]},
    "status": {"phase": "Succeeded",
               "containerStatuses": [{"name": "job", "ready": false, "state": {"terminated": {"exitCode": 0, "reason": "Completed"}}}]}})");
  EXPECT_EQ(completed.status, "Completed");
  EXPECT_TRUE(completed.terminating) << "the deletion is still visible through the keyword";

  const auto failed = state_of(R"({
    "metadata": {"deletionTimestamp": "2026-09-25T10:00:00Z"},
    "spec": {"containers": [{"name": "job"}]},
    "status": {"phase": "Failed",
               "containerStatuses": [{"name": "job", "ready": false, "state": {"terminated": {"exitCode": 1, "reason": "Error"}}}]}})");
  EXPECT_EQ(failed.status, "Error");

  const auto pending = state_of(R"({
    "metadata": {"deletionTimestamp": "2026-09-25T10:00:00Z"},
    "spec": {"containers": [{"name": "app"}]},
    "status": {"phase": "Pending"}})");
  EXPECT_EQ(pending.status, "Terminating");
}

TEST(KubePodStatus, RestartsFollowTheKubectlColumn) {
  // Once the pod is initialised, RESTARTS is the main containers plus the
  // sidecars that keep running; an init container that retried while waiting
  // for a database is not held against the pod.
  const auto initialised = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}, {"name": "istio-proxy", "restartPolicy": "Always"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Running",
               "conditions": [{"type": "Initialized", "status": "True"}, {"type": "Ready", "status": "True"}],
               "initContainerStatuses": [
                 {"name": "migrate", "ready": false, "restartCount": 6, "state": {"terminated": {"exitCode": 0, "reason": "Completed"}}},
                 {"name": "istio-proxy", "ready": true, "started": true, "restartCount": 2, "state": {"running": {}}}],
               "containerStatuses": [{"name": "app", "ready": true, "started": true, "restartCount": 1, "state": {"running": {}}}]}})");
  EXPECT_EQ(initialised.status, "Running");
  EXPECT_EQ(initialised.restarts, 3) << "1 for app + 2 for the sidecar; the 6 migrate retries are gone";
  EXPECT_EQ(initialised.ready, 2);

  // While still initialising, the init containers' restarts are the column.
  const auto initialising = state_of(R"({
    "spec": {"initContainers": [{"name": "migrate"}], "containers": [{"name": "app"}]},
    "status": {"phase": "Pending",
               "conditions": [{"type": "Initialized", "status": "False"}],
               "initContainerStatuses": [{"name": "migrate", "ready": false, "restartCount": 6, "state": {"waiting": {"reason": "CrashLoopBackOff"}}}],
               "containerStatuses": [{"name": "app", "ready": false, "restartCount": 0, "state": {"waiting": {"reason": "PodInitializing"}}}]}})");
  EXPECT_EQ(initialising.status, "Init:CrashLoopBackOff");
  EXPECT_EQ(initialising.restarts, 6);
}

TEST(KubePodStatus, ACrashedSidecarOnAnInitialisedPodStillShowsTheMainContainers) {
  // kubectl enters the main-container loop when Initialized is True even
  // though the sidecar is unhealthy: 2/3 ready, not 0/3.
  const auto s = state_of(R"({
    "spec": {"initContainers": [{"name": "istio-proxy", "restartPolicy": "Always"}], "containers": [{"name": "app"}, {"name": "worker"}]},
    "status": {"phase": "Running",
               "conditions": [{"type": "Initialized", "status": "True"}, {"type": "Ready", "status": "False"}],
               "initContainerStatuses": [{"name": "istio-proxy", "ready": false, "started": false, "restartCount": 4, "state": {"waiting": {"reason": "CrashLoopBackOff"}}}],
               "containerStatuses": [
                 {"name": "app", "ready": true, "started": true, "restartCount": 0, "state": {"running": {}}},
                 {"name": "worker", "ready": true, "started": true, "restartCount": 0, "state": {"running": {}}}]}})");
  EXPECT_EQ(s.status, "Init:CrashLoopBackOff") << "the sidecar's trouble is still the headline";
  EXPECT_EQ(s.ready, 2);
  EXPECT_EQ(s.containers, 3);
  EXPECT_EQ(s.restarts, 0) << "a sidecar that is not running does not count towards RESTARTS";
}

TEST(KubePodStatus, ReadyRequiresARunningContainer) {
  // A status that lags after a crash can still say ready:true for a moment;
  // kubectl counts a container as ready only when it is also running.
  const auto s = state_of(R"({
    "spec": {"containers": [{"name": "app"}, {"name": "helper"}]},
    "status": {"phase": "Running",
               "conditions": [{"type": "Ready", "status": "True"}],
               "containerStatuses": [
                 {"name": "app", "ready": true, "restartCount": 0, "state": {"running": {}}},
                 {"name": "helper", "ready": true, "restartCount": 1, "state": {"waiting": {"reason": "CrashLoopBackOff"}}}]}})");
  EXPECT_EQ(s.status, "CrashLoopBackOff");
  EXPECT_EQ(s.ready, 1);
}
