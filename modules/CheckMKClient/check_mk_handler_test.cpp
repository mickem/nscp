// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for check_mk_handler.hpp: the target object a
// `[/settings/check_mk/client/targets/x]` section is read into, and the
// options reader behind the check_mk client's command line. Both are thin
// wrappers around the shared target/SSL helpers, but they carry the module's
// defaults (the agent port, timeout and retries) and decide which SSL keys a
// check_mk target understands - a regression here silently points the client
// at the wrong port or drops a certificate setting.

// check_mk_handler.hpp expects its includer to have pulled in the client
// machinery already (CheckMKClient.cpp does); include it here too.
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_targets.hpp>

#include "check_mk_handler.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Normally provided by NSC_WRAP_DLL(); the plugin machinery needs it.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

// A settings backend backed by a plain map, so read() can be driven without a
// settings core (mirrors MockSettingsInterface in nscapi's helper_test).
class fake_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> values;
  std::vector<std::string> registered_keys;
  std::string expand_prefix;

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
  string_list get_sections(std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  std::string expand_path(std::string key) override { return expand_prefix + key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}
  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}

  bool registered(const std::string &key) const {
    return std::find(registered_keys.begin(), registered_keys.end(), key) != registered_keys.end();
  }
};

// The section every `[/settings/check_mk/client/targets]`-defined target
// lives under; the object appends its alias, so "default" reads kTargetPath.
const std::string kTargetsPath = "/settings/check_mk/client/targets";
const std::string kTargetPath = kTargetsPath + "/default";

}  // namespace

TEST(CheckMKTargetObject, ANewTargetGetsTheAgentDefaults) {
  check_mk_handler::check_mk_target_object target("default", kTargetsPath);

  // "port" is a routed property: target_object::translate folds it into the
  // address, so the default lands there rather than in a plain option.
  EXPECT_NE(target.get_property_string("address").find("5667"), std::string::npos) << target.get_property_string("address");
  EXPECT_EQ(target.get_property_int("timeout", 0), 30);
  EXPECT_EQ(target.get_property_int("retries", 0), 3);
}

TEST(CheckMKTargetObject, ReadPicksUpTheSslSettings) {
  auto settings = std::make_shared<fake_settings>();
  settings->values[kTargetPath] = {{"certificate", "/etc/ssl/client.pem"},
                                   {"certificate key", "/etc/ssl/client.key"},
                                   {"verify mode", "none"},
                                   {"use ssl", "true"}};

  check_mk_handler::check_mk_target_object target("default", kTargetsPath);
  target.read(settings, false, false);

  EXPECT_EQ(target.get_property_string("certificate"), "/etc/ssl/client.pem");
  EXPECT_EQ(target.get_property_string("certificate key"), "/etc/ssl/client.key");
  EXPECT_EQ(target.get_property_string("verify mode"), "none");
  EXPECT_TRUE(target.get_property_bool("ssl", false));
}

TEST(CheckMKTargetObject, ReadRegistersTheSslKeysForDocumentation) {
  auto settings = std::make_shared<fake_settings>();

  check_mk_handler::check_mk_target_object target("default", kTargetsPath);
  target.read(settings, false, false);

  for (const std::string key : {"certificate", "certificate key", "certificate format", "ca", "allowed ciphers", "verify mode", "use ssl", "dh"}) {
    EXPECT_TRUE(settings->registered(key)) << "key not registered: " << key;
  }
}

TEST(CheckMKTargetObject, ASampleReadStillRegistersTheKeys) {
  auto settings = std::make_shared<fake_settings>();

  check_mk_handler::check_mk_target_object target("default", kTargetsPath);
  target.read(settings, false, true);

  EXPECT_TRUE(settings->registered("use ssl"));
}

TEST(CheckMKOptionsReader, CreateAndCloneProduceTargetObjects) {
  check_mk_handler::options_reader_impl reader;

  const nscapi::settings_objects::object_instance created = reader.create("default", kTargetsPath);
  ASSERT_TRUE(created);
  EXPECT_EQ(created->get_alias(), "default");
  EXPECT_EQ(created->get_property_int("timeout", 0), 30) << "create() must go through the defaulting constructor";
  EXPECT_NE(created->get_property_string("address").find("5667"), std::string::npos) << "the default agent port must be applied";

  created->set_property_string("certificate", "/etc/ssl/client.pem");
  const nscapi::settings_objects::object_instance cloned = reader.clone(created, "copy", kTargetsPath);
  ASSERT_TRUE(cloned);
  EXPECT_EQ(cloned->get_alias(), "copy");
  EXPECT_EQ(cloned->get_property_string("certificate"), "/etc/ssl/client.pem") << "clone() must inherit the parent's settings";
}

TEST(CheckMKOptionsReader, ProcessOffersTheSslOptions) {
  check_mk_handler::options_reader_impl reader;
  boost::program_options::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  const std::vector<std::string> argv = {"--certificate", "/etc/ssl/client.pem", "--certificate-key", "/etc/ssl/client.key",
                                         "--ca",          "/etc/ssl/ca.pem",     "--verify",          "none",
                                         "--ssl"};
  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::command_line_parser(argv).options(desc).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ(data.get_string_data("certificate"), "/etc/ssl/client.pem");
  EXPECT_EQ(data.get_string_data("certificate key"), "/etc/ssl/client.key");
  EXPECT_EQ(data.get_string_data("ca"), "/etc/ssl/ca.pem");
  EXPECT_EQ(data.get_string_data("verify mode"), "none");
  EXPECT_TRUE(data.get_bool_data("ssl")) << "--ssl without a value must mean true";
}

TEST(CheckMKOptionsReader, AValuedSslFlagIsAccepted) {
  // REST passes booleans as `ssl=true` tokens; po::bool_switch would reject
  // that, so the option must take a value.
  check_mk_handler::options_reader_impl reader;
  boost::program_options::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  const std::vector<std::string> argv = {"--ssl=false"};
  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::command_line_parser(argv).options(desc).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_FALSE(data.get_bool_data("ssl", true));
}
