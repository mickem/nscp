// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for syslog_handler.hpp - the settings side of the syslog client.
//
// syslog_client_test.cpp covers what goes on the wire; this covers how a
// target object comes to hold its options in the first place: the defaults a
// bare `[/settings/syslog/client/targets/default]` section gets, what read()
// picks up from the settings store, and how the per-command options
// (--severity, --facility, the templates) land in the destination container
// the submit path reads. A wrong default here silently files every check
// under the wrong facility.
//
// The plugin singleton is defined once for the binary in
// syslog_client_test.cpp.

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>

// syslog_handler.hpp expects its includer to provide the `po` alias
// (SyslogClient.cpp does the same).
namespace po = boost::program_options;

#include "syslog_handler.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

// A minimal in-memory settings store, mirroring the one used by the settings
// helper's own suite.
class fake_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> values;
  std::vector<std::string> registered_keys;

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string key, std::string, std::string, std::string, std::string, bool, bool, bool) override {
    registered_keys.push_back(key);
  }
  void register_subkey(std::string, std::string, std::string, bool, bool) override {}
  void register_tpl(std::string, std::string, std::string, std::string, std::string) override {}
  std::string get_string(std::string path, std::string key, std::string def) override {
    const auto pit = values.find(path);
    if (pit == values.end()) return def;
    const auto kit = pit->second.find(key);
    return kit == pit->second.end() ? def : kit->second;
  }
  void set_string(std::string path, std::string key, std::string value) override { values[path][key] = value; }
  string_list get_sections(const std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  std::string expand_path(std::string key) override { return key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}
  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}

  bool has_registered(const std::string &key) const {
    return std::find(registered_keys.begin(), registered_keys.end(), key) != registered_keys.end();
  }
};

// Run the reader's command-line options over the given tokens and hand back
// the destination container they populate - the same flow the client uses
// for `nscp syslog --severity ...`.
client::destination_container parse_options(const std::vector<std::string> &tokens) {
  syslog_handler::options_reader_impl reader;
  po::options_description desc;
  client::destination_container source;
  client::destination_container data;
  reader.process(desc, source, data);

  po::variables_map vm;
  po::store(po::command_line_parser(tokens).options(desc).run(), vm);
  po::notify(vm);
  return data;
}

}  // namespace

// ---------------------------------------------------------------------------
// syslog_target_object - the defaults a fresh target carries.
// ---------------------------------------------------------------------------

TEST(SyslogTargetObject, AFreshTargetCarriesTheDocumentedDefaults) {
  syslog_handler::syslog_target_object target("default", "/settings/syslog/client/targets");

  EXPECT_EQ(target.get_property_int("timeout", 0), 30);
  EXPECT_EQ(target.get_property_string("path"), "/nsclient++");
  EXPECT_EQ(target.get_property_string("severity"), "error");
  EXPECT_EQ(target.get_property_string("facility"), "kernel");
  EXPECT_EQ(target.get_property_string("tag template"), "NSCA");
  EXPECT_EQ(target.get_property_string("message template"), "%message%");
  // The per-status severities are what map OK/WARNING/CRITICAL/UNKNOWN onto
  // syslog priorities; each has to have a sane default.
  EXPECT_EQ(target.get_property_string("ok severity"), "informational");
  EXPECT_EQ(target.get_property_string("warning severity"), "warning");
  EXPECT_EQ(target.get_property_string("critical severity"), "critical");
  EXPECT_EQ(target.get_property_string("unknown severity"), "emergency");
}

TEST(SyslogTargetObject, ReadPicksUpTheAddressFromTheSettings) {
  auto settings = std::make_shared<fake_settings>();
  auto target = std::make_shared<syslog_handler::syslog_target_object>("default", "/settings/syslog/client/targets");
  settings->values[target->get_path()]["address"] = "syslog.example.com";

  target->read(settings, false, false);

  EXPECT_EQ(target->get_property_string("address"), "syslog.example.com");
}

