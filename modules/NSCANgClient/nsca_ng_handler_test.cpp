// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the NSCA-NG settings/CLI bridge: the per-target settings object
// (defaults, keys read from the settings store) and the options reader that
// maps command-line options into the destination container the client reads.

#include "nsca_ng_handler.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace po = boost::program_options;

namespace {

// Minimal in-memory settings store (same shape as the mock used by the
// nscapi settings object tests).
class mock_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> settings_data;

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string, std::string, std::string, std::string, std::string, bool, bool, bool) override {}
  void register_subkey(std::string, std::string, std::string, bool, bool) override {}
  void register_tpl(std::string, std::string, std::string, std::string, std::string) override {}

  std::string get_string(std::string path, std::string key, std::string def) override {
    const auto pit = settings_data.find(path);
    if (pit == settings_data.end()) return def;
    const auto kit = pit->second.find(key);
    if (kit == pit->second.end()) return def;
    return kit->second;
  }
  void set_string(std::string path, std::string key, std::string value) override { settings_data[path][key] = value; }

  string_list get_sections(std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  std::string expand_path(std::string key) override { return key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}

  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}
};

constexpr char kBasePath[] = "/settings/NSCA-NG/client/targets";

}  // namespace

// ============================================================================
// nsca_ng_target_object
// ============================================================================

TEST(NscaNgTargetObject, ConstructorSetsDefaults) {
  nsca_ng_handler::nsca_ng_target_object obj("default", kBasePath);

  EXPECT_EQ(obj.get_property_int("timeout", 0), 30);
  EXPECT_EQ(obj.get_property_int("retries", 0), 2);
  EXPECT_TRUE(obj.get_property_bool("use psk", false)) << "PSK must be the default auth mode";
  EXPECT_FALSE(obj.get_property_bool("host check", true)) << "service checks must be the default";
  // "port" is translated into the address property by the target base class.
  EXPECT_NE(obj.get_property_string("address").find("5668"), std::string::npos) << obj.get_property_string("address");
}

TEST(NscaNgTargetObject, ReadAppliesConfiguredSettings) {
  auto proxy = std::make_shared<mock_settings>();
  const std::string path = std::string(kBasePath) + "/default";
  proxy->settings_data[path]["password"] = "s3cret";
  proxy->settings_data[path]["identity"] = "ident1";
  proxy->settings_data[path]["use psk"] = "false";
  proxy->settings_data[path]["insecure"] = "true";
  proxy->settings_data[path]["max output length"] = "1234";
  proxy->settings_data[path]["host check"] = "true";

  nsca_ng_handler::nsca_ng_target_object obj("default", kBasePath);
  obj.read(proxy, false, false);

  EXPECT_EQ(obj.get_property_string("password"), "s3cret");
  EXPECT_EQ(obj.get_property_string("identity"), "ident1");
  EXPECT_FALSE(obj.get_property_bool("use psk", true));
  EXPECT_TRUE(obj.get_property_bool("insecure", false));
  EXPECT_EQ(obj.get_property_int("max output length", 0), 1234);
  EXPECT_TRUE(obj.get_property_bool("host check", false));
}

TEST(NscaNgTargetObject, ReadKeepsDefaultsWhenNothingIsConfigured) {
  auto proxy = std::make_shared<mock_settings>();

  nsca_ng_handler::nsca_ng_target_object obj("default", kBasePath);
  obj.read(proxy, false, false);

  EXPECT_TRUE(obj.get_property_bool("use psk", false));
  EXPECT_FALSE(obj.get_property_bool("insecure", true));
  EXPECT_FALSE(obj.get_property_bool("host check", true));
  EXPECT_EQ(obj.get_property_string("password"), "");
}

TEST(NscaNgTargetObject, ReadInSampleModeStillRegisters) {
  auto proxy = std::make_shared<mock_settings>();

  nsca_ng_handler::nsca_ng_target_object obj("sample", kBasePath);
  EXPECT_NO_THROW(obj.read(proxy, false, true));
}

// ============================================================================
// options_reader_impl — factory
// ============================================================================

TEST(NscaNgOptionsReader, CreateProducesTargetWithDefaults) {
  nsca_ng_handler::options_reader_impl reader;
  const nscapi::settings_objects::object_instance obj = reader.create("t1", kBasePath);

  ASSERT_TRUE(obj);
  EXPECT_EQ(obj->get_alias(), "t1");
  EXPECT_EQ(obj->get_property_int("timeout", 0), 30);
  EXPECT_TRUE(obj->get_property_bool("use psk", false));
}

TEST(NscaNgOptionsReader, CloneCopiesParentOptions) {
  nsca_ng_handler::options_reader_impl reader;
  const nscapi::settings_objects::object_instance parent = reader.create("default", kBasePath);
  parent->set_property_string("password", "inherited-pw");

  const nscapi::settings_objects::object_instance clone = reader.clone(parent, "other", kBasePath);
  ASSERT_TRUE(clone);
  EXPECT_EQ(clone->get_alias(), "other");
  EXPECT_EQ(clone->get_property_string("password"), "inherited-pw");
}

// ============================================================================
// options_reader_impl — CLI option mapping
// ============================================================================

namespace {

void parse(const std::vector<std::string> &args, client::destination_container &source, client::destination_container &data) {
  po::options_description desc;
  nsca_ng_handler::options_reader_impl reader;
  reader.process(desc, source, data);

  po::variables_map vm;
  po::store(po::command_line_parser(args).options(desc).run(), vm);
  po::notify(vm);
}

}  // namespace

TEST(NscaNgOptionsReader, ProcessMapsAllCliOptions) {
  client::destination_container source;
  client::destination_container data;
  parse({"--password", "pw1", "--identity", "id1", "--hostname", "sender1", "--no-psk", "--insecure", "--host-check", "--max-output-length", "123"}, source,
        data);

  EXPECT_EQ(data.get_string_data("password"), "pw1");
  EXPECT_EQ(data.get_string_data("identity"), "id1");
  EXPECT_EQ(source.address.host, "sender1") << "--hostname names the sender, not the target";
  EXPECT_FALSE(data.get_bool_data("use psk", true));
  EXPECT_TRUE(data.get_bool_data("insecure", false));
  EXPECT_TRUE(data.get_bool_data("host check", false));
  EXPECT_EQ(data.get_int_data("max output length", 0), 123);
}

TEST(NscaNgOptionsReader, SwitchesDefaultToSafeValues) {
  client::destination_container source;
  client::destination_container data;
  parse({"--password", "x"}, source, data);

  EXPECT_TRUE(data.get_bool_data("use psk", true)) << "--no-psk absent: PSK stays on";
  EXPECT_FALSE(data.get_bool_data("insecure", false)) << "--insecure absent: MITM protection stays on";
  EXPECT_FALSE(data.get_bool_data("host check", false));
}

TEST(NscaNgOptionsReader, SslOptionsAreRegistered) {
  client::destination_container source;
  client::destination_container data;
  parse({"--certificate", "/tmp/c.pem", "--ca", "/tmp/ca.pem", "--verify", "peer-cert"}, source, data);

  EXPECT_EQ(data.get_string_data("certificate"), "/tmp/c.pem");
  EXPECT_EQ(data.get_string_data("ca"), "/tmp/ca.pem");
  EXPECT_EQ(data.get_string_data("verify mode"), "peer-cert");
}
