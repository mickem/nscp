// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The snapshot-to-tree mapping: which NSClient++ metric key ends up under which
// NCPA node, and in what shape. A metrics key is the wire format for every
// other consumer too, so a mapping that drifts here shows up as an empty node
// in Nagios rather than as an error.

#include "ncpa_sources.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>

namespace json = boost::json;

namespace {

using ncpa::metrics_snapshot;
using ncpa::node_ptr;
using ncpa::walk_context;

// A bundle builder that mirrors how the collectors publish: a `system` bundle
// with `cpu` / `mem` / `network` / `uptime` children, and a `disk` bundle with
// `free` / `io` children.
class snapshot_builder {
 public:
  snapshot_builder() { response_ = message_.add_payload(); }

  PB::Metrics::MetricsBundle *bundle(const std::string &key) {
    PB::Metrics::MetricsBundle *out = response_->add_bundles();
    out->set_key(key);
    return out;
  }
  static PB::Metrics::MetricsBundle *child(PB::Metrics::MetricsBundle *parent, const std::string &key) {
    PB::Metrics::MetricsBundle *out = parent->add_children();
    out->set_key(key);
    return out;
  }
  static void gauge(PB::Metrics::MetricsBundle *bundle, const std::string &key, const double v) { nscapi::metrics::metric(bundle, key).gauge(v); }
  static void info(PB::Metrics::MetricsBundle *bundle, const std::string &key, const std::string &v) { nscapi::metrics::metric(bundle, key).info(v); }

  void apply(metrics_snapshot &snapshot) const { snapshot.set(message_); }

 private:
  PB::Metrics::MetricsMessage message_;
  PB::Metrics::MetricsMessage::Response *response_ = nullptr;
};

json::value walk_node(const metrics_snapshot &snapshot, const std::string &path) {
  ncpa::tree_options options;
  options.agent_version = "0.0.0-test";
  const node_ptr root = ncpa::build_root(snapshot, options, nullptr);
  const node_ptr node = ncpa::resolve(root, ncpa::split_accessor(path), "/api/" + path);
  return json::parse(json::serialize(node->walk(walk_context())));
}

// --------------------------------------------------------------------------
// the snapshot itself
// --------------------------------------------------------------------------

TEST(ncpa_snapshot, flattens_nested_bundles_to_dotted_keys) {
  snapshot_builder builder;
  PB::Metrics::MetricsBundle *system = builder.bundle("system");
  PB::Metrics::MetricsBundle *mem = snapshot_builder::child(system, "mem");
  snapshot_builder::gauge(mem, "physical.total", 1024);
  snapshot_builder::info(mem, "note", "hello");

  metrics_snapshot snapshot;
  EXPECT_TRUE(snapshot.empty());
  builder.apply(snapshot);
  EXPECT_FALSE(snapshot.empty());

  double number = 0;
  EXPECT_TRUE(snapshot.get_number("system.mem.physical.total", number));
  EXPECT_DOUBLE_EQ(number, 1024);
  std::string text;
  EXPECT_TRUE(snapshot.get_string("system.mem.note", text));
  EXPECT_EQ(text, "hello");
  EXPECT_FALSE(snapshot.get_number("system.mem.nope", number));
}

TEST(ncpa_snapshot, instances_split_on_the_last_dot) {
  // A metric key can contain a dot of its own ("physical.%") and so can a
  // mount point ("/mnt/data.1"); splitting on the first dot would invent an
  // instance for each.
  snapshot_builder builder;
  PB::Metrics::MetricsBundle *disk = builder.bundle("disk");
  PB::Metrics::MetricsBundle *free = snapshot_builder::child(disk, "free");
  snapshot_builder::gauge(free, "/mnt/data.1.total", 10);
  snapshot_builder::gauge(free, "/mnt/data.1.free", 4);

  metrics_snapshot snapshot;
  builder.apply(snapshot);
  const std::vector<std::string> instances = snapshot.instances("disk.free.");
  ASSERT_EQ(instances.size(), 1u);
  EXPECT_EQ(instances[0], "/mnt/data.1");
}

// --------------------------------------------------------------------------
// the built-in nodes
// --------------------------------------------------------------------------

// A snapshot is not copyable (it owns its lock), so the fixture fills one in
// place rather than returning it.
void fill_machine(metrics_snapshot &snapshot) {
  snapshot_builder builder;
  PB::Metrics::MetricsBundle *system = builder.bundle("system");

  PB::Metrics::MetricsBundle *cpu = snapshot_builder::child(system, "cpu");
  // The aggregate `total` instance is a sibling of the cores and must not be
  // counted as one.
  for (const char *core : {"total", "core_0", "core_1"}) {
    snapshot_builder::gauge(cpu, std::string(core) + ".idle", 90);
    snapshot_builder::gauge(cpu, std::string(core) + ".total", 10);
    snapshot_builder::gauge(cpu, std::string(core) + ".user", 6);
    snapshot_builder::gauge(cpu, std::string(core) + ".kernel", 4);
  }

  PB::Metrics::MetricsBundle *mem = snapshot_builder::child(system, "mem");
  snapshot_builder::gauge(mem, "physical.total", 8589934592.0);
  snapshot_builder::gauge(mem, "physical.avail", 4294967296.0);
  snapshot_builder::gauge(mem, "physical.used", 4294967296.0);
  // The `%` key means the AVAILABLE share on Windows and the USED share on
  // Linux, so the tree must not read it - a deliberately wrong value here
  // catches a regression that starts doing so.
  snapshot_builder::gauge(mem, "physical.%", 12345);
  snapshot_builder::gauge(mem, "swap.total", 1000);
  snapshot_builder::gauge(mem, "swap.avail", 900);
  snapshot_builder::gauge(mem, "swap.used", 100);

  PB::Metrics::MetricsBundle *uptime = snapshot_builder::child(system, "uptime");
  snapshot_builder::gauge(uptime, "ticks.raw", 90061);

  PB::Metrics::MetricsBundle *network = snapshot_builder::child(system, "network");
  snapshot_builder::gauge(network, "eth0.received", 100);
  snapshot_builder::gauge(network, "eth0.sent", 200);
  snapshot_builder::info(network, "eth0.status", "up");
  snapshot_builder::gauge(network, "eth1.received", 0);
  snapshot_builder::gauge(network, "eth1.sent", 0);
  snapshot_builder::info(network, "eth1.status", "down");
  snapshot_builder::gauge(network, "lo.received", 0);
  snapshot_builder::gauge(network, "lo.sent", 0);
  // Linux reports `unknown` for any interface whose driver does not track
  // carrier - loopback always does - and that must not read as "down".
  snapshot_builder::info(network, "lo.status", "unknown");

  PB::Metrics::MetricsBundle *disk = builder.bundle("disk");
  PB::Metrics::MetricsBundle *free = snapshot_builder::child(disk, "free");
  snapshot_builder::gauge(free, "/.total", 1000);
  snapshot_builder::gauge(free, "/.free", 250);
  snapshot_builder::gauge(free, "/.used", 750);
  snapshot_builder::gauge(free, "/.used_pct", 75);
  PB::Metrics::MetricsBundle *io = snapshot_builder::child(disk, "io");
  snapshot_builder::gauge(io, "sda.read_bytes_per_sec", 4096);
  snapshot_builder::gauge(io, "sda.write_bytes_per_sec", 2048);

  builder.apply(snapshot);
}

TEST(ncpa_tree_sources, cpu_counts_cores_and_excludes_the_aggregate) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  EXPECT_EQ(json::serialize(walk_node(snapshot, "cpu/count")), R"({"count":[[2],"cores"]})");
  EXPECT_EQ(json::serialize(walk_node(snapshot, "cpu/percent")), R"({"percent":[[1E1,1E1],"%"]})");
}

