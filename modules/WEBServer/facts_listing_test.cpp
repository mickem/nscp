// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_listing.hpp"

#include <gtest/gtest.h>

namespace {
PB::Facts::Value strings(const std::vector<std::string> &values) {
  PB::Facts::Value list;
  list.mutable_list_value();
  for (const std::string &value : values) list.mutable_list_value()->add_values()->set_string_value(value);
  return list;
}

void add_object(PB::Facts::Value &objects, const std::string &name, const std::vector<std::string> &counters) {
  PB::Facts::Object *record = objects.mutable_list_value()->add_values()->mutable_object_value();
  PB::Facts::Field *id = record->add_fields();
  id->set_key("id");
  id->mutable_value()->set_string_value(name);
  PB::Facts::Field *list = record->add_fields();
  list->set_key("counters");
  *list->mutable_value() = strings(counters);
}

std::string envelope(const bool found, const bool on_demand, const std::map<std::string, std::string> &errors = {}) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = message.add_payload();
  payload->set_found(found);
  payload->set_on_demand(on_demand);
  if (found) *payload->mutable_facts() = strings({"a"});
  for (const std::pair<const std::string, std::string> &error : errors) {
    PB::Common::KeyValue *entry = payload->add_errors();
    entry->set_key(error.first);
    entry->set_value(error.second);
  }
  PB::Facts::FactsResponseMessage::Response::OnDemandSet *set = payload->add_on_demand_sets();
  set->set_id("scripts.python");
  return message.SerializeAsString();
}
}  // namespace

TEST(facts_listing, the_set_of_a_path_is_its_first_segment) {
  EXPECT_EQ(facts_listing::set_of("os"), "os");
  EXPECT_EQ(facts_listing::set_of("scripts.python.files"), "scripts");
  EXPECT_EQ(facts_listing::set_of("pdh.instances..NET CLR Memory"), "pdh");
  EXPECT_EQ(facts_listing::set_of(""), "");
}

TEST(facts_listing, a_first_segment_that_could_not_name_a_set_names_no_grant) {
  // It is spliced into `facts.get.<set>`, so nothing that means something to
  // the grant matcher - or is not a key at all - gets that far.
  for (const std::string &path : {std::string("*"), std::string("*.x"), std::string("a,b"), std::string("A"), std::string("1a"), std::string(" os"),
                                  std::string(".os"), std::string("os get"), std::string(65, 'a')}) {
    EXPECT_EQ(facts_listing::set_of(path), "") << path;
  }
}

TEST(facts_listing, names_render_as_the_array_the_list_command_printed) {
  EXPECT_EQ(facts_listing::render_names(strings({"scripts\\check_ok.bat", "scripts/a \"b\".sh"})), R"(["scripts\\check_ok.bat","scripts/a \"b\".sh"])");
  EXPECT_EQ(facts_listing::render_names(strings({})), "[]");
  // Not a list at all (the path named something else): still valid JSON.
  PB::Facts::Value scalar;
  scalar.set_string_value("x");
  EXPECT_EQ(facts_listing::render_names(scalar), "[]");
}

TEST(facts_listing, counters_render_as_the_paths_the_pdh_command_printed) {
  PB::Facts::Value objects;
  objects.mutable_list_value();
  add_object(objects, "Processor", {"% Processor Time", "% Idle Time"});
  add_object(objects, "Empty", {});
  add_object(objects, "Memory", {"Available Bytes"});
  EXPECT_EQ(facts_listing::render_counters(objects), R"(["\\Processor\\% Processor Time","\\Processor\\% Idle Time","\\Memory\\Available Bytes"])");
  PB::Facts::Value none;
  EXPECT_EQ(facts_listing::render_counters(none), "[]");
}

TEST(facts_listing, parse_reads_what_the_core_answered) {
  const facts_listing::reading found = facts_listing::parse(envelope(true, true), "scripts.python");
  EXPECT_TRUE(found.answered);
  EXPECT_TRUE(found.found);
  EXPECT_TRUE(found.on_demand);
  EXPECT_EQ(facts_listing::render_names(found.value), R"(["a"])");

  const facts_listing::reading missing = facts_listing::parse(envelope(false, false), "os");
  EXPECT_TRUE(missing.answered);
  EXPECT_FALSE(missing.found);
  EXPECT_FALSE(missing.on_demand);

  EXPECT_FALSE(facts_listing::parse("", "os").answered);
}

TEST(facts_listing, parse_picks_the_error_of_the_set_that_was_asked_for) {
  const std::map<std::string, std::string> errors = {{"os", "unrelated"}, {"scripts.python", "Failed to list /nowhere"}, {"scripts_other", "also unrelated"}};
  EXPECT_EQ(facts_listing::parse(envelope(false, true, errors), "scripts.python").error, "Failed to list /nowhere");
  // An error against a key of the set is the set's.
  EXPECT_EQ(facts_listing::parse(envelope(false, true, errors), "scripts").error, "Failed to list /nowhere");
  EXPECT_EQ(facts_listing::parse(envelope(false, true, errors), "pdh").error, "");
}

TEST(facts_listing, declares_reads_the_on_demand_catalog) {
  EXPECT_TRUE(facts_listing::declares(envelope(false, false), "scripts.python"));
  EXPECT_FALSE(facts_listing::declares(envelope(false, false), "scripts.lua"));
  EXPECT_FALSE(facts_listing::declares("", "scripts.python"));
}
