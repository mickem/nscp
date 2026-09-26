// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "docker_facts.hpp"

#include <gtest/gtest.h>

#include <map>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "docker_client.hpp"

// The plugin singleton is defined in check_docker_test.cpp, which is compiled
// into the same test binary.

namespace {

using nscapi::facts::testing::find_set;
using nscapi::facts::testing::json_of;

std::string docker_json(const nscapi::facts::response &out) { return json_of(out, "docker"); }

// A daemon serving one canned payload per path; records what was asked for.
struct fake_daemon {
  std::map<std::string, std::string> payloads;
  std::vector<std::string> paths;

  docker_checks::fetcher fetcher() {
    return [this](const std::string &path) -> std::string {
      paths.push_back(path);
      const std::map<std::string, std::string>::const_iterator it = payloads.find(path);
      if (it == payloads.end()) throw docker_checks::docker_http_error(404, "HTTP 404 Not Found");
      return it->second;
    };
  }
};

docker_facts::selection everything() {
  docker_facts::selection what;
  what.daemon = what.containers = what.images = true;
  return what;
}

// GET /info trimmed to what the producer reads plus the counts it must not.
const char *INFO = R"json({
  "ID": "ABCD:EFGH", "Containers": 3, "ContainersRunning": 1, "ContainersPaused": 0, "ContainersStopped": 2, "Images": 12,
  "Driver": "overlay2", "CgroupDriver": "systemd", "CgroupVersion": "2",
  "KernelVersion": "6.8.0-45-generic", "OperatingSystem": "Ubuntu 24.04.1 LTS", "OSType": "linux", "Architecture": "x86_64",
  "NCPU": 8, "MemTotal": 33547567104, "Name": "buildhost", "ServerVersion": "27.3.1",
  "Swarm": {"NodeID": "", "LocalNodeState": "inactive"}
})json";

// Two containers as the daemon lists them: a compose service with a port
// published on both address families, and a stopped one-shot job with no
// ports and no compose labels. Newest first, as the daemon orders them.
const char *CONTAINERS = R"json([
  {
    "Id": "bbb222", "Names": ["/job"], "Image": "alpine", "ImageID": "sha256:456", "Command": "sh -c exit", "Created": 1700000100,
    "State": "exited", "Status": "Exited (1) 2 hours ago", "Ports": [], "Labels": {},
    "NetworkSettings": {"Networks": {}}
  },
  {
    "Id": "aaa111", "Names": ["/web"], "Image": "nginx:1.25", "ImageID": "sha256:123", "Command": "nginx -g 'daemon off;'", "Created": 1700000000,
    "State": "running", "Status": "Up 3 hours (healthy)",
    "Ports": [
      {"IP": "0.0.0.0", "PrivatePort": 80, "PublicPort": 8080, "Type": "tcp"},
      {"IP": "::", "PrivatePort": 80, "PublicPort": 8080, "Type": "tcp"},
      {"PrivatePort": 443, "Type": "tcp"}
    ],
    "Labels": {"com.docker.compose.project": "shop", "com.docker.compose.service": "web", "env": "prod"},
    "NetworkSettings": {"Networks": {"frontend": {"IPAddress": "172.20.0.2"}}}
  }
])json";

// Three images: one with two tags, one dangling (RepoTags null, as current
// daemons report it), one dangling the old way.
const char *IMAGES = R"json([
  {"Id": "sha256:aaa", "RepoTags": ["nginx:latest", "nginx:1.25"], "RepoDigests": ["nginx@sha256:0"], "Created": 1699000000, "Size": 187000000, "Labels": null},
  {"Id": "sha256:bbb", "RepoTags": null, "Created": 1698000000, "Size": 5000000},
  {"Id": "sha256:ccc", "RepoTags": ["<none>:<none>"], "Created": 1697000000, "Size": 0}
])json";

}  // namespace

TEST(DockerFacts, DaemonRecordCarriesTheDaemonNotItsCounts) {
  docker_facts::selection what;
  what.daemon = true;
  docker_facts::snapshot snap;
  snap.daemon_info = docker_facts::parse_daemon(INFO);
  nscapi::facts::response out;
  docker_facts::publish(what, snap, 0, out);
  // No Containers/Images counts: those change every round and belong to
  // check_docker_info. The architecture is the `os` set's spelling.
  EXPECT_EQ(docker_json(out),
            "{\"version\":\"27.3.1\",\"os\":\"Ubuntu 24.04.1 LTS\",\"os_type\":\"linux\",\"architecture\":\"x86_64\",\"kernel_version\":\"6.8.0-45-generic\","
            "\"storage_driver\":\"overlay2\",\"cgroup_driver\":\"systemd\",\"cgroup_version\":\"2\",\"swarm\":\"inactive\",\"cpus\":8,"
            "\"memory_bytes\":33547567104}");
}

