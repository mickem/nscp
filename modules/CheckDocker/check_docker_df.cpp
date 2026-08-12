// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_docker_df.hpp"

#include "docker_client.hpp"
#include "docker_endpoint.hpp"

#include <boost/json.hpp>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>

namespace json = boost::json;
namespace po = boost::program_options;

namespace docker_checks {

namespace {

struct df_obj {
  long long images = 0, unused_images = 0;
  long long images_size = 0, images_reclaimable = 0;
  long long containers = 0;
  long long containers_size = 0, containers_reclaimable = 0;
  long long volumes = 0, unused_volumes = 0;
  long long volumes_size = 0, volumes_reclaimable = 0;
  long long build_cache_size = 0, build_cache_reclaimable = 0;

  long long get_images() const { return images; }
  long long get_unused_images() const { return unused_images; }
  long long get_images_size() const { return images_size; }
  long long get_images_reclaimable() const { return images_reclaimable; }
  long long get_containers() const { return containers; }
  long long get_containers_size() const { return containers_size; }
  long long get_containers_reclaimable() const { return containers_reclaimable; }
  long long get_volumes() const { return volumes; }
  long long get_unused_volumes() const { return unused_volumes; }
  long long get_volumes_size() const { return volumes_size; }
  long long get_volumes_reclaimable() const { return volumes_reclaimable; }
  long long get_build_cache_size() const { return build_cache_size; }
  long long get_build_cache_reclaimable() const { return build_cache_reclaimable; }
  long long get_total_size() const { return images_size + containers_size + volumes_size + build_cache_size; }
  long long get_total_reclaimable() const { return images_reclaimable + containers_reclaimable + volumes_reclaimable + build_cache_reclaimable; }

