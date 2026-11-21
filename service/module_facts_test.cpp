// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "module_facts.hpp"

#include <gtest/gtest.h>

#include "fact_repository.hpp"

namespace mf = nsclient::core::module_facts;

namespace {
mf::module make(const std::string &name, const std::string &alias, const std::string &title, const std::string &version, const bool enabled) {
  mf::module m;
  m.name = name;
  m.alias = alias;
  m.title = title;
  m.version = version;
  m.enabled = enabled;
  return m;
}
std::string json_of(const PB::Facts::Object &object) {
  // As the repository stores it: sorted, which is the order a reader sees.
  PB::Facts::Object sorted = object;
  nscapi::facts::tree::sort_fields(&sorted);
  return nscapi::facts::tree::to_json(sorted);
}
}  // namespace

TEST(module_facts, one_record_per_loaded_module_sorted_by_id) {
  const PB::Facts::Object set = mf::build({make("WEBServer", "", "Web server", "0.12.6", true), make("CheckDisk", "", "Disk checks", "0.12.6", false)});
  EXPECT_EQ(json_of(set), R"({"loaded":[{"enabled":false,"id":"CheckDisk","name":"CheckDisk","title":"Disk checks","version":"0.12.6"},)"
                          R"({"enabled":true,"id":"WEBServer","name":"WEBServer","title":"Web server","version":"0.12.6"}]})");
}

TEST(module_facts, load_order_is_not_a_change) {
  const std::vector<mf::module> forward = {make("A", "", "a", "1", true), make("B", "", "b", "1", true)};
  const std::vector<mf::module> backward = {forward[1], forward[0]};
  EXPECT_EQ(json_of(mf::build(forward)), json_of(mf::build(backward)));
}

TEST(module_facts, an_alias_is_the_record_id) {
  // What /api/v2/modules calls `id`: two instances of one module are two
  // records, told apart by the alias each was loaded under.
  const PB::Facts::Object set = mf::build({make("NRPEServer", "nrpe2", "NRPE", "1", true), make("NRPEServer", "", "NRPE", "1", true)});
  EXPECT_EQ(json_of(set), R"({"loaded":[{"enabled":true,"id":"NRPEServer","name":"NRPEServer","title":"NRPE","version":"1"},)"
                          R"({"alias":"nrpe2","enabled":true,"id":"nrpe2","name":"NRPEServer","title":"NRPE","version":"1"}]})");
}

TEST(module_facts, an_unknown_value_is_omitted_and_a_nameless_module_dropped) {
  const PB::Facts::Object set = mf::build({make("", "", "ghost", "1", true), make("CheckDisk", "", "", "", false)});
  EXPECT_EQ(json_of(set), R"({"loaded":[{"enabled":false,"id":"CheckDisk","name":"CheckDisk"}]})");
}

TEST(module_facts, a_repeated_id_is_listed_once) {
  const PB::Facts::Object set = mf::build({make("CheckDisk", "", "first", "1", true), make("CheckDisk", "", "second", "1", true)});
  const PB::Facts::Value *loaded = nscapi::facts::tree::get(set, "loaded");
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->list_value().values_size(), 1);
}

TEST(module_facts, no_modules_is_an_empty_list) { EXPECT_EQ(json_of(mf::build({})), R"({"loaded":[]})"); }

TEST(module_facts, the_set_passes_the_document_rules) {
  nsclient::core::fact_repository facts;
  std::string error;
  const PB::Facts::Object set = mf::build({make("CheckDisk", "disk", "Disk checks", "0.12.6", true), make("WEBServer", "", "Web", "0.12.6", true)});
  EXPECT_EQ(facts.set(mf::set_modules, nsclient::core::fact_repository::core_owner, set, error), nsclient::core::fact_repository::set_result::changed) << error;
}