TEST(DockerFacts, ArchitectureIsNormalisedToTheOsVocabulary) {
  const docker_facts::daemon d = docker_facts::parse_daemon(R"({"ServerVersion":"27.3.1","Architecture":"aarch64","OSType":"linux"})");
  EXPECT_EQ(d.architecture, "arm64");
}

TEST(DockerFacts, ContainersAreRecordsByNameWithoutState) {
  const std::vector<docker_facts::container> containers = docker_facts::parse_containers(CONTAINERS);
  ASSERT_EQ(containers.size(), 2u);
  // Sorted by id, not in the daemon's newest-first order.
  EXPECT_EQ(containers[0].id, "job");
  EXPECT_EQ(containers[1].id, "web");
  // Spelled as check_docker spells them and sorted: the port published on
  // both address families is one entry per family, since which addresses a
  // port is bound on is part of the record.
  EXPECT_EQ(containers[1].ports, (std::vector<std::string>{"0.0.0.0:8080->80/tcp", "443/tcp", ":::8080->80/tcp"}));
  EXPECT_EQ(containers[1].compose_project, "shop");
  EXPECT_EQ(containers[1].compose_service, "web");

  docker_facts::selection what;
  what.containers = true;
  docker_facts::snapshot snap;
  snap.containers = containers;
  nscapi::facts::response out;
  docker_facts::publish(what, snap, 0, out);
  // No state, no status, no health, no IP: inventory, not monitoring. The
  // job's empty ports list and missing compose labels are omitted, never
  // written empty. (The builder writes a record's values before its lists,
  // whatever order they were added in.)
  EXPECT_EQ(
      docker_json(out),
      "{\"containers\":[{\"id\":\"job\",\"container_id\":\"bbb222\",\"image\":\"alpine\",\"image_id\":\"sha256:456\",\"created\":\"2023-11-14T22:15:00Z\"},"
      "{\"id\":\"web\",\"container_id\":\"aaa111\",\"image\":\"nginx:1.25\",\"image_id\":\"sha256:123\",\"created\":\"2023-11-14T22:13:20Z\","
      "\"compose_project\":\"shop\",\"compose_service\":\"web\",\"ports\":[\"0.0.0.0:8080->80/tcp\",\"443/tcp\",\":::8080->80/tcp\"]}]}");
}

TEST(DockerFacts, ContainerIdIsTheNamesKeywordOfCheckDocker) {
  // A container with several names (legacy links) is one record, named the
  // way check_docker's `names` keyword spells it.
  const std::vector<docker_facts::container> containers =
      docker_facts::parse_containers(R"([{"Id":"c1","Names":["/db","/web/db"],"Image":"postgres:16","Created":1}])");
  ASSERT_EQ(containers.size(), 1u);
  EXPECT_EQ(containers[0].id, "db,web/db");
}

TEST(DockerFacts, ImagesAreRecordsByImageIdWithTheirTags) {
  const std::vector<docker_facts::image> images = docker_facts::parse_images(IMAGES);
  ASSERT_EQ(images.size(), 3u);
  // Keyed on the image id, which does not move when a tag does, and sorted
  // by it; the tags ride along, sorted, for reading.
  EXPECT_EQ(images[0].id, "sha256:aaa");
  EXPECT_EQ(images[0].tags, (std::vector<std::string>{"nginx:1.25", "nginx:latest"}));
  EXPECT_EQ(images[1].id, "sha256:bbb");
  EXPECT_TRUE(images[1].tags.empty());
  EXPECT_EQ(images[2].id, "sha256:ccc");
  EXPECT_TRUE(images[2].tags.empty()) << "<none>:<none> is not a tag";

  docker_facts::selection what;
  what.images = true;
  docker_facts::snapshot snap;
  snap.images = images;
  nscapi::facts::response out;
  docker_facts::publish(what, snap, 0, out);
  EXPECT_EQ(docker_json(out),
            "{\"images\":[{\"id\":\"sha256:aaa\",\"image_id\":\"sha256:aaa\",\"created\":\"2023-11-03T08:26:40Z\",\"size_bytes\":187000000,"
            "\"tags\":[\"nginx:1.25\",\"nginx:latest\"]},"
            "{\"id\":\"sha256:bbb\",\"image_id\":\"sha256:bbb\",\"created\":\"2023-10-22T18:40:00Z\",\"size_bytes\":5000000},"
            "{\"id\":\"sha256:ccc\",\"image_id\":\"sha256:ccc\",\"created\":\"2023-10-11T04:53:20Z\"}]}");
}

