// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <map>
#include <stdexcept>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "check_docker.hpp"
#include "docker_endpoint.hpp"

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

// A daemon serving canned payloads; records the last path requested.
struct fake_daemon {
  std::string payload;
  std::string last_path;

  docker_checks::fetcher_factory factory() {
    return [this](const std::string &, int) -> docker_checks::fetcher {
      return [this](const std::string &path) -> std::string {
        last_path = path;
        return payload;
      };
    };
  }
};

docker_checks::fetcher_factory refusing_daemon(const std::string &error) {
  return [error](const std::string &, int) -> docker_checks::fetcher {
    return [error](const std::string &) -> std::string { throw std::runtime_error(error); };
  };
}

docker_checks::settings unix_defaults() {
  docker_checks::settings s;
  s.endpoint = "/var/run/docker.sock";
  return s;
}

PB::Common::ResultCode run_containers(const docker_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                      PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_docker");
  for (const std::string &a : args) request.add_arguments(a);
  docker_checks::check_containers(unix_defaults(), request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_info(const docker_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_docker_info");
  for (const std::string &a : args) request.add_arguments(a);
  docker_checks::check_info(unix_defaults(), request, &response, factory);
  return response.result();
}

// Two containers as the daemon reports them: one healthy web server with a
// published port, one exited one-shot job. Trimmed to the fields the check
// reads plus some it must ignore.
const char *TWO_CONTAINERS = R"json([
  {
    "Id": "aaa111", "Names": ["/web"], "Image": "nginx:1.25", "ImageID": "sha256:123",
    "Command": "nginx -g 'daemon off;'", "Created": 1700000000,
    "State": "running", "Status": "Up 3 hours (healthy)",
    "Ports": [{"IP": "0.0.0.0", "PrivatePort": 80, "PublicPort": 8080, "Type": "tcp"}],
    "Labels": {"env": "prod"},
    "NetworkSettings": {"Networks": {"frontend": {"IPAddress": "172.20.0.2"}}}
  },
  {
    "Id": "bbb222", "Names": ["/job"], "Image": "alpine", "ImageID": "sha256:456",
    "Command": "sh -c exit", "Created": 1700000100,
    "State": "exited", "Status": "Exited (1) 2 hours ago",
    "Ports": [], "Labels": {},
    "NetworkSettings": {"Networks": {}}
  }
])json";

}  // namespace

// --- check_docker ------------------------------------------------------------

TEST(CheckDocker, RunningContainersAreOkAndExitedTripsCritical) {
  fake_daemon daemon;
  daemon.payload = TWO_CONTAINERS;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(), {"all=true"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("job=exited"), std::string::npos) << join_lines(response);
  EXPECT_EQ(daemon.last_path, "/containers/json?all=true");
}

TEST(CheckDocker, DefaultOmitsStoppedContainers) {
  fake_daemon daemon;
  daemon.payload = "[]";
  PB::Commands::QueryResponseMessage::Response response;
  run_containers(daemon.factory(), {}, response);
  EXPECT_EQ(daemon.last_path, "/containers/json");
}

TEST(CheckDocker, KeywordsAreExposed) {
  fake_daemon daemon;
  daemon.payload = TWO_CONTAINERS;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(),
                           {"filter=names = 'web'", "detail-syntax=%(names)|%(image)|%(health)|%(ip)|%(ports)|%(labels)", "top-syntax=${list}"}, response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "web|nginx:1.25|healthy|172.20.0.2|0.0.0.0:8080->80/tcp|env=prod");
}

TEST(CheckDocker, HealthFilterViaHasHealthCheck) {
  // The where-parser has no empty-string literal, so scoping to containers
  // with a health check goes through has_health_check.
  fake_daemon daemon;
  daemon.payload = TWO_CONTAINERS;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(),
                           {"all=true", "filter=has_health_check = 1", "critical=health = 'unhealthy'", "detail-syntax=%(names)=%(health)",
                            "top-syntax=${status}: ${list}"},
                           response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: web=healthy");
}

TEST(CheckDocker, RequiredContainerPresentAndRunningIsOk) {
  fake_daemon daemon;
  daemon.payload = TWO_CONTAINERS;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(), {"container=web"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  // Requesting specific containers always asks for the full list.
  EXPECT_EQ(daemon.last_path, "/containers/json?all=true");
}

TEST(CheckDocker, RequiredMissingContainerIsCritical) {
  fake_daemon daemon;
  daemon.payload = TWO_CONTAINERS;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(), {"container=web", "container=database"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("database=missing"), std::string::npos) << join_lines(response);
}

TEST(CheckDocker, RequiredExitedContainerIsCritical) {
  fake_daemon daemon;
  daemon.payload = TWO_CONTAINERS;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(), {"container=job"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("job=exited"), std::string::npos) << join_lines(response);
}

TEST(CheckDocker, DaemonFailureIsUnknownWithMessage) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(refusing_daemon("connection refused"), {}, response), PB::Common::ResultCode::UNKNOWN);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("Failed to connect to docker daemon at '/var/run/docker.sock'"), std::string::npos) << msg;
  EXPECT_NE(msg.find("connection refused"), std::string::npos) << msg;
}

