// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The node half of the NCPA bridge: the JSON shape a walk produces, how a path
// resolves, and how a RunnableParentNode folds its children into one line. The
// Nagios XI NCPA wizard reads the JSON shape, so these are a wire contract.

#include "ncpa_tree.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <memory>

namespace json = boost::json;

namespace {

using ncpa::check_result;
using ncpa::leaf_node;
using ncpa::node_ptr;
using ncpa::parent_node;
using ncpa::render_options;
using ncpa::runnable_parent_node;
using ncpa::value;
using ncpa::values_type;
using ncpa::walk_context;

std::shared_ptr<leaf_node> leaf(const std::string &name, const values_type &vals, const std::string &unit, const bool scalar = true) {
  auto out = std::make_shared<leaf_node>(name, vals, unit);
  out->set_scalar(scalar);
  return out;
}

std::string walk(const node_ptr &node, const walk_context &ctx = walk_context()) { return json::serialize(node->walk(ctx)); }

// --------------------------------------------------------------------------
// path resolution and mount-point encoding
// --------------------------------------------------------------------------

TEST(ncpa_path, splits_on_slashes_and_drops_empty_segments) {
  const std::vector<std::string> parts = ncpa::split_accessor("/cpu//percent/");
  ASSERT_EQ(parts.size(), 2u);
  EXPECT_EQ(parts[0], "cpu");
  EXPECT_EQ(parts[1], "percent");
}

TEST(ncpa_path, a_quoted_segment_keeps_its_slashes) {
  // NCPA's getter() splits on "/" except inside quotes, so a plugin argument
  // naming a path survives as one segment.
  const std::vector<std::string> parts = ncpa::split_accessor("plugins/check_files/\"path=/var/log\"");
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[2], "\"path=/var/log\"");
}

TEST(ncpa_mountpoint, slashes_and_backslashes_collapse_to_one_pipe) {
  EXPECT_EQ(ncpa::encode_mountpoint("/"), "|");
  EXPECT_EQ(ncpa::encode_mountpoint("/var/log"), "|var|log");
  EXPECT_EQ(ncpa::encode_mountpoint("C:\\"), "C:|");
  // A run of separators is one pipe, not one per character.
  EXPECT_EQ(ncpa::encode_mountpoint("\\\\server\\share"), "|server|share");
}

TEST(ncpa_resolve, a_missing_child_resolves_to_the_error_node) {
  const auto root = std::make_shared<parent_node>("root");
  root->add(std::make_shared<parent_node>("cpu"));

  const node_ptr node = ncpa::resolve(root, {"cpu", "nope"}, "/api/cpu/nope");
  const json::value body = json::parse(walk(node));
  const json::object &error = body.at("error").as_object();
  EXPECT_EQ(error.at("code").as_int64(), 100);
  EXPECT_EQ(error.at("node").as_string(), "nope");
  EXPECT_EQ(error.at("path").as_string(), "/api/cpu/nope");

  const check_result result = node->run_check(walk_context(), render_options());
  EXPECT_EQ(result.returncode, 3);
  EXPECT_EQ(result.stdout_text, "UNKNOWN: The node (nope) requested does not exist.");
}

TEST(ncpa_resolve, an_empty_path_resolves_to_the_root) {
  const auto root = std::make_shared<parent_node>("root");
  EXPECT_EQ(ncpa::resolve(root, {}, "/api"), root);
}

// --------------------------------------------------------------------------
// walk JSON
// --------------------------------------------------------------------------

TEST(ncpa_walk, a_scalar_leaf_is_value_then_unit) {
  EXPECT_EQ(walk(leaf("total", values_type{value::from_int(8589934592LL)}, "B")), R"({"total":[8589934592,"B"]})");
}

