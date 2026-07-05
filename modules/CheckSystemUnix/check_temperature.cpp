// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_temperature.h"

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <locale>
#include <map>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <set>
#include <string>
#include <vector>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace temperature_check {

namespace {

// Read the entire contents of a sysfs file as a trimmed string. Returns false
// when the file does not exist or cannot be read. The stream is imbued with the
// "C" locale so that numeric parsing downstream is not affected by the host
// locale (e.g. comma decimal separators).
bool read_file(const fs::path &p, std::string &out) {
  boost::system::error_code ec;
  if (!fs::is_regular_file(p, ec)) return false;
  std::ifstream ifs(p.string().c_str());
  if (!ifs.good()) return false;
  try {
    ifs.imbue(std::locale("C"));
  } catch (...) {
    // If the "C" locale is unavailable for some reason just continue with the default.
  }
  std::string line;
  if (!std::getline(ifs, line)) return false;
  boost::algorithm::trim(line);
  out = line;
  return true;
}

// Parse a millidegree-Celsius sysfs value into degrees Celsius. Returns false on
// any parse error so the caller can skip the entry.
bool read_millidegree(const fs::path &p, double &out) {
  std::string raw;
  if (!read_file(p, raw)) return false;
  if (raw.empty()) return false;
  try {
    out = std::stoll(raw) / 1000.0;
  } catch (...) {
    return false;
  }
  return true;
}

// The web UI prefers underscore-separated keys (matching CheckSystem.cpp's cpu
// core naming), so spaces in sensor labels are normalized to underscores.
std::string normalize_name(std::string name) {
  std::replace(name.begin(), name.end(), ' ', '_');
  return name;
}

// Enumerate <base>/thermal_zone*/ reading `temp` and `type`.
void read_thermal_zones(const fs::path &base, zones_type &zones, std::set<std::string> &seen) {
  boost::system::error_code ec;
  if (!fs::is_directory(base, ec)) return;

  // Directory iteration order is unspecified; sort the zones so that
  // duplicate-name suffixes (assigned in iteration order below) are deterministic.
  std::vector<fs::path> dirs;
  for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->path().filename().string().compare(0, 12, "thermal_zone") == 0) dirs.push_back(it->path());
  }
  std::sort(dirs.begin(), dirs.end());

  for (const fs::path &dir : dirs) {
    const std::string fname = dir.filename().string();

    double temp;
    if (!read_millidegree(dir / "temp", temp)) continue;

    std::string type;
    if (!read_file(dir / "type", type) || type.empty()) type = fname;

    // Two zones of the same type (dual "acpitz"/"iwlwifi" zones) would produce
    // indistinguishable rows and colliding metric keys, masking one sensor:
    // suffix duplicates just like the hwmon path does.
    const std::string name = normalize_name(type);
    std::string unique = name;
    for (int n = 1; !seen.insert(unique).second; ++n) unique = name + "-" + std::to_string(n);

    thermal_zone z;
    z.name = unique;
    z.temperature = temp;
    z.active = true;
    zones.push_back(z);
  }
}

// Enumerate <base>/hwmon*/ reading each temp<N>_input, naming via the
// hwmon `name` file plus the optional temp<N>_label.
void read_hwmon(const fs::path &base, zones_type &zones, std::set<std::string> &seen) {
  boost::system::error_code ec;
  if (!fs::is_directory(base, ec)) return;

  // Directory iteration order is unspecified; sort the hwmon devices so that
  // duplicate-chip suffixes (assigned in iteration order below) are deterministic.
  std::vector<fs::path> devices;
  for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->path().filename().string().compare(0, 5, "hwmon") == 0) devices.push_back(it->path());
  }
  std::sort(devices.begin(), devices.end());

  std::map<std::string, int> chip_count;
  for (const fs::path &dir : devices) {
    const std::string fname = dir.filename().string();

    std::string hwmon_name;
    if (!read_file(dir / "name", hwmon_name) || hwmon_name.empty()) hwmon_name = fname;

    // A second chip with the same name (dual-socket "coretemp", multiple "nvme"
    // drives) gets a numeric suffix so its sensors are never dropped; the first
    // chip keeps the plain name so existing configs and metrics stay stable.
    const int dup = chip_count[hwmon_name]++;
    if (dup > 0) hwmon_name += "-" + std::to_string(dup);

    boost::system::error_code ec2;
    for (fs::directory_iterator fit(dir, ec2), fend; !ec2 && fit != fend; fit.increment(ec2)) {
      const std::string leaf = fit->path().filename().string();
      // Match temp<N>_input
      if (leaf.compare(0, 4, "temp") != 0) continue;
      const std::string::size_type suffix = leaf.rfind("_input");
      if (suffix == std::string::npos || suffix + 6 != leaf.size()) continue;

      // The numeric token between "temp" and "_input" (e.g. "1" in "temp1_input").
      const std::string num = leaf.substr(4, suffix - 4);

      double temp;
      if (!read_millidegree(dir / leaf, temp)) continue;

      std::string label;
      std::string name;
      if (read_file(dir / ("temp" + num + "_label"), label) && !label.empty()) {
        name = hwmon_name + "_" + label;
      } else {
        name = hwmon_name + "_temp" + num;
      }
      name = normalize_name(name);

      // Never drop a sensor over a name clash (duplicate labels within a chip or
      // overlap with a thermal zone name): suffix until the name is unique.
      std::string unique = name;
      for (int n = 1; !seen.insert(unique).second; ++n) unique = name + "-" + std::to_string(n);

      thermal_zone z;
      z.name = unique;
      z.temperature = temp;
      z.active = true;
      zones.push_back(z);
    }
  }
}

}  // namespace

zones_type read_temperature_from(const std::string &thermal_base, const std::string &hwmon_base) {
  zones_type zones;
  std::set<std::string> seen;

  read_thermal_zones(fs::path(thermal_base), zones, seen);
  read_hwmon(fs::path(hwmon_base), zones, seen);

  return zones;
}

zones_type read_temperature() { return read_temperature_from("/sys/class/thermal", "/sys/class/hwmon"); }

typedef thermal_zone filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Thermal zone / sensor name")
      .add_string_var("active", &filter_obj::get_active, "Whether the sensor is active");

  registry_.add_int_var("temperature", &filter_obj::get_temperature_i, "Temperature in degrees Celsius").add_int_perf("C");
}

void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("temperature > 70", "temperature > 90", "", filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${temperature}C", "${name}", "", "%(status): Temperature is ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  zones_type zones = read_temperature();
  if (zones.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No temperature sensors found");
  }

  for (const zones_type::value_type &v : zones) {
    const std::shared_ptr<filter_obj> record(new filter_obj(v));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

void build_temperature_metrics(PB::Metrics::MetricsBundle *parent) {
  using namespace nscapi::metrics;

  PB::Metrics::MetricsBundle *section = parent->add_children();
  section->set_key("temperature");

  zones_type zones = read_temperature();
  for (const zones_type::value_type &v : zones) {
    add_metric(section, v.get_name() + ".temperature", v.get_temperature_i());
  }
}

}  // namespace temperature_check
