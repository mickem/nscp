// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_sources.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <ctime>
#include <memory>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <utility>

namespace ncpa {

namespace {

void flatten(std::map<std::string, double> &numbers, std::map<std::string, std::string> &strings, const PB::Metrics::MetricsBundle &bundle,
             const std::string &path) {
  const std::string prefix = path.empty() ? bundle.key() : path + "." + bundle.key();
  for (const PB::Metrics::MetricsBundle &child : bundle.children()) {
    flatten(numbers, strings, child, prefix);
  }
  for (const PB::Metrics::Metric &metric : bundle.value()) {
    double number = 0;
    // numeric_value(), not has_gauge_value(): a producer that retypes a metric
    // as a counter must not silently disappear from this tree.
    if (nscapi::metrics::numeric_value(metric, number)) {
      numbers[prefix + "." + metric.key()] = number;
    } else if (metric.has_string_value()) {
      strings[prefix + "." + metric.key()] = metric.string_value().value();
    }
  }
}

// A whole number the metrics snapshot carries as a double, as the integer NCPA
// would report - byte counts and object counts are ints there, and the "%d" vs
// "%0.2f" difference reaches the Nagios line.
value as_int(const double v) { return value::from_int(static_cast<long long>(std::llround(v))); }

// The core number out of a CPU instance key, for sorting: `core 0` and `core_0`
// both sort as 0, and the aggregate `total` sorts last.
long core_index(const std::string &name) {
  const std::string label = nscapi::metrics::core_label(name);
  if (label == name) return 1000000;
  try {
    return std::stol(label);
  } catch (const std::exception &) {
    return 1000000;
  }
}

bool is_core(const std::string &name) { return core_index(name) != 1000000; }

std::shared_ptr<leaf_node> leaf(const std::string &name, values_type vals, const std::string &unit, const bool scalar = true) {
  auto out = std::make_shared<leaf_node>(name, std::move(vals), unit);
  out->set_scalar(scalar);
  return out;
}

std::shared_ptr<leaf_node> rate_leaf(const std::string &name, values_type vals, const std::string &unit) {
  auto out = leaf(name, std::move(vals), unit);
  out->set_rate(true);
  return out;
}

// -------------------------------------------------------------------------
// cpu
// -------------------------------------------------------------------------

node_ptr build_cpu(const metrics_snapshot &snapshot) {
  const auto cpu = std::make_shared<parent_node>("cpu");

  std::vector<std::string> cores;
  for (const std::string &instance : snapshot.instances("system.cpu.")) {
    if (is_core(instance)) cores.push_back(instance);
  }
  std::sort(cores.begin(), cores.end(), [](const std::string &a, const std::string &b) { return core_index(a) < core_index(b); });

  const auto per_core = [&](const std::string &metric) {
    values_type vals;
    for (const std::string &core : cores) {
      double v = 0;
      if (snapshot.get_number("system.cpu." + core + "." + metric, v)) vals.push_back(value::from_double(v));
    }
    return vals;
  };

  cpu->add(leaf("count", values_type{value::from_int(static_cast<long long>(cores.size()))}, "cores", false));
  // NCPA reports cpu/{user,system,idle} as cumulative milliseconds of CPU time,
  // because that is what psutil.cpu_times() hands it. The NSClient++ collector
  // measures the same four quantities as a share of the last five minutes, and
  // that is what these report - with a "%" unit that says so rather than an
  // "ms" one that would not. A `-w 80` on cpu/user therefore means "80 percent"
  // here, which is the threshold an operator meant anyway; against the real
  // agent the same switch compares against a monotonically rising counter and
  // alerts forever once it trips.
  cpu->add(leaf("idle", per_core("idle"), "%", false));
  cpu->add(leaf("percent", per_core("total"), "%", false));
  cpu->add(leaf("system", per_core("kernel"), "%", false));
  cpu->add(leaf("user", per_core("user"), "%", false));
  return cpu;
}

// -------------------------------------------------------------------------
// memory
// -------------------------------------------------------------------------

// One memory section of the snapshot as an NCPA memory node. `total`, `avail`
// and `used` are published identically on every platform; the percentage is
// computed here rather than read, because `physical.%` means the *available*
// share on Windows and the *used* share on Linux, while NCPA's `percent` is
// always the used share.
node_ptr build_memory_section(const metrics_snapshot &snapshot, const std::string &node_name, const std::string &section, const std::string &custom_output,
                              const bool add_primary_to_perfdata, const std::vector<std::string> &include) {
  double total = 0;
  double avail = 0;
  double used = 0;
  const bool has_total = snapshot.get_number("system.mem." + section + ".total", total);
  snapshot.get_number("system.mem." + section + ".avail", avail);
  if (!snapshot.get_number("system.mem." + section + ".used", used)) used = total - avail;
  if (!has_total) return {};

  const double percent = total > 0 ? (100.0 * used) / total : 0;

  const auto out = std::make_shared<runnable_parent_node>(node_name, "percent", "%");
  out->set_custom_output(custom_output);
  out->set_include(include);
  out->set_add_primary_to_perfdata(add_primary_to_perfdata);
  // Child order is the order the secondary values appear in the check line, so
  // it is part of the output contract, not an implementation detail.
  if (node_name == "virtual") {
    out->add(leaf("available", values_type{as_int(avail)}, "B"));
    out->add(leaf("total", values_type{as_int(total)}, "B"));
    out->add(leaf("percent", values_type{value::from_double(percent)}, "%"));
    out->add(leaf("free", values_type{as_int(avail)}, "B"));
    out->add(leaf("used", values_type{as_int(used)}, "B"));
  } else {
    out->add(leaf("used", values_type{as_int(used)}, "B"));
    out->add(leaf("total", values_type{as_int(total)}, "B"));
    out->add(leaf("percent", values_type{value::from_double(percent)}, "%"));
    out->add(leaf("free", values_type{as_int(avail)}, "B"));
  }
  return out;
}

node_ptr build_memory(const metrics_snapshot &snapshot) {
  const auto memory = std::make_shared<parent_node>("memory");
  // NCPA's `virtual` is psutil's virtual_memory(), which is physical RAM - not
  // the Windows "virtual address space" the agent publishes under that name.
  if (const node_ptr v = build_memory_section(snapshot, "virtual", "physical", "Memory usage was", true, {})) memory->add(v);

  // Linux publishes a `swap` section and Windows a `page` one; both are what
  // NCPA calls swap.
  double probe = 0;
  const std::string swap_section = snapshot.get_number("system.mem.swap.total", probe) ? "swap" : "page";
  if (const node_ptr s = build_memory_section(snapshot, "swap", swap_section, "Swap usage was", false, {"total", "used", "free", "percent"})) {
    memory->add(s);
  }
  return memory;
}

// -------------------------------------------------------------------------
// disk
// -------------------------------------------------------------------------

node_ptr build_disk(const metrics_snapshot &snapshot) {
  const auto disk = std::make_shared<parent_node>("disk");

  const auto logical = std::make_shared<parent_node>("logical");
  for (const std::string &drive : snapshot.instances("disk.free.")) {
    double total = 0;
    double free = 0;
    double used = 0;
    if (!snapshot.get_number("disk.free." + drive + ".total", total)) continue;
    snapshot.get_number("disk.free." + drive + ".free", free);
    if (!snapshot.get_number("disk.free." + drive + ".used", used)) used = total - free;
    double used_pct = 0;
    if (!snapshot.get_number("disk.free." + drive + ".used_pct", used_pct)) used_pct = total > 0 ? (100.0 * used) / total : 0;

    const auto mount = std::make_shared<runnable_parent_node>(encode_mountpoint(drive), "used_percent", "%");
    mount->set_custom_output("Used disk space was");
    mount->set_include({"total", "used", "free", "used_percent"});
    mount->add(leaf("used_percent", values_type{value::from_double(used_pct)}, "%"));
    mount->add(leaf("used", values_type{as_int(used)}, "B"));
    mount->add(leaf("free", values_type{as_int(free)}, "B"));
    // NCPA reports the device as a one-element list with no unit, so the body
    // is a bare array rather than the [value, unit] pair every other leaf is.
    mount->add(leaf("device_name", values_type{value::from_string(drive)}, "", false));
    mount->add(leaf("total", values_type{as_int(total)}, "B"));
    logical->add(mount);
  }
  disk->add(logical);

  const auto physical = std::make_shared<parent_node>("physical");
  for (const std::string &dev : snapshot.instances("disk.io.")) {
    double probe = 0;
    if (!snapshot.get_number("disk.io." + dev + ".read_bytes_per_sec", probe)) continue;

    const auto entry = std::make_shared<parent_node>(encode_mountpoint(dev));
    const auto add_rate = [&](const std::string &name, const std::string &metric, const std::string &unit) {
      double v = 0;
      if (snapshot.get_number("disk.io." + dev + "." + metric, v)) entry->add(rate_leaf(name, values_type{value::from_double(v)}, unit));
    };
    // NCPA's disk/physical counters are cumulative totals since boot; the
    // NSClient++ collector only ever publishes the per-second rate, so that is
    // what these carry. `delta` on them is a no-op rather than a difference of
    // two rates - see leaf_node::set_rate.
    add_rate("read_bytes", "read_bytes_per_sec", "B");
    add_rate("write_bytes", "write_bytes_per_sec", "B");
    add_rate("read_count", "reads_per_sec", "c");
    add_rate("write_count", "writes_per_sec", "c");
    add_rate("read_time", "read_latency", "ms");
    add_rate("write_time", "write_latency", "ms");
    physical->add(entry);
  }
  disk->add(physical);
  return disk;
}

// -------------------------------------------------------------------------
// interface
// -------------------------------------------------------------------------

node_ptr build_interface(const metrics_snapshot &snapshot) {
  const auto interfaces = std::make_shared<parent_node>("interface");
  for (const std::string &nic : snapshot.instances("system.network.")) {
    const std::string base = "system.network." + nic + ".";

    double sent = 0;
    double recv = 0;
    // Linux publishes `received` / `sent`; the Windows collector publishes the
    // WMI names. Either way the number is already bytes per second.
    const bool unix_style = snapshot.get_number(base + "sent", sent);
    if (unix_style) {
      snapshot.get_number(base + "received", recv);
    } else {
      if (!snapshot.get_number(base + "BytesSentPersec", sent)) continue;
      snapshot.get_number(base + "BytesReceivedPersec", recv);
    }

    const auto entry = std::make_shared<runnable_parent_node>(nic, "bytes_sent", "");
    const auto add_optional = [&](const std::string &name, const std::string &metric, const std::string &unit) {
      double v = 0;
      if (snapshot.get_number(base + metric, v)) entry->add(rate_leaf(name, values_type{value::from_double(v)}, unit));
    };

    // Child order matches NCPA's, because it is the order the secondary values
    // appear in the check line.
    add_optional("packets_sent", "PacketsSentPersec", "packets");
    add_optional("dropin", "PacketsReceivedDiscarded", "packets");
    entry->add(rate_leaf("bytes_recv", values_type{value::from_double(recv)}, "B"));
    add_optional("packets_recv", "PacketsReceivedPersec", "packets");
    add_optional("errin", "PacketsReceivedErrors", "errors");
    add_optional("dropout", "PacketsOutboundDiscarded", "packets");
    entry->add(rate_leaf("bytes_sent", values_type{value::from_double(sent)}, "B"));
    add_optional("errout", "PacketsOutboundErrors", "errors");

    // NCPA's status is numeric - 0 up, 2 down, 3 unknown - and the check
    // renders it as a word. All three are used: Linux reports `unknown` for
    // any interface whose driver does not track carrier (loopback always
    // does), and calling that "down" would alert on a machine that is fine.
    std::string status;
    long numeric_status = 3;
    if (snapshot.get_string(base + "status", status)) {
      // The Linux collector publishes /sys/class/net/<if>/operstate.
      if (boost::iequals(status, "up")) {
        numeric_status = 0;
      } else if (boost::iequals(status, "down") || boost::iequals(status, "lowerlayerdown")) {
        numeric_status = 2;
      }
    } else if (snapshot.get_string(base + "NetConnectionStatus", status)) {
      // The Windows collector publishes the WMI NetConnectionStatus, where 2
      // is "Connected" and everything else names a way of not being connected.
      if (status == "2" || boost::iequals(status, "connected")) {
        numeric_status = 0;
      } else if (!status.empty()) {
        numeric_status = 2;
      }
    }
    entry->add(leaf("status", values_type{value::from_int(numeric_status)}, ""));
    interfaces->add(entry);
  }
  return interfaces;
}

// -------------------------------------------------------------------------
// system / user
// -------------------------------------------------------------------------

node_ptr build_system(const metrics_snapshot &snapshot, const tree_options &options) {
  const system_info info = sysinfo::gather();
  const auto system = std::make_shared<parent_node>("system");

  system->add(leaf("node", values_type{value::from_string(info.node)}, ""));
  system->add(leaf("machine", values_type{value::from_string(info.machine)}, ""));

  double uptime = 0;
  snapshot.get_number("system.uptime.ticks.raw", uptime);
  system->add(leaf("uptime", values_type{value::from_double(uptime)}, "s"));

  system->add(leaf("version", values_type{value::from_string(info.version)}, ""));
  system->add(leaf("time", values_type{value::from_double(static_cast<double>(std::time(nullptr)))}, ""));
  system->add(leaf("release", values_type{value::from_string(info.release)}, ""));
  system->add(leaf("timezone", values_type{value::from_string(info.timezone)}, "", false));
  // The node exists either way: the XI wizard reads it to decide what the agent
  // supports, so removing it would break discovery rather than hide anything.
  system->add(leaf("agent_version", values_type{value::from_string(options.expose_version ? options.agent_version : std::string("hidden"))}, ""));
  system->add(leaf("system", values_type{value::from_string(info.system)}, ""));
  system->add(leaf("processor", values_type{value::from_string(info.processor)}, ""));
  return system;
}

node_ptr build_user() {
  const std::vector<std::string> users = sysinfo::logged_on_users();
  const auto user = std::make_shared<parent_node>("user");
  user->add(leaf("count", values_type{value::from_int(static_cast<long long>(users.size()))}, "users"));
  values_type names;
  names.reserve(users.size());
  for (const std::string &name : users) names.push_back(value::from_string(name));
  user->add(leaf("list", std::move(names), "users", false));
  return user;
}

}  // namespace

void metrics_snapshot::set(const PB::Metrics::MetricsMessage &message) {
  std::map<std::string, double> numbers;
  std::map<std::string, std::string> strings;
  for (const PB::Metrics::MetricsMessage::Response &payload : message.payload()) {
    for (const PB::Metrics::MetricsBundle &bundle : payload.bundles()) {
      flatten(numbers, strings, bundle, "");
    }
  }
  const boost::mutex::scoped_lock lock(mutex_);
  numbers_ = std::move(numbers);
  strings_ = std::move(strings);
}

bool metrics_snapshot::get_number(const std::string &key, double &out) const {
  const boost::mutex::scoped_lock lock(mutex_);
  const auto it = numbers_.find(key);
  if (it == numbers_.end()) return false;
  out = it->second;
  return true;
}

bool metrics_snapshot::get_string(const std::string &key, std::string &out) const {
  const boost::mutex::scoped_lock lock(mutex_);
  const auto it = strings_.find(key);
  if (it == strings_.end()) return false;
  out = it->second;
  return true;
}

std::vector<std::string> metrics_snapshot::instances(const std::string &prefix) const {
  std::vector<std::string> out;
  const boost::mutex::scoped_lock lock(mutex_);
  const auto collect = [&](const std::string &key) {
    if (key.compare(0, prefix.size(), prefix) != 0) return;
    const std::string rest = key.substr(prefix.size());
    // The instance is everything up to the LAST dot, not the first: a mount
    // point ("/var/lib") and a metric key that itself contains a dot
    // ("physical.%") both live in this namespace, and splitting on the first
    // dot would invent an instance for each.
    const std::size_t split = rest.find_last_of('.');
    if (split == std::string::npos || split == 0) return;
    const std::string instance = rest.substr(0, split);
    if (std::find(out.begin(), out.end(), instance) == out.end()) out.push_back(instance);
  };
  for (const auto &entry : numbers_) collect(entry.first);
  for (const auto &entry : strings_) collect(entry.first);
  return out;
}

bool metrics_snapshot::empty() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return numbers_.empty() && strings_.empty();
}

node_ptr build_root(const metrics_snapshot &snapshot, const tree_options &options, query_dispatcher *dispatcher) {
  const auto root = std::make_shared<parent_node>("root");
  root->add(build_cpu(snapshot));
  root->add(build_memory(snapshot));
  root->add(build_disk(snapshot));
  root->add(build_interface(snapshot));
  root->add(std::make_shared<plugins_node>("plugins", dispatcher));
  root->add(build_user());
  root->add(build_system(snapshot, options));
  // Unlike every node above, these two are not in the metrics snapshot: they
  // are answered by dispatching the agent's own check_service / check_process.
  root->add(std::make_shared<services_node>("services", dispatcher));
  root->add(std::make_shared<processes_node>("processes", dispatcher));
  return root;
}

}  // namespace ncpa