TEST(ncpa_walk, a_per_core_leaf_wraps_its_values_in_an_array) {
  const values_type cores{value::from_double(1.5), value::from_double(2.5)};
  EXPECT_EQ(walk(leaf("percent", cores, "%", false)), R"({"percent":[[1.5E0,2.5E0],"%"]})");
}

TEST(ncpa_walk, a_single_value_that_is_natively_a_list_stays_a_list) {
  // cpu/count answers [[8], "cores"], not [8, "cores"] - the wizard reads the
  // shape, so a one-core machine must not change it.
  EXPECT_EQ(walk(leaf("count", values_type{value::from_int(8)}, "cores", false)), R"({"count":[[8],"cores"]})");
}

TEST(ncpa_walk, a_unitless_leaf_has_no_unit_slot) {
  EXPECT_EQ(walk(leaf("device_name", values_type{value::from_string("/dev/sda1")}, "", false)), R"({"device_name":["/dev/sda1"]})");
}

TEST(ncpa_walk, a_parent_nests_its_children_under_its_own_name) {
  const auto memory = std::make_shared<parent_node>("memory");
  const auto swap = std::make_shared<parent_node>("swap");
  swap->add(leaf("total", values_type{value::from_int(1024)}, "B"));
  memory->add(swap);
  EXPECT_EQ(walk(memory), R"({"memory":{"swap":{"total":[1024,"B"]}}})");
}

TEST(ncpa_walk, the_unit_override_replaces_the_nodes_own) {
  walk_context ctx;
  ctx.opts.unit = "blocks";
  EXPECT_EQ(walk(leaf("total", values_type{value::from_int(1024)}, "B"), ctx), R"({"total":[1024,"blocks"]})");
}

TEST(ncpa_walk, units_scale_a_byte_leaf_in_the_walk_too) {
  walk_context ctx;
  ctx.opts.units = "Ki";
  EXPECT_EQ(walk(leaf("total", values_type{value::from_int(2048)}, "B"), ctx), R"({"total":[2E0,"KiB"]})");
}

TEST(ncpa_walk, aggregate_collapses_a_list_but_keeps_it_a_list) {
  walk_context ctx;
  ctx.opts.aggregate = "max";
  const values_type cores{value::from_double(1), value::from_double(9)};
  EXPECT_EQ(walk(leaf("percent", cores, "%", false), ctx), R"({"percent":[[9E0],"%"]})");
}

TEST(ncpa_walk, delta_on_a_rate_leaf_only_renames_the_unit) {
  // Everything the NSClient++ collector publishes for an interface or a disk
  // is already per-second, so differencing two samples of it would report ~0.
  auto node = leaf("bytes_sent", values_type{value::from_double(2048)}, "B");
  node->set_rate(true);
  walk_context ctx;
  ctx.opts.delta = true;
  EXPECT_EQ(walk(node, ctx), R"({"bytes_sent":[2.048E3,"B/s"]})");
}

TEST(ncpa_walk, delta_without_a_store_reports_zero_rather_than_a_raw_counter) {
  walk_context ctx;
  ctx.opts.delta = true;
  EXPECT_EQ(walk(leaf("read_bytes", values_type{value::from_int(50000)}, "B"), ctx), R"({"read_bytes":[0,"B/s"]})");
}

TEST(ncpa_walk, a_failing_child_does_not_take_the_subtree_down) {
  // A leaf that throws is reported in its own key; a dashboard polling /api
  // still gets everything else.
  class throwing_node : public ncpa::node {
   public:
    throwing_node() : node("bad") {}
    json::value walk_body(const walk_context &) const override { throw std::runtime_error("nope"); }
  };
  const auto parent = std::make_shared<parent_node>("cpu");
  parent->add(std::make_shared<throwing_node>());
  parent->add(leaf("count", values_type{value::from_int(2)}, "cores", false));
  EXPECT_EQ(walk(parent), R"({"cpu":{"bad":"Error retrieving child: nope","count":[[2],"cores"]}})");
}