TEST(CheckDocker, MalformedPayloadIsUnknown) {
  fake_daemon daemon;
  daemon.payload = "this is not json";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(), {}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Failed to parse docker daemon response"), std::string::npos) << join_lines(response);
}

TEST(CheckDocker, SparseContainerObjectsDoNotCrash) {
  // A minimal (podman-ish) payload with almost everything missing.
  fake_daemon daemon;
  daemon.payload = R"json([{"Id": "ccc333", "State": "running"}])json";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_containers(daemon.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckDocker, RemoteEndpointIsRefused) {
  fake_daemon daemon;
  daemon.payload = "[]";
  PB::Commands::QueryResponseMessage::Response response;
#ifdef WIN32
  const std::string remote = "host=\\\\attacker\\pipe\\x";
#else
  const std::string remote = "host=../../etc/passwd";
#endif
  EXPECT_EQ(run_containers(daemon.factory(), {remote}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing docker endpoint"), std::string::npos) << join_lines(response);
  EXPECT_TRUE(daemon.last_path.empty());  // never reached the transport
}

// --- check_docker_info -------------------------------------------------------

TEST(CheckDockerInfo, HealthyDaemonIsOkWithCounts) {
  fake_daemon daemon;
  daemon.payload = R"json({"ServerVersion": "26.1.4", "Name": "docker-host", "OperatingSystem": "Ubuntu 24.04",
                       "Containers": 5, "ContainersRunning": 3, "ContainersPaused": 0, "ContainersStopped": 2, "Images": 42})json";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_info(daemon.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("docker 26.1.4 on docker-host: 3 running, 0 paused, 2 stopped containers, 42 images"), std::string::npos) << msg;
  EXPECT_EQ(daemon.last_path, "/info");
}

TEST(CheckDockerInfo, ThresholdsApplyToCounts) {
  fake_daemon daemon;
  daemon.payload = R"json({"ServerVersion": "26.1.4", "Name": "docker-host",
                       "Containers": 5, "ContainersRunning": 0, "ContainersStopped": 5, "Images": 42})json";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_info(daemon.factory(), {"critical=running < 1"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckDockerInfo, DaemonFailureIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_info(refusing_daemon("no such file or directory"), {}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Failed to connect to docker daemon"), std::string::npos) << join_lines(response);
}

// --- check_docker_stats --------------------------------------------------------

#include "check_docker_stats.hpp"
#include "check_docker_restarts.hpp"
#include "check_docker_df.hpp"

namespace {

// A daemon routing by path: the container list plus per-container payloads.
struct routed_daemon {
  std::map<std::string, std::string> routes;
  std::vector<std::string> requested;

