// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_battery.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <locale>
#include <memory>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>

namespace battery_check {

namespace {

constexpr auto POWER_SUPPLY_PATH = "/sys/class/power_supply";

// Read a single sysfs attribute file. Returns true and fills `value` (trimmed)
// on success, false if the file is missing or unreadable. The stream is imbued
// with the "C" locale so numeric parsing downstream is not affected by the
// process locale.
bool read_attr(const boost::filesystem::path &dir, const std::string &name, std::string &value) {
  boost::filesystem::path p = dir / name;
  std::ifstream ifs;
  try {
    ifs.imbue(std::locale("C"));
  } catch (...) {
    // Fall back to the default locale if "C" is unavailable.
  }
  ifs.open(p.string().c_str());
  if (!ifs.is_open()) return false;
  std::string line;
  if (!std::getline(ifs, line)) return false;
  boost::algorithm::trim(line);
  value = line;
  return true;
}

// Read a sysfs attribute as a long long. Returns true on success.
bool read_attr_ll(const boost::filesystem::path &dir, const std::string &name, long long &value) {
  std::string raw;
  if (!read_attr(dir, name, raw)) return false;
  try {
    value = std::stoll(raw);
    return true;
  } catch (...) {
    return false;
  }
}

// Normalise a sysfs status string ("Charging", "Not charging", ...) into the
// lowercase, underscore-separated form used by the filter ("charging",
// "not_charging", ...).
std::string normalize_status(std::string status) {
  boost::algorithm::to_lower(status);
  boost::algorithm::replace_all(status, " ", "_");
  if (status.empty()) return "unknown";
  return status;
}

// Convert a charge value (uAh) to energy (mWh) using a reference voltage (uV):
// uAh * uV = pWh, so divide by 1e9 for mWh. Returns 0 if no voltage is known.
long long charge_to_mwh(long long charge_uah, long long voltage_uv) {
  if (voltage_uv <= 0) return 0;
  return charge_uah * voltage_uv / 1000000000LL;
}

long long ll_abs(long long v) { return v < 0 ? -v : v; }

void read_one_battery(const boost::filesystem::path &entry, battery_info &info) {
  info.name = entry.filename().string();

  std::string status;
  if (read_attr(entry, "status", status)) {
    info.status = normalize_status(status);
  }

  long long energy_now = 0;
  long long energy_full = 0;
  long long energy_full_design = 0;
  const bool have_energy_now = read_attr_ll(entry, "energy_now", energy_now);
  const bool have_energy_full = read_attr_ll(entry, "energy_full", energy_full);
  const bool have_energy_full_design = read_attr_ll(entry, "energy_full_design", energy_full_design);

  long long charge_now = 0;
  long long charge_full = 0;
  long long charge_full_design = 0;
  const bool have_charge_now = read_attr_ll(entry, "charge_now", charge_now);
  const bool have_charge_full = read_attr_ll(entry, "charge_full", charge_full);
  const bool have_charge_full_design = read_attr_ll(entry, "charge_full_design", charge_full_design);

  // Reference voltage for converting charge_* (uAh) drivers to Windows-parity
  // mWh/mW units. The design voltage is stable across load, so prefer it.
  long long voltage_uv = 0;
  if (!read_attr_ll(entry, "voltage_min_design", voltage_uv)) {
    read_attr_ll(entry, "voltage_now", voltage_uv);
  }

  // Prefer the driver's own capacity attribute; some drivers omit it and only
  // expose the charge_* or energy_* pairs, so derive the percent from those.
  long long capacity = -1;
  if (read_attr_ll(entry, "capacity", capacity)) {
    info.charge_percent = capacity;
  } else if (have_charge_now && have_charge_full && charge_full > 0) {
    info.charge_percent = charge_now * 100 / charge_full;
  } else if (have_energy_now && have_energy_full && energy_full > 0) {
    info.charge_percent = energy_now * 100 / energy_full;
  }
  // charge_now can briefly exceed charge_full on a freshly charged battery.
  if (info.charge_percent > 100) info.charge_percent = 100;

  if (have_energy_full && have_energy_full_design && energy_full_design > 0) {
    info.health_percent = energy_full * 100 / energy_full_design;
  } else if (have_charge_full && have_charge_full_design && charge_full_design > 0) {
    info.health_percent = charge_full * 100 / charge_full_design;
  }

  if (have_energy_full_design) {
    info.design_capacity = energy_full_design / 1000;
  } else if (have_charge_full_design) {
    info.design_capacity = charge_to_mwh(charge_full_design, voltage_uv);
  }
  if (have_energy_full) {
    info.full_capacity = energy_full / 1000;
  } else if (have_charge_full) {
    info.full_capacity = charge_to_mwh(charge_full, voltage_uv);
  }
  if (have_energy_now) {
    info.remaining_capacity = energy_now / 1000;
  } else if (have_charge_now) {
    info.remaining_capacity = charge_to_mwh(charge_now, voltage_uv);
  }

  // Rate: power_now is uW; current_now is uA and needs the voltage. The sign
  // indicates direction on some drivers, the status already tells us that.
  long long rate_mw = 0;
  long long power_uw = 0;
  long long current_ua = 0;
  const bool have_power = read_attr_ll(entry, "power_now", power_uw);
  const bool have_current = read_attr_ll(entry, "current_now", current_ua);
  if (have_power) {
    rate_mw = ll_abs(power_uw) / 1000;
  } else if (have_current) {
    rate_mw = charge_to_mwh(ll_abs(current_ua), voltage_uv);
  }
  if (info.status == "charging") {
    info.charge_rate = rate_mw;
  } else if (info.status == "discharging") {
    info.discharge_rate = rate_mw;
  }

  long long time_to_empty = 0;
  if (read_attr_ll(entry, "time_to_empty_now", time_to_empty)) {
    info.time_remaining = time_to_empty;
  } else if (info.status == "discharging") {
    // remaining / rate gives hours; the u-prefixes cancel out.
    if (have_energy_now && have_power && ll_abs(power_uw) > 0) {
      info.time_remaining = energy_now * 3600 / ll_abs(power_uw);
    } else if (have_charge_now && have_current && ll_abs(current_ua) > 0) {
      info.time_remaining = charge_now * 3600 / ll_abs(current_ua);
    }
  }

  long long present = 1;
  if (!read_attr_ll(entry, "present", present)) present = 1;
  // Mirror the Windows behaviour for unknown data: a battery whose charge
  // cannot be determined reports battery_present=false so the default filter
  // (battery_present = 'true') never evaluates thresholds against the -1
  // charge sentinel.
  info.battery_present = present == 1 && info.charge_percent >= 0;
}

}  // namespace

std::string battery_info::get_time_remaining_s() const {
  if (time_remaining < 0) return "unknown";
  return str::format::itos_as_time(time_remaining * 1000);
}

std::string battery_info::show() const {
  std::string charge = charge_percent < 0 ? "unknown" : (std::to_string(charge_percent) + "%");
  return name + ": " + charge + " (" + status + ", " + power_source + ")";
}

void battery_info::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, name + ".charge", charge_percent);
  add_metric(section, name + ".health", health_percent);
  add_metric(section, name + ".status", status);
  add_metric(section, name + ".power_source", power_source);
  add_metric(section, name + ".battery_present", get_battery_present());
  if (time_remaining >= 0) add_metric(section, name + ".time_remaining", time_remaining);
  if (charge_rate > 0) add_metric(section, name + ".charge_rate", charge_rate);
  if (discharge_rate > 0) add_metric(section, name + ".discharge_rate", discharge_rate);
  if (design_capacity > 0) add_metric(section, name + ".design_capacity", design_capacity);
  if (full_capacity > 0) add_metric(section, name + ".full_capacity", full_capacity);
  if (remaining_capacity > 0) add_metric(section, name + ".remaining_capacity", remaining_capacity);
}