// --------------------------------------------------------------------------
// check mode
// --------------------------------------------------------------------------

TEST(ncpa_check_node, a_plain_parent_is_unknown) {
  const auto cpu = std::make_shared<parent_node>("cpu");
  const check_result result = cpu->run_check(walk_context(), render_options());
  EXPECT_EQ(result.returncode, 3);
  EXPECT_EQ(result.stdout_text, "UNKNOWN: Unable to run check on node without check method. Requested 'cpu' node.");
}

// memory/virtual as NCPA builds it: percentage primary, custom output, and the
// one node where the primary's own perfdata joins the line.
std::shared_ptr<runnable_parent_node> memory_virtual() {
  auto out = std::make_shared<runnable_parent_node>("virtual", "percent", "%");
  out->set_custom_output("Memory usage was");
  out->set_add_primary_to_perfdata(true);
  out->add(leaf("available", values_type{value::from_int(4294967296LL)}, "B"));
  out->add(leaf("total", values_type{value::from_int(8589934592LL)}, "B"));
  out->add(leaf("percent", values_type{value::from_double(50)}, "%"));
  out->add(leaf("free", values_type{value::from_int(4294967296LL)}, "B"));
  out->add(leaf("used", values_type{value::from_int(4294967296LL)}, "B"));
  return out;
}

TEST(ncpa_check_node, a_runnable_parent_folds_its_children_into_one_line) {
  walk_context ctx;
  ctx.opts.warning = "80";
  ctx.opts.critical = "90";

  const check_result result = memory_virtual()->run_check(ctx, render_options());
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text,
            "OK: Memory usage was 50.00 % (Available: 4294967296 B, Total: 8589934592 B, Free: 4294967296 B, Used: 4294967296 B) | "
            "'available'=4294967296B;;; 'total'=8589934592B;;; 'percent'=50.00%;80;90; 'free'=4294967296B;;; 'used'=4294967296B;;;");
}

TEST(ncpa_check_node, the_primary_value_decides_the_return_code) {
  walk_context ctx;
  ctx.opts.warning = "40";
  ctx.opts.critical = "90";
  const check_result result = memory_virtual()->run_check(ctx, render_options());
  EXPECT_EQ(result.returncode, 1);
  EXPECT_EQ(result.stdout_text.compare(0, 8, "WARNING:"), 0) << result.stdout_text;
}

TEST(ncpa_check_node, units_reach_every_child_of_the_parent) {
  walk_context ctx;
  ctx.opts.units = "Gi";
  const check_result result = memory_virtual()->run_check(ctx, render_options());
  EXPECT_NE(result.stdout_text.find("Total: 8.00 GiB"), std::string::npos) << result.stdout_text;
  // The percentage is not a byte value, so it keeps its own unit.
  EXPECT_NE(result.stdout_text.find("Memory usage was 50.00 %"), std::string::npos) << result.stdout_text;
}

TEST(ncpa_check_node, include_limits_which_children_take_part_in_the_check) {
  auto swap = std::make_shared<runnable_parent_node>("swap", "percent", "%");
  swap->set_custom_output("Swap usage was");
  swap->set_include({"total", "used", "free", "percent"});
  swap->add(leaf("used", values_type{value::from_int(100)}, "B"));
  swap->add(leaf("swapped_in", values_type{value::from_int(7)}, "B"));
  swap->add(leaf("total", values_type{value::from_int(1000)}, "B"));
  swap->add(leaf("percent", values_type{value::from_double(10)}, "%"));
  swap->add(leaf("free", values_type{value::from_int(900)}, "B"));

  const check_result result = swap->run_check(walk_context(), render_options());
  EXPECT_EQ(result.stdout_text.find("Swapped_in"), std::string::npos) << result.stdout_text;
  // And, as in NCPA, a percentage primary that did not opt in has its own
  // perfdata dropped: the line carries the three secondary labels only.
  EXPECT_EQ(result.stdout_text, "OK: Swap usage was 10.00 % (Used: 100 B, Total: 1000 B, Free: 900 B) | 'used'=100B;;; 'total'=1000B;;; 'free'=900B;;;");
}

