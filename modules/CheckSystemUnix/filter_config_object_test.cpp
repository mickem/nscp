// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "filter_config_object.hpp"

#include <gtest/gtest.h>

#include <list>
#include <map>
#include <memory>
#include <string>

// The real-time filter definitions under /settings/system/real-time/{cpu,
// memory,process}: what each reads from its section.

namespace {

class fake_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> values;

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
  string_list get_sections(const std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  std::string expand_path(std::string key) override { return key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}
  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}
};

const std::string base = "/settings/system/real-time";

template <class T>
T read(const std::string &section, const std::string &alias, const std::map<std::string, std::string> &keys, const std::string &oneliner = "") {
  auto settings = std::make_shared<fake_settings>();
  for (const auto &kv : keys) settings->values[base + "/" + section + "/" + alias][kv.first] = kv.second;
  T obj(alias, base + "/" + section);
  if (!oneliner.empty()) obj.set_value(oneliner);
  obj.read(settings, !oneliner.empty(), false);
  return obj;
}

std::list<std::string> L(std::initializer_list<std::string> v) { return v; }

}  // namespace

// set_data replaces the list with one entry; set_datas with the trimmed,
// comma separated entries; an empty value leaves the list alone, so a later
// empty `time` does not wipe what `times` set.
TEST(RealtimeFilterConfig, SetDataAndSetDatas) {
  filters::cpu::filter_config_object o("x", base + "/cpu");
  o.set_datas(" 1m , 5m,15m ");
  EXPECT_EQ(o.data, L({"1m", "5m", "15m"}));
  o.set_data("");
  o.set_datas("");
  EXPECT_EQ(o.data, L({"1m", "5m", "15m"}));
  o.set_data("10s");
  EXPECT_EQ(o.data, L({"10s"}));
}

TEST(RealtimeFilterConfig, CpuReadsTimesAndFilter) {
  const auto o = read<filters::cpu::filter_config_object>("cpu", "busy", {{"times", "1m, 5m"}, {"filter", "core = 'core 0'"}, {"warning", "load > 80"}});
  EXPECT_EQ(o.data, L({"1m", "5m"}));
  EXPECT_EQ(std::string(o.filter.filter_string()), "core = 'core 0'");
  EXPECT_EQ(o.filter.filter_warn, "load > 80");
  EXPECT_EQ(o.get_alias(), "busy");
}

TEST(RealtimeFilterConfig, CpuSingleTime) {
  const auto o = read<filters::cpu::filter_config_object>("cpu", "busy", {{"time", "30s"}});
  EXPECT_EQ(o.data, L({"30s"}));
}

// The default template watches the total, not every core.
TEST(RealtimeFilterConfig, CpuDefaultTemplateFiltersOnTotal) {
  const auto o = read<filters::cpu::filter_config_object>("cpu", "default", {});
  EXPECT_EQ(std::string(o.filter.filter_string()), "core = 'total'");
}

// A one-liner (`busy = load > 80`) is the filter itself.
TEST(RealtimeFilterConfig, OneLinerIsTheFilter) {
  const auto o = read<filters::cpu::filter_config_object>("cpu", "busy", {}, "load > 80");
  EXPECT_EQ(std::string(o.filter.filter_string()), "load > 80");
}

TEST(RealtimeFilterConfig, MemoryReadsTypes) {
  const auto o = read<filters::mem::filter_config_object>("memory", "low", {{"types", "physical,swap"}, {"critical", "used > 90%"}});
  EXPECT_EQ(o.data, L({"physical", "swap"}));
  EXPECT_EQ(o.filter.filter_crit, "used > 90%");
  EXPECT_EQ(read<filters::mem::filter_config_object>("memory", "low", {{"type", "cached"}}).data, L({"cached"}));
}

TEST(RealtimeFilterConfig, ProcessReadsProcesses) {
  const auto o = read<filters::proc::filter_config_object>("process", "web", {{"processes", "nginx, php-fpm"}});
  EXPECT_EQ(o.data, L({"nginx", "php-fpm"}));
  EXPECT_EQ(read<filters::proc::filter_config_object>("process", "web", {{"process", "sshd"}}).data, L({"sshd"}));
  // No process listed means all processes.
  EXPECT_TRUE(read<filters::proc::filter_config_object>("process", "all", {}).data.empty());
}

TEST(RealtimeFilterConfig, ToStringNamesTheAlias) {
  EXPECT_EQ(filters::cpu::filter_config_object("c", base).to_string().rfind("c[c] = {tpl: ", 0), 0u);
  EXPECT_EQ(filters::mem::filter_config_object("m", base).to_string().rfind("m[m] = {tpl: ", 0), 0u);
  EXPECT_EQ(filters::proc::filter_config_object("p", base).to_string().rfind("p[p] = {tpl: ", 0), 0u);
}