TEST(ncpa_tree_sources, memory_virtual_reports_the_used_share_not_the_published_percent) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  const json::value body = walk_node(snapshot, "memory/virtual");
  const json::object &node = body.at("virtual").as_object();
  EXPECT_EQ(node.at("total").as_array().at(0).as_int64(), 8589934592LL);
  EXPECT_EQ(node.at("available").as_array().at(0).as_int64(), 4294967296LL);
  // 50, computed from total and used - not the 12345 the `%` key carries.
  EXPECT_DOUBLE_EQ(node.at("percent").as_array().at(0).as_double(), 50.0);
}

TEST(ncpa_tree_sources, memory_swap_is_present_and_checkable) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  ncpa::tree_options options;
  const node_ptr root = ncpa::build_root(snapshot, options, nullptr);
  const node_ptr swap = ncpa::resolve(root, {"memory", "swap"}, "/api/memory/swap");

  const ncpa::check_result result = swap->run_check(walk_context(), ncpa::render_options());
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text, "OK: Swap usage was 10.00 % (Used: 100 B, Total: 1000 B, Free: 900 B) | 'used'=100B;;; 'total'=1000B;;; 'free'=900B;;;");
}

TEST(ncpa_tree_sources, a_mount_point_is_keyed_by_its_encoded_name) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  const json::value body = walk_node(snapshot, "disk/logical");
  const json::object &logical = body.at("logical").as_object();
  ASSERT_EQ(logical.size(), 1u);
  // "/" is spelled "|" in the tree.
  const json::object &mount = logical.at("|").as_object();
  EXPECT_EQ(mount.at("total").as_array().at(0).as_int64(), 1000);
  EXPECT_EQ(mount.at("used").as_array().at(0).as_int64(), 750);
  EXPECT_EQ(mount.at("free").as_array().at(0).as_int64(), 250);
  EXPECT_DOUBLE_EQ(mount.at("used_percent").as_array().at(0).as_double(), 75.0);
  EXPECT_EQ(mount.at("device_name").as_array().at(0).as_string(), "/");
}