TEST(ncpa_check_node, a_non_percentage_primary_keeps_its_perfdata_first) {
  auto nic = std::make_shared<runnable_parent_node>("eth0", "bytes_sent", "");
  nic->add(leaf("bytes_recv", values_type{value::from_double(100)}, "B"));
  nic->add(leaf("bytes_sent", values_type{value::from_double(200)}, "B"));

  walk_context ctx;
  ctx.opts.warning = "1000";
  const check_result result = nic->run_check(ctx, render_options());
  EXPECT_EQ(result.stdout_text, "OK: Bytes_sent was 200.00 B (Bytes_recv: 100.00 B) | 'bytes_sent'=200.00B;1000;; 'bytes_recv'=100.00B;1000;;");
}

TEST(ncpa_check_node, a_runnable_parent_without_its_primary_child_is_unknown) {
  auto broken = std::make_shared<runnable_parent_node>("virtual", "percent", "%");
  broken->add(leaf("total", values_type{value::from_int(1)}, "B"));
  EXPECT_EQ(broken->run_check(walk_context(), render_options()).returncode, 3);
}

// --------------------------------------------------------------------------
// the plugins node
// --------------------------------------------------------------------------

class fake_dispatcher : public ncpa::query_dispatcher {
 public:
  check_result run_query(const std::string &name, const std::vector<std::string> &args) override {
    last_name = name;
    last_args = args;
    check_result out;
    out.returncode = 1;
    out.stdout_text = "WARNING: load is high|'load'=9";
    return out;
  }
  std::vector<std::string> list_queries() override { return {"check_cpu", "check_uptime"}; }
  bool is_exposed(const std::string &name) override { return name != "check_secret"; }
  bool allow_arguments() override { return allow_args; }
  std::vector<ncpa::service_entry> list_services() override { return services; }
  std::vector<ncpa::process_entry> list_processes() override { return processes; }

  std::string last_name;
  std::vector<std::string> last_args;
  bool allow_args = true;
  std::vector<ncpa::service_entry> services;
  std::vector<ncpa::process_entry> processes;
};

ncpa::service_entry service(const std::string &name, const std::string &status) {
  ncpa::service_entry out;
  out.name = name;
  out.status = status;
  return out;
}

ncpa::process_entry process(const std::string &name, const std::string &username, const long long pid) {
  ncpa::process_entry out;
  out.name = name;
  out.exe = "/usr/bin/" + name;
  out.username = username;
  out.cmd = "/usr/bin/" + name + " --serve";
  out.pid = pid;
  return out;
}

walk_context with_extras(const std::vector<std::pair<std::string, std::string> > &extras) {
  walk_context ctx;
  for (const auto &entry : extras) ctx.extras.add(entry.first, entry.second);
  return ctx;
}

TEST(ncpa_plugins, the_bare_node_lists_the_exposed_queries) {
  fake_dispatcher dispatcher;
  const auto plugins = std::make_shared<ncpa::plugins_node>("plugins", &dispatcher);
  EXPECT_EQ(walk(plugins), R"({"plugins":["check_cpu","check_uptime"]})");
}

TEST(ncpa_plugins, a_plugin_answers_returncode_and_stdout_verbatim) {
  fake_dispatcher dispatcher;
  const auto plugins = std::make_shared<ncpa::plugins_node>("plugins", &dispatcher);
  const node_ptr node = ncpa::resolve(plugins, {"check_cpu"}, "/api/plugins/check_cpu");

  const check_result result = node->run_check(walk_context(), render_options());
  EXPECT_EQ(result.returncode, 1);
  // The one node whose output is not reformatted.
  EXPECT_EQ(result.stdout_text, "WARNING: load is high|'load'=9");
  EXPECT_EQ(dispatcher.last_name, "check_cpu");
  EXPECT_TRUE(dispatcher.last_args.empty());
}

