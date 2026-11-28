// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// How a client target (`/settings/<X>/client/targets/<name>`) takes its keys
// from its parent: everything the target's own section leaves out comes from
// the target it names as `parent` (`default` when it names none), and only
// `default` itself falls back to the built-in defaults.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/settings/object.hpp>
#include <string>

namespace {

class fake_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> values;

  void set(const std::string &path, const std::map<std::string, std::string> &kv) {
    for (const auto &e : kv) values[path][e.first] = e.second;
  }

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string, std::string, std::string, std::string, std::string, bool, bool, bool) override {}
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
  string_list get_keys(std::string path) override {
    string_list ret;
    const auto pit = values.find(path);
    if (pit != values.end())
      for (const auto &e : pit->second) ret.push_back(e.first);
    return ret;
  }
  std::string expand_path(std::string key) override { return key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}
  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}
};

// A target with TLS keys that have built-in defaults, the way NSCPClient and
// CheckMKClient register them.
struct tls_target : nscapi::targets::target_object {
  tls_target(std::string alias, std::string path) : target_object(std::move(alias), std::move(path)) {}
  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) override {
    target_object::read(proxy, oneliner, is_sample);
    nscapi::settings_helper::settings_registry settings(proxy);
    add_ssl_keys(settings.path(get_path()), {"peer", "/etc/ca.pem"});
    settings.register_all();
    settings.notify();
  }
};

const std::string kTargets = "/settings/X/client/targets";

class TargetInheritance : public ::testing::Test {
 protected:
  std::shared_ptr<fake_settings> settings_ = std::make_shared<fake_settings>();
  nscapi::settings_objects::object_handler<tls_target> targets_;

  void SetUp() override { targets_.set_path(kTargets); }
  void set(const std::string &name, const std::map<std::string, std::string> &kv) { settings_->set(kTargets + "/" + name, kv); }
  std::shared_ptr<tls_target> load(const std::string &name, const std::string &value = "") {
    targets_.add(settings_, name, value);
    return targets_.find_object(name);
  }
};

}  // namespace

TEST_F(TargetInheritance, DefaultKeepsTheBuiltInDefaults) {
  const auto t = load("default");
  EXPECT_EQ(t->get_property_int("timeout", 0), 30);
  EXPECT_EQ(t->get_property_int("retries", 0), 3);
  EXPECT_FALSE(t->get_property_bool("allow host override", true));
  EXPECT_EQ(t->get_property_string("verify mode"), "peer");
  EXPECT_EQ(t->get_property_string("ca"), "/etc/ca.pem");
}

TEST_F(TargetInheritance, KeysWithBuiltInDefaultsComeFromDefault) {
  // Each of these is registered with a default, and a target that left them
  // out used to be reset to it, whatever `default` said.
  set("default", {{"timeout", "60"}, {"retries", "7"}, {"allow host override", "true"}, {"verify mode", "peer-cert"}, {"ca", "/etc/own-ca.pem"}});
  set("child", {{"address", "child.example.com"}});

  const auto t = load("child");
  EXPECT_EQ(t->get_property_int("timeout", 0), 60);
  EXPECT_EQ(t->get_property_int("retries", 0), 7);
  EXPECT_TRUE(t->get_property_bool("allow host override", false));
  EXPECT_EQ(t->get_property_string("verify mode"), "peer-cert");
  EXPECT_EQ(t->get_property_string("ca"), "/etc/own-ca.pem");
}

TEST_F(TargetInheritance, AOneLineTargetInheritsEverythingButTheAddress) {
  set("default", {{"timeout", "60"}, {"address", "default.example.com:1"}, {"host", "other.example.com"}});
  const auto t = load("short", "short.example.com:5666");

  EXPECT_EQ(t->get_property_int("timeout", 0), 60);
  EXPECT_EQ(t->get_property_string("address"), "short.example.com:5666");
}

TEST_F(TargetInheritance, ASectionTargetWithoutAnAddressTakesItsParents) {
  set("tpl", {{"is template", "true"}, {"address", "tpl.example.com:5666"}});
  set("child", {{"parent", "tpl"}, {"timeout", "5"}});

  const auto t = load("child");
  EXPECT_EQ(t->get_property_string("address"), "tpl.example.com:5666");
  EXPECT_EQ(t->get_property_int("timeout", 0), 5);
}

TEST_F(TargetInheritance, AnOwnAddressIsNotRewrittenByAnInheritedHostOrPort) {
  set("default", {{"host", "default.example.com"}, {"port", "1"}});
  set("child", {{"address", "child.example.com:2"}});

  EXPECT_EQ(load("child")->get_property_string("address"), "child.example.com:2");
}

TEST_F(TargetInheritance, AnOwnPortAppliesToTheInheritedAddress) {
  set("default", {{"address", "tcp://default.example.com:1"}});
  set("child", {{"port", "2"}});

  EXPECT_EQ(load("child")->get_property_string("address"), "tcp://default.example.com:2");
}