  docker_checks::fetcher_factory factory() {
    return [this](const std::string &, int) -> docker_checks::fetcher {
      return [this](const std::string &path) -> std::string {
        requested.push_back(path);
        const auto it = routes.find(path);
        if (it == routes.end()) throw std::runtime_error("unexpected path: " + path);
        return it->second;
      };
    };
  }
};

PB::Common::ResultCode run_stats(const docker_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_docker_stats");
  for (const std::string &a : args) request.add_arguments(a);
  docker_checks::check_stats(unix_defaults(), request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_restarts(const docker_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                    PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_docker_restarts");
  for (const std::string &a : args) request.add_arguments(a);
  docker_checks::check_restarts(unix_defaults(), request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_df(const docker_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                              PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_docker_df");
  for (const std::string &a : args) request.add_arguments(a);
  docker_checks::check_df(unix_defaults(), request, &response, factory);
  return response.result();
}

// cpu: delta 200 of 2000 system ticks on 4 cpus = 40%; memory: 300MiB usage
// minus 44MiB inactive cache = 256MiB of a 512MiB limit = 50%.
const char *WEB_STATS = R"json({
  "precpu_stats": {"cpu_usage": {"total_usage": 1000}, "system_cpu_usage": 10000, "online_cpus": 4},
  "cpu_stats": {"cpu_usage": {"total_usage": 1200}, "system_cpu_usage": 12000, "online_cpus": 4},
  "memory_stats": {"usage": 314572800, "limit": 536870912, "stats": {"inactive_file": 46137344}}
})json";

routed_daemon stats_daemon() {
  routed_daemon daemon;
  daemon.routes["/containers/json"] = R"json([{"Id": "aaa111", "Names": ["/web"], "Image": "nginx:1.25", "State": "running"}])json";
  daemon.routes["/containers/aaa111/stats?stream=false"] = WEB_STATS;
  return daemon;
}

}  // namespace

TEST(CheckDockerStats, ComputesCpuAndMemoryLikeDockerStats) {
  routed_daemon daemon = stats_daemon();
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_stats(daemon.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("web: cpu 40%"), std::string::npos) << join_lines(response);
  EXPECT_NE(join_lines(response).find("(50%)"), std::string::npos) << join_lines(response);
}

TEST(CheckDockerStats, ThresholdsAcceptSizeUnits) {
  routed_daemon daemon = stats_daemon();
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_stats(daemon.factory(), {"critical=memory_used > 100M"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_EQ(run_stats(daemon.factory(), {"warning=cpu_pct > 30"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckDockerStats, ContainerSelectorLimitsSampling) {
  routed_daemon daemon = stats_daemon();
  daemon.routes["/containers/json"] =
      R"json([{"Id": "aaa111", "Names": ["/web"], "Image": "nginx:1.25", "State": "running"},
              {"Id": "bbb222", "Names": ["/db"], "Image": "postgres:16", "State": "running"}])json";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_stats(daemon.factory(), {"container=web"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  // The unselected container's (unrouted) stats endpoint must never be hit.
  for (const std::string &path : daemon.requested) {
    EXPECT_EQ(path.find("bbb222"), std::string::npos) << path;
  }
}

// --- check_docker_restarts -----------------------------------------------------

namespace {
routed_daemon restarts_daemon(const long long restart_count, const std::string &started_at, const bool oom_killed) {
  routed_daemon daemon;
  daemon.routes["/containers/json?all=true"] = R"json([{"Id": "aaa111", "Names": ["/web"], "Image": "nginx:1.25", "State": "restarting"}])json";
  daemon.routes["/containers/aaa111/json"] = std::string(R"json({"Id": "aaa111", "RestartCount": )json") + std::to_string(restart_count) +
                                             R"json(, "State": {"Status": "restarting", "ExitCode": 137, "OOMKilled": )json" +
                                             (oom_killed ? "true" : "false") + R"json(, "StartedAt": ")json" + started_at + R"json("}})json";
  return daemon;
}
}  // namespace

TEST(CheckDockerRestarts, RecentRestartLoopTripsDefaultWarning) {
  // Started "now" (well within 15m) with 10 restarts -> default warning.
  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  routed_daemon daemon = restarts_daemon(10, boost::posix_time::to_iso_extended_string(now) + "Z", false);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_restarts(daemon.factory(), {}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("web: 10 restarts"), std::string::npos) << join_lines(response);
}

TEST(CheckDockerRestarts, OldRestartsDoNotTrip) {
  // Many historical restarts but started a day ago -> healthy.
  routed_daemon daemon = restarts_daemon(10, "2020-01-01T00:00:00.000000000Z", false);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_restarts(daemon.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckDockerRestarts, OomKillIsCriticalByDefault) {
  routed_daemon daemon = restarts_daemon(0, "2020-01-01T00:00:00.000000000Z", true);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_restarts(daemon.factory(), {}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckDockerRestarts, NeverStartedTimestampIsMinusOne) {
  // The daemon's zero-value timestamp must not look like "started long ago".
  routed_daemon daemon = restarts_daemon(0, "0001-01-01T00:00:00Z", false);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_restarts(daemon.factory(), {"filter=started = -1", "detail-syntax=%(names)=%(started)", "top-syntax=${list}"}, response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "web=-1");
}

// --- check_docker_df -----------------------------------------------------------

namespace {
// One used image (10MB unique + 2MB shared), one unused (5MB unique of which
// 2MB shared with the used one); a running and a stopped container; a
// referenced and an orphan volume; build cache half in use.
const char *DF_PAYLOAD = R"json({
  "LayersSize": 0,
  "Images": [
    {"Containers": 1, "Size": 12000000, "SharedSize": 2000000},
    {"Containers": 0, "Size": 7000000, "SharedSize": 2000000}
  ],
  "Containers": [
    {"State": "running", "SizeRw": 1000000},
    {"State": "exited", "SizeRw": 3000000}
  ],
  "Volumes": [
    {"UsageData": {"RefCount": 1, "Size": 4000000}},
    {"UsageData": {"RefCount": 0, "Size": 6000000}}
  ],
  "BuildCache": [
    {"Size": 2000000, "InUse": true, "Shared": false},
    {"Size": 8000000, "InUse": false, "Shared": false}
  ]
})json";
}  // namespace

TEST(CheckDockerDf, AggregatesSizesAndReclaimable) {
  routed_daemon daemon;
  daemon.routes["/system/df"] = DF_PAYLOAD;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_df(daemon.factory(),
                   {"detail-syntax=%(unused_images)|%(images_size)|%(images_reclaimable)|%(containers_reclaimable)|%(volumes_reclaimable)|%(build_cache_"
                    "reclaimable)|%(total_reclaimable)",
                    "top-syntax=${list}"},
                   response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  // images_size = unique sizes: 10M + 5M; reclaimable: 5M (unused image) +
  // 3M (stopped container) + 6M (orphan volume) + 8M (idle build cache) = 22M.
  EXPECT_EQ(join_lines(response), "1|15000000|5000000|3000000|6000000|8000000|22000000");
}

TEST(CheckDockerDf, ThresholdsAcceptSizeUnits) {
  routed_daemon daemon;
  daemon.routes["/system/df"] = DF_PAYLOAD;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_df(daemon.factory(), {"warning=total_reclaimable > 20M"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_EQ(run_df(daemon.factory(), {"warning=total_reclaimable > 25M"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckDockerDf, DaemonFailureIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_df(refusing_daemon("boom"), {}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Failed to connect to docker daemon"), std::string::npos) << join_lines(response);
}