TEST(ncpa_plugins, every_path_segment_after_the_name_is_one_argument) {
  fake_dispatcher dispatcher;
  const auto plugins = std::make_shared<ncpa::plugins_node>("plugins", &dispatcher);
  const node_ptr node = ncpa::resolve(plugins, {"check_cpu", "warning=load>80", "time=5m"}, "/api/plugins/check_cpu/...");
  node->run_check(walk_context(), render_options());

  ASSERT_EQ(dispatcher.last_args.size(), 2u);
  EXPECT_EQ(dispatcher.last_args[0], "warning=load>80");
  EXPECT_EQ(dispatcher.last_args[1], "time=5m");
}

TEST(ncpa_plugins, arguments_are_refused_when_allow_arguments_is_off) {
  fake_dispatcher dispatcher;
  dispatcher.allow_args = false;
  const auto plugins = std::make_shared<ncpa::plugins_node>("plugins", &dispatcher);

  const node_ptr with_args = ncpa::resolve(plugins, {"check_cpu", "warning=load>80"}, "/api/plugins/check_cpu/warning=load>80");
  const check_result refused = with_args->run_check(walk_context(), render_options());
  EXPECT_EQ(refused.returncode, 3);
  EXPECT_NE(refused.stdout_text.find("Arguments are not allowed"), std::string::npos);
  EXPECT_TRUE(dispatcher.last_name.empty());

  // The command itself still runs - this gates arguments, not the plugin.
  const node_ptr bare = ncpa::resolve(plugins, {"check_cpu"}, "/api/plugins/check_cpu");
  EXPECT_EQ(bare->run_check(walk_context(), render_options()).returncode, 1);
}

TEST(ncpa_plugins, a_query_outside_the_allow_list_is_a_missing_plugin) {
  fake_dispatcher dispatcher;
  const auto plugins = std::make_shared<ncpa::plugins_node>("plugins", &dispatcher);
  const node_ptr node = ncpa::resolve(plugins, {"check_secret"}, "/api/plugins/check_secret");

  const check_result result = node->run_check(walk_context(), render_options());
  EXPECT_EQ(result.returncode, 3);
  EXPECT_EQ(result.stdout_text, "UNKNOWN: The plugin (check_secret) requested does not exist.");
  // And the walk reports it as a plugin rather than a node, which is the
  // distinction NCPA's error body makes.
  const json::value body = json::parse(walk(node));
  EXPECT_EQ(body.at("error").as_object().at("plugin").as_string(), "check_secret");
}

TEST(ncpa_plugins, a_walk_of_a_plugin_runs_it) {
  // There is nothing to list about a plugin, so NCPA answers a walk with the
  // check result - which is what makes `-M plugins/x` work without `check=1`.
  fake_dispatcher dispatcher;
  const auto plugins = std::make_shared<ncpa::plugins_node>("plugins", &dispatcher);
  const node_ptr node = ncpa::resolve(plugins, {"check_uptime"}, "/api/plugins/check_uptime");
  const json::value body = json::parse(json::serialize(node->walk_body(walk_context())));
  EXPECT_EQ(body.at("returncode").as_int64(), 1);
  EXPECT_EQ(body.at("stdout").as_string(), "WARNING: load is high|'load'=9");
}

}  // namespace

