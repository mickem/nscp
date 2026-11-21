// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/pdh_facts.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>

using namespace nscapi::facts::testing;

TEST(pdh_facts, objects_keep_the_enumeration_order) {
  // Not sorted: `\object\counter` in this order is what the counters
  // metadata endpoint has always returned.
  nscapi::facts::response out;
  pdh_facts::publish_objects({{"Processor", {"% Processor Time", "% Idle Time"}}, {"Memory", {"Available Bytes"}}}, 1700000000, out);
  EXPECT_EQ(json_of(out, "pdh"),
            R"({"objects":[{"id":"Processor","counters":["% Processor Time","% Idle Time"]},{"id":"Memory","counters":["Available Bytes"]}]})");
  EXPECT_EQ(gathered_of(out, "pdh"), "2023-11-14T22:13:20Z");
  EXPECT_EQ(error_of(out, pdh_facts::id_objects), "");
}

TEST(pdh_facts, an_object_without_a_name_or_listed_twice_is_skipped) {
  // A record id has to be present and unique, or the core refuses the set.
  nscapi::facts::response out;
  pdh_facts::publish_objects({{"", {"x"}}, {"Memory", {"a"}}, {"Memory", {"b"}}}, 1, out);
  EXPECT_EQ(json_of(out, "pdh"), R"({"objects":[{"id":"Memory","counters":["a"]}]})");
}

TEST(pdh_facts, an_object_without_counters_is_still_listed) {
  nscapi::facts::response out;
  pdh_facts::publish_objects({{"Empty", {}}}, 1, out);
  EXPECT_EQ(json_of(out, "pdh"), R"({"objects":[{"id":"Empty","counters":[]}]})");
}

TEST(pdh_facts, the_catalog_is_capped_and_says_so) {
  std::vector<pdh_facts::object> objects;
  for (std::size_t i = 0; i < pdh_facts::max_entries + 3; ++i) objects.push_back({"object " + std::to_string(i), {"c"}});
  nscapi::facts::response out;
  pdh_facts::publish_objects(objects, 1, out);
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, "pdh");
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(static_cast<std::size_t>(nscapi::facts::tree::get(set->facts(), "objects")->list_value().values_size()), pdh_facts::max_entries);
  EXPECT_NE(error_of(out, pdh_facts::id_objects).find("truncated"), std::string::npos);
}

TEST(pdh_facts, the_parameter_only_ever_selects_an_enumerated_name) {
  const std::vector<std::string> known = {"Processor", ".NET CLR Memory", "Memory"};
  EXPECT_EQ(pdh_facts::find_object(known, "Processor"), "Processor");
  // Dots and spaces are part of a name, not path syntax.
  EXPECT_EQ(pdh_facts::find_object(known, ".NET CLR Memory"), ".NET CLR Memory");
  // Anything that is not a name PDH enumerated selects nothing: no wildcard,
  // no counter path, no option.
  EXPECT_EQ(pdh_facts::find_object(known, "processor"), "");
  EXPECT_EQ(pdh_facts::find_object(known, "*"), "");
  EXPECT_EQ(pdh_facts::find_object(known, "\\Processor(*)\\% Processor Time"), "");
  EXPECT_EQ(pdh_facts::find_object(known, "Memory --computer other"), "");
  EXPECT_EQ(pdh_facts::find_object(known, ""), "");
}

TEST(pdh_facts, instances_name_the_object_they_belong_to) {
  nscapi::facts::response out;
  pdh_facts::publish_instances("Processor", {"0", "1", "_Total"}, 1700000000, out);
  EXPECT_EQ(json_of(out, "pdh"), R"({"instances":{"object":"Processor","names":["0","1","_Total"]}})");
}

TEST(pdh_facts, an_object_without_instances_is_an_empty_list) {
  nscapi::facts::response out;
  pdh_facts::publish_instances("Memory", {}, 1, out);
  EXPECT_EQ(json_of(out, "pdh"), R"({"instances":{"object":"Memory","names":[]}})");
}
