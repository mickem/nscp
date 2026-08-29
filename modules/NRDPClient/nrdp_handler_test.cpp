// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for nrdp_handler.hpp - the settings side of the NRDP client.
//
// nrdp_test.cpp covers the XML payload and nrdp_client_test.cpp what gets
// POSTed; this covers how the target object is populated in the first place:
// what read() picks up from a `[/settings/NRDP/client/targets/...]` section
// (the token aliases, the TLS knobs, the proxy pair), that the secrets are
// registered as sensitive keys, and how the per-command options (--token,
// --tls-version, --verify, ...) land in the destination container the submit
// path reads.
//
// The plugin singleton is defined once for the binary in
// nrdp_client_test.cpp.

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>

// nrdp_handler.hpp expects its includer to provide the `po` alias
// (NRDPClient.cpp does the same).
namespace po = boost::program_options;

#include "nrdp_handler.hpp"

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
  struct registered_key {
    std::string key;
    bool sensitive;
  };
  std::map<std::string, std::map<std::string, std::string>> values;
  std::vector<registered_key> registered_keys;

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string key, std::string, std::string, std::string, std::string, bool, bool, bool sensitive) override {
    registered_keys.push_back({key, sensitive});
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
    return std::find_if(registered_keys.begin(), registered_keys.end(), [&key](const registered_key &k) { return k.key == key; }) != registered_keys.end();
  }
  bool is_sensitive(const std::string &key) const {
    const auto it = std::find_if(registered_keys.begin(), registered_keys.end(), [&key](const registered_key &k) { return k.key == key; });
    return it != registered_keys.end() && it->sensitive;
  }
};

// A target that has been read() from the given section values, the way the
// client machinery loads `[/settings/NRDP/client/targets/default]`.
std::shared_ptr<nrdp_handler::nrdp_target_object> target_from_settings(const std::map<std::string, std::string> &section) {
  auto settings = std::make_shared<fake_settings>();
  auto target = std::make_shared<nrdp_handler::nrdp_target_object>("default", "/settings/NRDP/client/targets");
  settings->values[target->get_path()] = section;
  target->read(settings, false, false);
  return target;
}

