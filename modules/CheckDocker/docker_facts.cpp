// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "docker_facts.hpp"

#include <algorithm>
#include <boost/json.hpp>
#include <facts/host_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>
#include <stdexcept>
#include <str/utf8.hpp>

#include "docker_client.hpp"

namespace json = boost::json;

namespace docker_facts {

const char *const set_docker = "docker";
const char *const id_daemon = "docker";
const char *const id_containers = "docker.containers";
const char *const id_images = "docker.images";
const char *const key_containers = "containers";
const char *const key_images = "images";

namespace {

const char *const PATH_INFO = "/info";
const char *const PATH_CONTAINERS = "/containers/json?all=true";
const char *const PATH_IMAGES = "/images/json";

// The labels compose stamps on every container it starts. Read by key and
// dropped otherwise: a label map does not fit the document rules (its keys
// are dotted), and most labels are configuration rather than inventory.
const char *const LABEL_COMPOSE_PROJECT = "com.docker.compose.project";
const char *const LABEL_COMPOSE_SERVICE = "com.docker.compose.service";

json::value parse_body(const std::string &body, const char *path) {
  try {
    return json::parse(body);
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Failed to parse docker daemon response from ") + path + ": " + utf8::utf8_from_native(e.what()));
  }
}

std::string label_of(const json::object &o, const char *key) {
  const json::object *labels = docker_checks::get_obj(o, "Labels");
  if (labels == nullptr) return "";
  return docker_checks::get_str(*labels, key);
}

std::string fetch_body(const docker_checks::fetcher &fetch, const std::string &endpoint, const char *path) {
  try {
    return fetch(path);
  } catch (const docker_checks::docker_http_error &e) {
    // The daemon answered and refused: it is there, it would not serve the
    // request. Worth telling apart from an unreachable socket.
    throw std::runtime_error("docker daemon at '" + endpoint + "' returned HTTP " + std::to_string(e.status()) + " for " + path + ": " + e.what());
  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to connect to docker daemon at '" + endpoint + "': " + utf8::utf8_from_native(e.what()));
  }
}

}  // namespace

daemon parse_daemon(const std::string &body) {
  const json::value root = parse_body(body, PATH_INFO);
  if (!root.is_object()) throw std::runtime_error(std::string("Failed to parse docker daemon response from ") + PATH_INFO + ": expected an object");
  const json::object &o = root.as_object();
  daemon d;
  d.version = docker_checks::get_str(o, "ServerVersion");
  d.os = docker_checks::get_str(o, "OperatingSystem");
  d.os_type = docker_checks::get_str(o, "OSType");
  // The daemon spells it as the kernel does (`x86_64`, `aarch64`); the `os`
  // set has one spelling per architecture and this has to match it, or one
  // host's docker and its OS disagree about what CPU they run on.
  d.architecture = host_facts::normalize_arch(docker_checks::get_str(o, "Architecture"));
  d.kernel_version = docker_checks::get_str(o, "KernelVersion");
  d.storage_driver = docker_checks::get_str(o, "Driver");
  d.cgroup_driver = docker_checks::get_str(o, "CgroupDriver");
  d.cgroup_version = docker_checks::get_str(o, "CgroupVersion");
  if (const json::object *swarm = docker_checks::get_obj(o, "Swarm")) d.swarm = docker_checks::get_str(*swarm, "LocalNodeState");
  d.cpus = docker_checks::get_num(o, "NCPU");
  const long long memory = docker_checks::get_num(o, "MemTotal");
  if (memory > 0) d.memory_bytes = static_cast<unsigned long long>(memory);
  return d;
}

std::vector<container> parse_containers(const std::string &body) {
  const json::value root = parse_body(body, PATH_CONTAINERS);
  if (!root.is_array()) {
    throw std::runtime_error(std::string("Failed to parse docker daemon response from ") + PATH_CONTAINERS + ": expected a list of containers");
  }
  std::vector<container> containers;
  for (const json::value &v : root.as_array()) {
    if (!v.is_object()) continue;
    const json::object &o = v.as_object();
    container c;
    c.container_id = docker_checks::get_str(o, "Id");
    c.image = docker_checks::get_str(o, "Image");
    c.image_id = docker_checks::get_str(o, "ImageID");
    const long long created = docker_checks::get_num(o, "Created");
    if (created > 0) c.created = static_cast<std::time_t>(created);
    // The same helper check_docker reads its `names` keyword from, so the id
    // is the check's value byte for byte.
    c.id = docker_checks::container_names(o);
    // A container the daemon lists without a name cannot be a record (a
    // record without an id is rejected); the daemon always names one, so
    // falling back to the Id is belt and braces rather than a real case.
    if (c.id.empty()) c.id = c.container_id;
    // Spelled as check_docker spells its `ports` keyword, and kept as the
    // daemon lists them: a port published on both address families is one
    // entry per family (0.0.0.0:8080->80/tcp and :::8080->80/tcp), because
    // which addresses a port is bound on is part of what the record says.
    // Sorted, with exact duplicates dropped, so the record does not change
    // when only the daemon's listing order did.
    c.ports = docker_checks::container_ports(o);
    std::sort(c.ports.begin(), c.ports.end());
    c.ports.erase(std::unique(c.ports.begin(), c.ports.end()), c.ports.end());
    c.compose_project = label_of(o, LABEL_COMPOSE_PROJECT);
    c.compose_service = label_of(o, LABEL_COMPOSE_SERVICE);
    containers.push_back(c);
  }
  // The daemon lists newest first, so every new container would reorder the
  // list; by id, an unchanged set of containers is an unchanged document.
  std::sort(containers.begin(), containers.end(), [](const container &a, const container &b) { return a.id < b.id; });
  return containers;
}

std::vector<image> parse_images(const std::string &body) {
  const json::value root = parse_body(body, PATH_IMAGES);
  if (!root.is_array()) throw std::runtime_error(std::string("Failed to parse docker daemon response from ") + PATH_IMAGES + ": expected a list of images");
  std::vector<image> images;
  for (const json::value &v : root.as_array()) {
    if (!v.is_object()) continue;
    const json::object &o = v.as_object();
    image i;
    i.image_id = docker_checks::get_str(o, "Id");
    if (i.image_id.empty()) continue;  // not an image the daemon can name; nothing to record it by
    const long long created = docker_checks::get_num(o, "Created");
    if (created > 0) i.created = static_cast<std::time_t>(created);
    const long long size = docker_checks::get_num(o, "Size");
    if (size > 0) i.size_bytes = static_cast<unsigned long long>(size);
    // RepoTags is null (older daemons: ["<none>:<none>"]) for a dangling
    // image; neither spelling is a tag.
    if (const json::value *tags = o.if_contains("RepoTags")) {
      if (tags->is_array()) {
        for (const json::value &tag : tags->as_array()) {
          if (!tag.is_string()) continue;
          const std::string text = tag.as_string().c_str();
          if (!text.empty() && text != "<none>:<none>") i.tags.push_back(text);
        }
      }
    }
    std::sort(i.tags.begin(), i.tags.end());
    i.tags.erase(std::unique(i.tags.begin(), i.tags.end()), i.tags.end());
    // The image id is the identity: a tag moves (pulling a new `latest`
    // retags the old image, `docker tag` adds one), and a record keyed on it
    // would read as a removal and an addition to a consumer diffing by id.
    // The tags are what an operator reads the list by, and they are carried.
    i.id = i.image_id;
    images.push_back(i);
  }
  std::sort(images.begin(), images.end(), [](const image &a, const image &b) { return a.id < b.id; });
  return images;
}

snapshot gather(const selection &what, const docker_checks::fetcher &fetch, const std::string &endpoint) {
  snapshot snap;
  if (what.daemon) snap.daemon_info = parse_daemon(fetch_body(fetch, endpoint, PATH_INFO));
  if (what.containers) snap.containers = parse_containers(fetch_body(fetch, endpoint, PATH_CONTAINERS));
  if (what.images) snap.images = parse_images(fetch_body(fetch, endpoint, PATH_IMAGES));
  return snap;
}

void publish(const selection &what, const snapshot &snap, const std::time_t taken_at, nscapi::facts::response &out) {
  if (!what.any()) return;
  nscapi::facts::section docker = out.set(set_docker);
  if (what.daemon) {
    const daemon &d = snap.daemon_info;
    docker.value("version", d.version)
        .value("os", d.os)
        .value("os_type", d.os_type)
        .value("architecture", d.architecture)
        .value("kernel_version", d.kernel_version)
        .value("storage_driver", d.storage_driver)
        .value("cgroup_driver", d.cgroup_driver)
        .value("cgroup_version", d.cgroup_version)
        .value("swarm", d.swarm);
    // Zero is "not reported", and the builder writes a number as it is
    // given, so the guard is here.
    if (d.cpus > 0) docker.value("cpus", d.cpus);
    if (d.memory_bytes > 0) docker.value("memory_bytes", d.memory_bytes);
  }
  if (what.containers) {
    // Written even when empty: a daemon with no containers has told us
    // something, and an absent list would read as "not collected".
    nscapi::facts::record_list list = docker.list(key_containers);
    for (const container &c : snap.containers) {
      nscapi::facts::section record = list.record(c.id);
      record.value("container_id", c.container_id).value("image", c.image).value("image_id", c.image_id);
      record.time("created", c.created);
      if (!c.ports.empty()) record.strings("ports", c.ports);
      record.value("compose_project", c.compose_project).value("compose_service", c.compose_service);
    }
  }
  if (what.images) {
    nscapi::facts::record_list list = docker.list(key_images);
    for (const image &i : snap.images) {
      nscapi::facts::section record = list.record(i.id);
      record.value("image_id", i.image_id);
      if (!i.tags.empty()) record.strings("tags", i.tags);
      record.time("created", i.created);
      if (i.size_bytes > 0) record.value("size_bytes", i.size_bytes);
    }
  }
  out.gathered(set_docker, taken_at);
}

}  // namespace docker_facts