namespace {

// --------------------------------------------------------------------------
// match modes
// --------------------------------------------------------------------------

TEST(ncpa_match, exact_is_case_insensitive_and_whole_string) {
  EXPECT_TRUE(ncpa::ncpa_matches("SSHD", "sshd", ""));
  EXPECT_FALSE(ncpa::ncpa_matches("ssh", "sshd", ""));
}

TEST(ncpa_match, search_is_a_substring) {
  EXPECT_TRUE(ncpa::ncpa_matches("ssh", "openssh-server", "search"));
  EXPECT_FALSE(ncpa::ncpa_matches("nginx", "openssh-server", "search"));
}

TEST(ncpa_match, regex_is_unanchored) {
  EXPECT_TRUE(ncpa::ncpa_matches("ssh.*", "sshd", "regex"));
  EXPECT_TRUE(ncpa::ncpa_matches("h.$", "sshd", "regex"));
  // A pattern that does not compile matches nothing rather than throwing out
  // of the whole check.
  EXPECT_FALSE(ncpa::ncpa_matches("[unterminated", "sshd", "regex"));
}

// --------------------------------------------------------------------------
// services
// --------------------------------------------------------------------------

std::shared_ptr<ncpa::services_node> three_services(fake_dispatcher &dispatcher) {
  dispatcher.services = {service("sshd", "running"), service("cron", "running"), service("nginx", "stopped")};
  return std::make_shared<ncpa::services_node>("services", &dispatcher);
}

TEST(ncpa_services, a_walk_is_a_flat_name_to_status_map) {
  fake_dispatcher dispatcher;
  // Insertion order, which is the order the agent listed them in.
  EXPECT_EQ(walk(three_services(dispatcher)), R"({"services":{"sshd":"running","cron":"running","nginx":"stopped"}})");
}

TEST(ncpa_services, a_walk_filters_by_name) {
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  EXPECT_EQ(walk(node, with_extras({{"service", "sshd"}})), R"({"services":{"sshd":"running"}})");
}

TEST(ncpa_services, a_walk_filters_by_status) {
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  EXPECT_EQ(walk(node, with_extras({{"status", "stopped"}})), R"({"services":{"nginx":"stopped"}})");
}

TEST(ncpa_services, a_check_defaults_to_expecting_running) {
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  const check_result result = node->run_check(with_extras({{"service", "sshd"}}), render_options());
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text, "OK: sshd is running");
}

TEST(ncpa_services, a_service_in_the_wrong_state_is_critical_and_reported_first) {
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  const check_result result = node->run_check(with_extras({{"service", "sshd"}, {"service", "nginx"}}), render_options());
  EXPECT_EQ(result.returncode, 2);
  EXPECT_EQ(result.stdout_text, "CRITICAL: nginx is stopped (should be running), sshd is running");
}

TEST(ncpa_services, the_expected_state_does_not_filter_the_listing) {
  // `status=running` in check mode means "these should be running", not "only
  // look at the ones that are" - filtering on it would hide the very service
  // the check exists to catch.
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  const check_result result = node->run_check(with_extras({{"service", "nginx"}, {"status", "running"}}), render_options());
  EXPECT_EQ(result.returncode, 2);
  EXPECT_EQ(result.stdout_text, "CRITICAL: nginx is stopped (should be running)");
}

TEST(ncpa_services, a_name_that_matches_nothing_is_unknown) {
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  const check_result result = node->run_check(with_extras({{"service", "sshd"}, {"service", "absent"}}), render_options());
  EXPECT_EQ(result.returncode, 3);
  EXPECT_NE(result.stdout_text.find("absent could not be found"), std::string::npos) << result.stdout_text;
}

TEST(ncpa_services, no_services_at_all_is_unknown) {
  fake_dispatcher dispatcher;
  const auto node = std::make_shared<ncpa::services_node>("services", &dispatcher);
  const check_result result = node->run_check(with_extras({{"service", "sshd"}}), render_options());
  EXPECT_EQ(result.returncode, 3);
  EXPECT_EQ(result.stdout_text, "UNKNOWN: No services found for service names: sshd");
}

TEST(ncpa_services, a_search_match_never_reports_a_missing_service) {
  fake_dispatcher dispatcher;
  const auto node = three_services(dispatcher);
  const check_result result = node->run_check(with_extras({{"service", "ssh"}, {"match", "search"}}), render_options());
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text, "OK: sshd is running");
}

