// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "check_docker.hpp"

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `docker` fact set: which docker daemon this host runs, which containers
// it holds and which images it has pulled.
//
// The inventory, not the monitoring: a container is listed with the image it
// was created from, never with its state. State (running, exited, unhealthy)
// changes under a running agent, a value that changes every round moves the
// document's revision every round, and check_docker is where state lives. The
// same rule keeps the daemon's container and image *counts* out of the daemon
// record: check_docker_info reports those.
//
// Everything here is platform-neutral: the daemon speaks the same API over a
// named pipe and a unix socket, and the transport is the module's injected
// fetcher, so the parsing is unit-tested against canned payloads on every
// build exactly as the checks are.
namespace docker_facts {

// The fact set this module produces, and the three enableable ids in it. The
// ids are the settings keys under [/settings/docker/facts]: `docker` is the
// daemon record, `docker.containers` and `docker.images` the two lists.
extern const char *const set_docker;
extern const char *const id_daemon;
extern const char *const id_containers;
extern const char *const id_images;
extern const char *const key_containers;
extern const char *const key_images;

// Which of the three an operator turned on. gather() fetches only what is
// asked for, and publish() writes only that, so a set with just
// `docker.containers` enabled is one list and nothing else.
struct selection {
  bool daemon = false;
  bool containers = false;
  bool images = false;
  bool any() const { return daemon || containers || images; }
};

// The daemon, as GET /info describes it. Empty strings and zero counts mean
// "not known" and are omitted from the record, never written empty.
struct daemon {
  std::string version;                  // ServerVersion: `27.3.1`
  std::string os;                       // OperatingSystem: `Ubuntu 24.04.1 LTS`, `Docker Desktop`
  std::string os_type;                  // OSType: `linux` or `windows`, the `os` set's family vocabulary
  std::string architecture;             // Architecture, normalised to the `os` set's vocabulary (`x86_64`, `arm64`)
  std::string kernel_version;           // KernelVersion
  std::string storage_driver;           // Driver: `overlay2`, `windowsfilter`
  std::string cgroup_driver;            // CgroupDriver: `systemd`, `cgroupfs`
  std::string cgroup_version;           // CgroupVersion: `1` or `2`
  std::string swarm;                    // Swarm.LocalNodeState: `inactive`, `active`, `pending`, `error`, `locked`
  long long cpus = 0;                   // NCPU
  unsigned long long memory_bytes = 0;  // MemTotal
};

// One container, as GET /containers/json?all=true lists it.
struct container {
  // The record id: the container's names, comma separated - exactly the
  // `names` keyword of check_docker for the same container, which is how a
  // failing check finds its record. One name is the overwhelmingly common
  // case.
  std::string id;
  std::string container_id;  // the daemon's Id, the `id` keyword of check_docker
  std::string image;         // `nginx:1.25`
  std::string image_id;      // `sha256:…`
  std::time_t created = 0;
  // Published and exposed ports, spelled as check_docker spells them
  // (`0.0.0.0:8080->80/tcp`, `80/tcp`), sorted; a port published on both
  // address families is one entry per family, as the daemon lists it.
  std::vector<std::string> ports;
  // The compose project and service the container belongs to, from the
  // labels compose stamps on it. Labels in general are not carried: their
  // keys (`com.docker.compose.project`) are not fact keys, and most of them
  // are configuration. These two are the ones that say what a container *is*.
  std::string compose_project;
  std::string compose_service;
};

// One image, as GET /images/json lists it.
struct image {
  // The record id: the daemon's Id (`sha256:…`), which is the one name of
  // an image that does not move when it is retagged. The tags are carried
  // beside it, and are what an operator reads the list by.
  std::string id;
  std::string image_id;           // the same Id, as a field
  std::vector<std::string> tags;  // every tag, sorted; `<none>:<none>` is not a tag
  std::time_t created = 0;
  unsigned long long size_bytes = 0;
};

// Everything one round collected. Gathered whole before anything is
// published: a round that read the daemon but could not list the containers
// must not replace a set that has both with one that has only the daemon.
struct snapshot {
  daemon daemon_info;
  std::vector<container> containers;
  std::vector<image> images;
};

// Parse one API payload each. Tolerant of fields the daemon (or podman's
// compat API) leaves out, exactly like the checks; a payload that is not JSON
// of the expected shape throws std::runtime_error naming the path. Pure, so
// they are what the unit tests drive.
daemon parse_daemon(const std::string &body);
std::vector<container> parse_containers(const std::string &body);
std::vector<image> parse_images(const std::string &body);

// Fetch what `what` asks for from the daemon behind `fetch`. Throws
// std::runtime_error - "Failed to connect to docker daemon at '<endpoint>':
// …" for a transport failure, the HTTP status for a refused request, the
// parse error for a bad payload - and never returns a partial snapshot.
snapshot gather(const selection &what, const docker_checks::fetcher &fetch, const std::string &endpoint);

// Add the `docker` set, with the parts of `snap` that `what` selects, to
// `out`. `taken_at` stamps when the values were read.
void publish(const selection &what, const snapshot &snap, std::time_t taken_at, nscapi::facts::response &out);

}  // namespace docker_facts