  std::string get_message() const {
    return "images " + std::to_string(images) + " (" + str::format::format_byte_units(images_size) + "), containers " + std::to_string(containers) + " (" +
           str::format::format_byte_units(containers_size) + "), volumes " + std::to_string(volumes) + " (" + str::format::format_byte_units(volumes_size) +
           "), build cache " + str::format::format_byte_units(build_cache_size) + ", reclaimable " + str::format::format_byte_units(get_total_reclaimable());
  }
  std::string show() const { return get_message(); }
};

// Aggregate /system/df the way `docker system df` does: reclaimable is what a
// prune would free - unused images (their non-shared size), stopped
// containers, unreferenced volumes and idle build cache.
std::shared_ptr<df_obj> parse_df(const json::object &o) {
  auto record = std::make_shared<df_obj>();

  if (const json::value *images = o.if_contains("Images")) {
    if (images->is_array()) {
      for (const auto &v : images->as_array()) {
        if (!v.is_object()) continue;
        const json::object &img = v.as_object();
        record->images++;
        const long long size = get_num(img, "Size");
        const long long shared = get_num(img, "SharedSize");
        record->images_size += size - shared;  // sum unique sizes; shared layers would otherwise count once per image
        if (get_num(img, "Containers") == 0) {
          record->unused_images++;
          if (size > shared) record->images_reclaimable += size - shared;
        }
      }
    }
  }

  if (const json::value *containers = o.if_contains("Containers")) {
    if (containers->is_array()) {
      for (const auto &v : containers->as_array()) {
        if (!v.is_object()) continue;
        const json::object &c = v.as_object();
        record->containers++;
        const long long size = get_num(c, "SizeRw");
        record->containers_size += size;
        if (get_str(c, "State") != "running") record->containers_reclaimable += size;
      }
    }
  }

  if (const json::value *volumes = o.if_contains("Volumes")) {
    if (volumes->is_array()) {
      for (const auto &v : volumes->as_array()) {
        if (!v.is_object()) continue;
        record->volumes++;
        if (const json::object *usage = get_obj(v.as_object(), "UsageData")) {
          const long long size = get_num(*usage, "Size");  // -1 = unknown
          if (size > 0) record->volumes_size += size;
          if (get_num(*usage, "RefCount") == 0) {
            record->unused_volumes++;
            if (size > 0) record->volumes_reclaimable += size;
          }
        }
      }
    }
  }

  if (const json::value *cache = o.if_contains("BuildCache")) {
    if (cache->is_array()) {
      for (const auto &v : cache->as_array()) {
        if (!v.is_object()) continue;
        const json::object &e = v.as_object();
        const long long size = get_num(e, "Size");
        // Shared entries alias image layers already counted above.
        if (get_bool(e, "Shared")) continue;
        record->build_cache_size += size;
        if (!get_bool(e, "InUse")) record->build_cache_reclaimable += size;
      }
    }
  }

  return record;
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<df_obj>> df_context;
struct df_obj_handler : public df_context {
  df_obj_handler() {
    using parsers::where::type_size;
    registry_.add_string_var("message", &df_obj::get_message, "Human readable disk-usage summary");
    registry_.add_int_var("images", &df_obj::get_images, "Number of images")
        .add_int_perf("", "", " images")
        .add_int_var("unused_images", &df_obj::get_unused_images, "Number of images not used by any container")
        .add_int_perf("", "", " unused images")
        .add_int_var("containers", &df_obj::get_containers, "Number of containers (running and stopped)")
        .add_int_var("volumes", &df_obj::get_volumes, "Number of volumes")
        .add_int_var("unused_volumes", &df_obj::get_unused_volumes, "Number of volumes not referenced by any container");
    registry_.add_int_var("images_size", type_size, &df_obj::get_images_size, "Disk used by images (bytes; supports units, e.g. images_size > 10G)")
        .add_int_perf("B", "", " images size")
        .add_int_var("images_reclaimable", type_size, &df_obj::get_images_reclaimable, "Disk freed by pruning unused images (bytes)")
        .add_int_var("containers_size", type_size, &df_obj::get_containers_size, "Disk used by container writable layers (bytes)")
        .add_int_perf("B", "", " containers size")
        .add_int_var("containers_reclaimable", type_size, &df_obj::get_containers_reclaimable, "Disk freed by pruning stopped containers (bytes)")
        .add_int_var("volumes_size", type_size, &df_obj::get_volumes_size, "Disk used by volumes (bytes)")
        .add_int_perf("B", "", " volumes size")
        .add_int_var("volumes_reclaimable", type_size, &df_obj::get_volumes_reclaimable, "Disk freed by pruning unused volumes (bytes)")
        .add_int_var("build_cache_size", type_size, &df_obj::get_build_cache_size, "Disk used by the build cache (bytes)")
        .add_int_perf("B", "", " build cache")
        .add_int_var("build_cache_reclaimable", type_size, &df_obj::get_build_cache_reclaimable, "Disk freed by pruning the idle build cache (bytes)")
        .add_int_var("total_size", type_size, &df_obj::get_total_size, "Total disk used by docker (bytes)")
        .add_int_perf("B", "", " total")
        .add_int_var("total_reclaimable", type_size, &df_obj::get_total_reclaimable, "Total disk a full prune would free (bytes)")
        .add_int_perf("B", "", " reclaimable");
  }
};
typedef modern_filter::modern_filters<df_obj, df_obj_handler> df_filter;

}  // namespace

void check_df(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
              const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<df_filter> filter_helper(request, response, data);
  std::string endpoint = defaults.endpoint.empty() ? default_docker_endpoint() : defaults.endpoint;
  // /system/df walks every image layer, container filesystem and volume, so
  // it can take far longer than the chatty endpoints; give it more rope.
  int timeout = defaults.timeout * 6;

  df_filter filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${message}", "docker", "%(status): No disk usage information returned", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&endpoint)->default_value(endpoint), "The local docker daemon socket (named pipe on Windows, unix socket elsewhere).")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // See check_containers for why the endpoint must be constrained.
  std::string endpoint_error;
  if (!is_local_docker_endpoint(endpoint, endpoint_error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, endpoint_error);
  }

  if (!filter_helper.build_filter(filter)) return;

  json::value root;
  if (!fetch_json(make_fetcher(endpoint, timeout), endpoint, std::string(API) + "/system/df", root, response)) return;
  if (!root.is_object()) {
    return fail(response, "Failed to parse docker daemon response from /system/df: expected an object");
  }

  filter.match(parse_df(root.as_object()));
  filter_helper.post_process(filter);
}

}  // namespace docker_checks