// --------------------------------------------------------------------------
// processes
// --------------------------------------------------------------------------

std::shared_ptr<ncpa::processes_node> three_processes(fake_dispatcher &dispatcher) {
  dispatcher.processes = {process("nginx", "www-data", 10), process("nginx", "root", 11), process("sshd", "root", 20)};
  return std::make_shared<ncpa::processes_node>("processes", &dispatcher);
}

TEST(ncpa_processes, a_check_counts_the_matches_and_names_what_it_counted) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  const check_result result = node->run_check(with_extras({{"name", "nginx"}}), render_options());
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text.compare(0, 49, "OK: Process count for processes named nginx was 2"), 0) << result.stdout_text;
  // The fixed label keeps a graph on the same series whatever the filter names.
  EXPECT_NE(result.stdout_text.find("'process_count'=2;;;"), std::string::npos) << result.stdout_text;
}

TEST(ncpa_processes, the_matched_processes_are_listed_before_the_perfdata) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  const check_result result = node->run_check(with_extras({{"name", "sshd"}}), render_options());
  const std::size_t table = result.stdout_text.find("Processes Matched");
  const std::size_t perf = result.stdout_text.rfind('|');
  ASSERT_NE(table, std::string::npos) << result.stdout_text;
  // Every Nagios frontend reads the text as "MESSAGE|PERFDATA", so the table
  // has to come first or it is swallowed into the graph.
  EXPECT_LT(table, perf) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("20: sshd: root: /usr/bin/sshd"), std::string::npos) << result.stdout_text;
}

TEST(ncpa_processes, thresholds_apply_to_the_count) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  walk_context ctx = with_extras({{"name", "nginx"}});
  ctx.opts.critical = "1";
  const check_result result = node->run_check(ctx, render_options());
  EXPECT_EQ(result.returncode, 2);
}

TEST(ncpa_processes, combiner_and_requires_every_term_to_match) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  const check_result result = node->run_check(with_extras({{"name", "nginx"}, {"username", "root"}}), render_options());
  EXPECT_NE(result.stdout_text.find(" was 1 "), std::string::npos) << result.stdout_text;
}

TEST(ncpa_processes, combiner_or_requires_one_term_to_match) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  const check_result result = node->run_check(with_extras({{"name", "nginx"}, {"username", "root"}, {"combiner", "or"}}), render_options());
  EXPECT_NE(result.stdout_text.find(" was 3 "), std::string::npos) << result.stdout_text;
}

TEST(ncpa_processes, no_filter_counts_everything) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  const check_result result = node->run_check(walk_context(), render_options());
  EXPECT_EQ(result.stdout_text.compare(0, 24, "OK: Process count was 3 "), 0) << result.stdout_text;
}

TEST(ncpa_processes, a_walk_lists_the_matches_with_one_element_lists) {
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  const json::value body = json::parse(walk(node, with_extras({{"name", "sshd"}})));
  const json::array &processes = body.at("processes").as_array();
  ASSERT_EQ(processes.size(), 1u);
  const json::object &entry = processes.at(0).as_object();
  EXPECT_EQ(entry.at("name").as_array().at(0).as_string(), "sshd");
  EXPECT_EQ(entry.at("pid").as_array().at(0).as_int64(), 20);
}

TEST(ncpa_processes, units_do_not_rescale_the_count) {
  // `units` is sent on every request; applying it to a process count would
  // divide it by a billion.
  fake_dispatcher dispatcher;
  const auto node = three_processes(dispatcher);
  walk_context ctx = with_extras({{"name", "nginx"}});
  ctx.opts.units = "G";
  const check_result result = node->run_check(ctx, render_options());
  EXPECT_NE(result.stdout_text.find(" was 2 "), std::string::npos) << result.stdout_text;
}

}  // namespace