TEST(SyslogTargetObject, ConfiguredSeverityAndFacilityAreAppliedByRead) {
  // A `severity =` / `facility =` on the target section must override the
  // constructor defaults, and the syslog keys must be registered so they show
  // up in the generated documentation.
  auto settings = std::make_shared<fake_settings>();
  auto target = std::make_shared<syslog_handler::syslog_target_object>("default", "/settings/syslog/client/targets");
  settings->values[target->get_path()]["severity"] = "warning";
  settings->values[target->get_path()]["facility"] = "daemon";

  target->read(settings, false, false);

  EXPECT_EQ(target->get_property_string("severity"), "warning");
  EXPECT_EQ(target->get_property_string("facility"), "daemon");
  EXPECT_TRUE(settings->has_registered("address")) << "the parent keys register";
  EXPECT_TRUE(settings->has_registered("severity")) << "the syslog keys register too";
}

TEST(SyslogTargetObject, ReadKeepsTheDefaultsWhenNothingIsConfigured) {
  auto settings = std::make_shared<fake_settings>();
  auto target = std::make_shared<syslog_handler::syslog_target_object>("default", "/settings/syslog/client/targets");

  target->read(settings, false, false);

  EXPECT_EQ(target->get_property_string("severity"), "error");
  EXPECT_EQ(target->get_property_string("facility"), "kernel");
  EXPECT_EQ(target->get_property_string("ok severity"), "informational");
}

// ---------------------------------------------------------------------------
// options_reader_impl - factory and command-line options.
// ---------------------------------------------------------------------------

TEST(SyslogOptionsReader, CreateBuildsATargetWithTheSyslogDefaults) {
  syslog_handler::options_reader_impl reader;

  const nscapi::settings_objects::object_instance target = reader.create("default", "/settings/syslog/client/targets");

  ASSERT_TRUE(target);
  EXPECT_EQ(target->get_alias(), "default");
  EXPECT_EQ(target->get_property_string("severity"), "error");
  EXPECT_EQ(target->get_property_string("facility"), "kernel");
}

TEST(SyslogOptionsReader, CloneInheritsTheParentsOptions) {
  // Cloning is how `[targets/mine]` inherits from `[targets/default]`; the
  // parent's already-resolved options have to carry over.
  syslog_handler::options_reader_impl reader;
  const nscapi::settings_objects::object_instance parent = reader.create("default", "/settings/syslog/client/targets");
  parent->set_property_string("facility", "local0");

  const nscapi::settings_objects::object_instance child = reader.clone(parent, "mine", "/settings/syslog/client/targets");

  ASSERT_TRUE(child);
  EXPECT_EQ(child->get_alias(), "mine");
  EXPECT_EQ(child->get_property_string("facility"), "local0");
  EXPECT_EQ(child->get_property_string("severity"), "error");
}

TEST(SyslogOptionsReader, CommandLineOptionsLandInTheDestinationContainer) {
  client::destination_container data = parse_options({"--path", "/nsclient++", "--severity", "warning", "--facility", "daemon", "--ok-severity", "debug",
                                                      "--warning-severity", "notice", "--critical-severity", "alert", "--unknown-severity", "emergency"});

  // These are the exact keys the submit path's connection_data looks up - the
  // per-status ones carry a space, not an underscore.
  EXPECT_EQ(data.get_string_data("path"), "/nsclient++");
  EXPECT_EQ(data.get_string_data("severity"), "warning");
  EXPECT_EQ(data.get_string_data("facility"), "daemon");
  EXPECT_EQ(data.get_string_data("ok severity"), "debug");
  EXPECT_EQ(data.get_string_data("warning severity"), "notice");
  EXPECT_EQ(data.get_string_data("critical severity"), "alert");
  EXPECT_EQ(data.get_string_data("unknown severity"), "emergency");
}

TEST(SyslogOptionsReader, TheTemplateOptionsKeepTheirSpacedNames) {
  // The option names contain a literal space ("tag template"); the client
  // passes them as single --"tag template" tokens and the data keys are the
  // spaced ones connection_data reads.
  client::destination_container data = parse_options({"--tag template", "my-tag", "--message template", "msg %message%"});

  EXPECT_EQ(data.get_string_data("tag template"), "my-tag");
  EXPECT_EQ(data.get_string_data("message template"), "msg %message%");
}

TEST(SyslogOptionsReader, ShortOptionsForSeverityAndFacilityWork) {
  client::destination_container data = parse_options({"-s", "critical", "-f", "local7"});

  EXPECT_EQ(data.get_string_data("severity"), "critical");
  EXPECT_EQ(data.get_string_data("facility"), "local7");
}