// Run the reader's command-line options over the given tokens and hand back
// the destination container they populate.
client::destination_container parse_options(const std::vector<std::string> &tokens) {
  nrdp_handler::options_reader_impl reader;
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
// nrdp_target_object - construction and read().
// ---------------------------------------------------------------------------

TEST(NrdpTargetObject, AFreshTargetDefaultsTheTimeout) {
  nrdp_handler::nrdp_target_object target("default", "/settings/NRDP/client/targets");

  EXPECT_EQ(target.get_property_int("timeout", 0), 30);
}

TEST(NrdpTargetObject, ReadAppliesTheConfiguredSettings) {
  const auto target = target_from_settings({{"address", "https://nagios.example.com/nrdp/"},
                                            {"token", "s3cret"},
                                            {"tls version", "1.2"},
                                            {"verify mode", "none"},
                                            {"ca", "/etc/ca.pem"},
                                            {"proxy", "http://proxy:3128/"},
                                            {"no proxy", "localhost,.internal"}});

  EXPECT_EQ(target->get_property_string("address"), "https://nagios.example.com/nrdp/");
  EXPECT_EQ(target->get_property_string("token"), "s3cret");
  EXPECT_EQ(target->get_property_string("tls version"), "1.2");
  EXPECT_EQ(target->get_property_string("verify mode"), "none");
  EXPECT_EQ(target->get_property_string("ca"), "/etc/ca.pem");
  EXPECT_EQ(target->get_property_string("proxy"), "http://proxy:3128/");
  EXPECT_EQ(target->get_property_string("no proxy"), "localhost,.internal");
}

TEST(NrdpTargetObject, ReadDefaultsTheTlsKnobsWhenTheSectionIsEmpty) {
  // These defaults are security-relevant: 1.3 keeps the client off legacy TLS
  // and "peer" keeps certificate verification on unless the operator opts
  // out. "${ca-path}" is expanded later by the real settings core.
  const auto target = target_from_settings({});

  EXPECT_EQ(target->get_property_string("tls version"), "1.3");
  EXPECT_EQ(target->get_property_string("verify mode"), "peer");
  EXPECT_EQ(target->get_property_string("ca"), "${ca-path}");
}

TEST(NrdpTargetObject, KeyAndPasswordAreAliasesForTheToken) {
  // NRDP servers call it a token; older configurations used `key` or
  // `password`. All three must land in the same property.
  EXPECT_EQ(target_from_settings({{"key", "via-key"}})->get_property_string("token"), "via-key");
  EXPECT_EQ(target_from_settings({{"password", "via-password"}})->get_property_string("token"), "via-password");
}

TEST(NrdpTargetObject, TheSecretKeysAreRegisteredAsSensitive) {
  // Sensitive keys are masked when the configuration is rendered; the token
  // is a shared secret and must not show up in a settings dump.
  auto settings = std::make_shared<fake_settings>();
  auto target = std::make_shared<nrdp_handler::nrdp_target_object>("default", "/settings/NRDP/client/targets");

  target->read(settings, false, false);

  EXPECT_TRUE(settings->has_registered("token"));
  EXPECT_TRUE(settings->is_sensitive("token"));
  EXPECT_TRUE(settings->is_sensitive("password"));
}

TEST(NrdpTargetObject, AOnelinerSkipsTheNrdpKeys) {
  // The oneliner form (`target = https://...`) has no section of its own, so
  // read() must not register or read the per-section keys.
  auto settings = std::make_shared<fake_settings>();
  auto target = std::make_shared<nrdp_handler::nrdp_target_object>("default", "/settings/NRDP/client/targets");

  target->read(settings, true, false);

  EXPECT_FALSE(settings->has_registered("token"));
  EXPECT_EQ(target->get_property_string("tls version"), "") << "the 1.3 default is only applied for full sections";
}

// ---------------------------------------------------------------------------
// options_reader_impl - factory and command-line options.
// ---------------------------------------------------------------------------

TEST(NrdpOptionsReader, CreateAndCloneBuildNrdpTargets) {
  nrdp_handler::options_reader_impl reader;

  const nscapi::settings_objects::object_instance parent = reader.create("default", "/settings/NRDP/client/targets");
  ASSERT_TRUE(parent);
  EXPECT_EQ(parent->get_property_int("timeout", 0), 30);

  parent->set_property_string("token", "inherited");
  const nscapi::settings_objects::object_instance child = reader.clone(parent, "mine", "/settings/NRDP/client/targets");
  ASSERT_TRUE(child);
  EXPECT_EQ(child->get_alias(), "mine");
  EXPECT_EQ(child->get_property_string("token"), "inherited");
}

TEST(NrdpOptionsReader, CommandLineOptionsLandInTheDestinationContainer) {
  client::destination_container data = parse_options(
      {"--token", "s3cret", "--tls-version", "1.2", "--verify", "none", "--ca", "/etc/ca.pem", "--proxy", "http://proxy:3128/", "--no-proxy", "localhost"});

  // These are the exact keys connection_data looks up.
  EXPECT_EQ(data.get_string_data("token"), "s3cret");
  EXPECT_EQ(data.get_string_data("tls version"), "1.2");
  EXPECT_EQ(data.get_string_data("verify mode"), "none");
  EXPECT_EQ(data.get_string_data("ca"), "/etc/ca.pem");
  EXPECT_EQ(data.get_string_data("proxy"), "http://proxy:3128/");
  EXPECT_EQ(data.get_string_data("no proxy"), "localhost");
}

TEST(NrdpOptionsReader, KeyAndPasswordOptionsAreTokenAliases) {
  EXPECT_EQ(parse_options({"--key", "via-key"}).get_string_data("token"), "via-key");
  EXPECT_EQ(parse_options({"--password", "via-password"}).get_string_data("token"), "via-password");
}

TEST(NrdpOptionsReader, TheLegacySpacedAndDashedAliasesStillWork) {
  // Older command lines used "--tls version" / "--verify mode" (with a
  // space); --verify-mode is the dashed alias. All must keep working.
  EXPECT_EQ(parse_options({"--tls version", "1.1"}).get_string_data("tls version"), "1.1");
  EXPECT_EQ(parse_options({"--verify mode", "peer-cert"}).get_string_data("verify mode"), "peer-cert");
  EXPECT_EQ(parse_options({"--verify-mode", "peer"}).get_string_data("verify mode"), "peer");
}