TEST(DockerFacts, OnlyTheSelectedPartsAreFetchedAndPublished) {
  fake_daemon daemon;
  daemon.payloads["/info"] = INFO;
  daemon.payloads["/containers/json?all=true"] = CONTAINERS;
  daemon.payloads["/images/json"] = IMAGES;
  docker_facts::selection what;
  what.containers = true;
  const docker_facts::snapshot snap = docker_facts::gather(what, daemon.fetcher(), "/var/run/docker.sock");
  // Stopped containers included: an inventory lists what exists, not what
  // runs. Nothing else was asked of the daemon.
  EXPECT_EQ(daemon.paths, (std::vector<std::string>{"/containers/json?all=true"}));
  EXPECT_EQ(snap.containers.size(), 2u);
  EXPECT_TRUE(snap.daemon_info.version.empty());
  EXPECT_TRUE(snap.images.empty());

  nscapi::facts::response out;
  docker_facts::publish(what, snap, 0, out);
  EXPECT_EQ(docker_json(out).substr(0, 16), "{\"containers\":[{");
  EXPECT_EQ(docker_json(out).find("\"version\""), std::string::npos);
  EXPECT_EQ(docker_json(out).find("\"images\""), std::string::npos);
}

TEST(DockerFacts, NoContainersIsAnEmptyListNotAMissingSet) {
  fake_daemon daemon;
  daemon.payloads["/containers/json?all=true"] = "[]";
  daemon.payloads["/images/json"] = "[]";
  docker_facts::selection what;
  what.containers = what.images = true;
  nscapi::facts::response out;
  docker_facts::publish(what, docker_facts::gather(what, daemon.fetcher(), "/var/run/docker.sock"), 0, out);
  EXPECT_EQ(docker_json(out), "{\"containers\":[],\"images\":[]}");
}

TEST(DockerFacts, NothingSelectedPublishesNothing) {
  nscapi::facts::response out;
  docker_facts::publish(docker_facts::selection(), docker_facts::snapshot(), 0, out);
  EXPECT_EQ(docker_json(out), "(no such set)");
}

TEST(DockerFacts, StampsWhenTheValuesWereRead) {
  docker_facts::selection what;
  what.daemon = true;
  nscapi::facts::response out;
  docker_facts::publish(what, docker_facts::snapshot(), 1790000000, out);
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, "docker");
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(set->gathered(), nscapi::facts::format_time(1790000000));
}

TEST(DockerFacts, AnUnreachableDaemonThrowsRatherThanPublishingAPartialSnapshot) {
  // The daemon answers /info and then goes away: nothing is returned, so
  // the module reports an error and the core keeps the last good set. A
  // snapshot with the daemon and no containers would have replaced it.
  fake_daemon daemon;
  daemon.payloads["/info"] = INFO;
  try {
    docker_facts::gather(everything(), daemon.fetcher(), "/var/run/docker.sock");
    FAIL() << "a refused request must throw";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("returned HTTP 404 for /containers/json?all=true"), std::string::npos) << e.what();
  }

  const docker_checks::fetcher refusing = [](const std::string &) -> std::string { throw std::runtime_error("connection refused"); };
  try {
    docker_facts::gather(everything(), refusing, "/var/run/docker.sock");
    FAIL() << "a transport failure must throw";
  } catch (const std::runtime_error &e) {
    EXPECT_EQ(std::string(e.what()), "Failed to connect to docker daemon at '/var/run/docker.sock': connection refused");
  }
}

TEST(DockerFacts, ABadPayloadNamesThePath) {
  EXPECT_THROW(docker_facts::parse_daemon("not json"), std::runtime_error);
  EXPECT_THROW(docker_facts::parse_daemon("[]"), std::runtime_error);
  EXPECT_THROW(docker_facts::parse_containers("{}"), std::runtime_error);
  EXPECT_THROW(docker_facts::parse_images("{}"), std::runtime_error);
  try {
    docker_facts::parse_containers("{}");
  } catch (const std::runtime_error &e) {
    EXPECT_EQ(std::string(e.what()), "Failed to parse docker daemon response from /containers/json?all=true: expected a list of containers");
  }
}

TEST(DockerFacts, ToleratesWhatTheDaemonLeavesOut) {
  // podman's compat API and older daemons omit fields; a missing one is an
  // omitted value, never a throw and never an empty string in the record.
  const docker_facts::daemon d = docker_facts::parse_daemon(R"({"ServerVersion":"4.9.3"})");
  EXPECT_EQ(d.version, "4.9.3");
  EXPECT_TRUE(d.swarm.empty());
  EXPECT_EQ(d.cpus, 0);
  docker_facts::selection what;
  what.daemon = true;
  docker_facts::snapshot snap;
  snap.daemon_info = d;
  nscapi::facts::response out;
  docker_facts::publish(what, snap, 0, out);
  EXPECT_EQ(docker_json(out), "{\"version\":\"4.9.3\"}");
}