TEST(ncpa_tree_sources, a_disk_check_reads_as_the_agent_would_render_it) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  ncpa::tree_options options;
  const node_ptr root = ncpa::build_root(snapshot, options, nullptr);
  const node_ptr mount = ncpa::resolve(root, {"disk", "logical", "|"}, "/api/disk/logical/|");

  walk_context ctx;
  ctx.opts.warning = "80";
  ctx.opts.critical = "90";
  const ncpa::check_result result = mount->run_check(ctx, ncpa::render_options());
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text, "OK: Used disk space was 75.00 % (Used: 750 B, Free: 250 B, Total: 1000 B) | 'used'=750B;;; 'free'=250B;;; 'total'=1000B;;;");
}

TEST(ncpa_tree_sources, disk_physical_carries_the_collectors_rates) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  const json::value body = walk_node(snapshot, "disk/physical/sda");
  const json::object &sda = body.at("sda").as_object();
  EXPECT_DOUBLE_EQ(sda.at("read_bytes").as_array().at(0).as_double(), 4096.0);
  EXPECT_DOUBLE_EQ(sda.at("write_bytes").as_array().at(0).as_double(), 2048.0);
}

TEST(ncpa_tree_sources, an_interface_maps_the_collectors_per_second_values) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  const json::value body = walk_node(snapshot, "interface/eth0");
  const json::object &nic = body.at("eth0").as_object();
  EXPECT_DOUBLE_EQ(nic.at("bytes_sent").as_array().at(0).as_double(), 200.0);
  EXPECT_DOUBLE_EQ(nic.at("bytes_recv").as_array().at(0).as_double(), 100.0);
  // NCPA's status is numeric: 0 up, 2 down, 3 unknown.
  EXPECT_EQ(nic.at("status").as_int64(), 0);
  EXPECT_EQ(walk_node(snapshot, "interface/eth1").at("eth1").as_object().at("status").as_int64(), 2);
  EXPECT_EQ(walk_node(snapshot, "interface/lo").at("lo").as_object().at("status").as_int64(), 3);
}

TEST(ncpa_tree_sources, system_uptime_comes_from_the_snapshot) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  ncpa::tree_options options;
  const node_ptr root = ncpa::build_root(snapshot, options, nullptr);
  const node_ptr uptime = ncpa::resolve(root, {"system", "uptime"}, "/api/system/uptime");
  const ncpa::check_result result = uptime->run_check(walk_context(), ncpa::render_options());
  EXPECT_EQ(result.stdout_text, "OK: Uptime was 1 day 1 hour 1 minute 1 second | 'uptime'=90061.00s;;;");
}

TEST(ncpa_tree_sources, agent_version_is_redacted_when_expose_version_is_off) {
  metrics_snapshot snapshot;
  fill_machine(snapshot);
  ncpa::tree_options options;
  options.agent_version = "0.19.0";
  options.expose_version = false;
  node_ptr root = ncpa::build_root(snapshot, options, nullptr);
  json::value body = json::parse(json::serialize(ncpa::resolve(root, {"system", "agent_version"}, "")->walk(walk_context())));
  // The node still exists - the Nagios XI wizard reads it to decide what the
  // agent supports, so removing it would break discovery rather than hide
  // anything.
  EXPECT_EQ(body.at("agent_version").as_string(), "hidden");

  options.expose_version = true;
  root = ncpa::build_root(snapshot, options, nullptr);
  body = json::parse(json::serialize(ncpa::resolve(root, {"system", "agent_version"}, "")->walk(walk_context())));
  EXPECT_EQ(body.at("agent_version").as_string(), "0.19.0");
}

TEST(ncpa_tree_sources, an_empty_snapshot_still_answers_a_walkable_tree) {
  // The collector needs a second to produce its first sample, and a monitoring
  // server that polls in that window must get an empty node rather than an
  // error - which is also what the real agent does on a machine with no disks.
  const metrics_snapshot snapshot;
  const json::value body = walk_node(snapshot, "");
  const json::object &root = body.at("root").as_object();
  EXPECT_TRUE(root.contains("cpu"));
  EXPECT_TRUE(root.contains("memory"));
  EXPECT_TRUE(root.contains("disk"));
  EXPECT_TRUE(root.contains("interface"));
  EXPECT_TRUE(root.contains("system"));
  EXPECT_TRUE(root.contains("user"));
  EXPECT_TRUE(root.contains("plugins"));
  EXPECT_EQ(root.at("cpu").as_object().at("count").as_array().at(0).as_array().at(0).as_int64(), 0);
}

}  // namespace