batteries_type read_battery() { return read_battery_from(POWER_SUPPLY_PATH); }

batteries_type read_battery_from(const std::string &power_supply_path) {
  batteries_type result;

  boost::filesystem::path root(power_supply_path);
  boost::system::error_code ec;
  if (!boost::filesystem::is_directory(root, ec)) return result;

  // Tri-state AC status: -1 unknown, 0 offline, 1 online. Computed from any
  // "Mains" entry and applied to every battery after scanning.
  int ac_online = -1;

  for (boost::filesystem::directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
    const boost::filesystem::path &entry = it->path();

    std::string type;
    if (!read_attr(entry, "type", type)) continue;

    if (type == "Battery") {
      battery_info info;
      read_one_battery(entry, info);
      result.push_back(info);
    } else if (type == "Mains") {
      long long online = 0;
      if (read_attr_ll(entry, "online", online)) {
        if (online == 1) {
          ac_online = 1;
        } else if (ac_online != 1) {
          ac_online = 0;
        }
      }
    }
  }

  std::string power_source;
  if (ac_online == 1) {
    power_source = "ac";
  } else if (ac_online == 0) {
    power_source = "battery";
  } else {
    power_source = "unknown";
  }
  for (battery_info &b : result) {
    b.power_source = power_source;
  }

  return result;
}

namespace check {

typedef battery_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Battery name")
      .add_string_var("power_source", &filter_obj::get_power_source, "Power source: ac/battery/unknown")
      .add_string_var("status", &filter_obj::get_status, "Charge status")
      .add_string_var("battery_present", &filter_obj::get_battery_present, "Whether a battery is present: 'true' or 'false'")
      .add_string_var("present", &filter_obj::get_battery_present, "Alias for battery_present");

  registry_.add_int_var("charge", &filter_obj::get_charge_percent, "Battery charge percent")
      .add_int_perf("%")
      .add_int_var("health", &filter_obj::get_health_percent, "Battery health percent (full/design capacity)")
      .add_int_perf("%")
      .add_int_var("time_remaining", &filter_obj::get_time_remaining, "Estimated time remaining in seconds (-1 if unknown or on AC)")
      .add_int_var("charge_rate", &filter_obj::get_charge_rate, "Current charge rate in mW (when charging)")
      .add_int_var("discharge_rate", &filter_obj::get_discharge_rate, "Current discharge rate in mW (when discharging)")
      .add_int_var("design_capacity", &filter_obj::get_design_capacity, "Design capacity in mWh")
      .add_int_var("full_capacity", &filter_obj::get_full_capacity, "Current full charge capacity in mWh")
      .add_int_var("remaining_capacity", &filter_obj::get_remaining_capacity, "Current remaining capacity in mWh");

  registry_.add_human_string("time_remaining", &filter_obj::get_time_remaining_s, "Estimated time remaining (human readable)");
}

}  // namespace check

void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                   const batteries_type &batteries) {
  typedef check::filter_type filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("charge < 20", "charge < 10", "battery_present = 'true'", filter.get_filter_syntax(), "warning");
  // The empty-syntax renders when zero records match (no battery, or all
  // filtered out) — without it the top-syntax renders a useless ": ".
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${charge}% (${power_source}, ${status})", "${name}", "No battery found",
                           "%(status): No battery found or all batteries ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  // No battery (desktops, servers) yields zero matched records and falls
  // through to the filter's empty-state — user-configurable and defaulting to
  // "warning", exactly as on Windows.
  for (const battery_info &b : batteries) {
    const std::shared_ptr<check::filter_obj> record(new check::filter_obj(b));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_battery(request, response, read_battery());
}

void build_battery_metrics(PB::Metrics::MetricsBundle *parent) {
  batteries_type batteries = read_battery();
  if (batteries.empty()) return;

  PB::Metrics::MetricsBundle *section = parent->add_children();
  section->set_key("battery");
  for (const battery_info &b : batteries) {
    b.build_metrics(section);
  }
}

}  // namespace battery_check
