// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "on_demand_facts.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_helper.hpp>

using nsclient::core::on_demand_facts;

namespace {
const unsigned int ext = 1;
const unsigned int python = 2;
const unsigned int system_module = 3;

// The three script runtimes and the counter catalog, as the modules declare
// them.
void declare_all(on_demand_facts &sets) {
  EXPECT_TRUE(sets.declare(ext, {{"scripts.external", false}}).empty());
  EXPECT_TRUE(sets.declare(python, {{"scripts.python", false}}).empty());
  EXPECT_TRUE(sets.declare(system_module, {{"pdh.objects", false}, {"pdh.instances", true}}).empty());
}

std::string python_listing() {
  nscapi::facts::response out;
  out.set("scripts").object("python").strings("files", {"python/b.py", "python/a.py"});
  return out.serialize();
}

std::string json_of(const PB::Facts::Value &value) { return nscapi::facts::tree::to_json(value); }
}  // namespace

TEST(on_demand_facts, an_undeclared_path_is_the_documents_business) {
  on_demand_facts sets;
  declare_all(sets);
  EXPECT_FALSE(sets.resolve("os").on_demand);
  EXPECT_FALSE(sets.resolve("os.family").on_demand);
  EXPECT_FALSE(sets.resolve("").on_demand);
  // A prefix of a set name is not the set.
  EXPECT_FALSE(sets.resolve("script").on_demand);
  EXPECT_FALSE(sets.covers("script"));
  EXPECT_TRUE(sets.covers("scripts"));
}

TEST(on_demand_facts, a_declared_key_resolves_to_its_producer) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::plan plan = sets.resolve("scripts.python");
  ASSERT_TRUE(plan.on_demand);
  EXPECT_FALSE(plan.merge);
  ASSERT_EQ(plan.targets.size(), 1u);
  EXPECT_EQ(plan.targets[0].id, "scripts.python");
  EXPECT_EQ(plan.targets[0].plugin_id, python);
  EXPECT_TRUE(plan.targets[0].remainder.empty());
}

TEST(on_demand_facts, a_path_below_a_declared_key_is_walked_in_its_answer) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::plan plan = sets.resolve("scripts.python.files");
  ASSERT_EQ(plan.targets.size(), 1u);
  EXPECT_EQ(plan.targets[0].remainder, std::vector<std::string>{"files"});

  PB::Facts::Value out;
  ASSERT_TRUE(on_demand_facts::assemble(plan, {on_demand_facts::extract(python_listing(), "scripts.python")}, out));
  EXPECT_EQ(json_of(out), R"(["python/b.py","python/a.py"])");
}

TEST(on_demand_facts, a_path_that_is_not_in_the_answer_is_not_found) {
  on_demand_facts sets;
  declare_all(sets);
  PB::Facts::Value out;
  EXPECT_FALSE(on_demand_facts::assemble(sets.resolve("scripts.python.nope"), {on_demand_facts::extract(python_listing(), "scripts.python")}, out));
  EXPECT_FALSE(on_demand_facts::assemble(sets.resolve("scripts.python."), {on_demand_facts::extract(python_listing(), "scripts.python")}, out));
}

TEST(on_demand_facts, the_set_itself_is_every_declared_key_as_one_object) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::plan plan = sets.resolve("scripts");
  ASSERT_TRUE(plan.on_demand);
  EXPECT_TRUE(plan.merge);
  ASSERT_EQ(plan.targets.size(), 2u);
  EXPECT_EQ(plan.targets[0].id, "scripts.external");
  EXPECT_EQ(plan.targets[1].id, "scripts.python");

  // One runtime answered and one did not: the object holds what was found.
  on_demand_facts::answer missing;
  PB::Facts::Value out;
  ASSERT_TRUE(on_demand_facts::assemble(plan, {missing, on_demand_facts::extract(python_listing(), "scripts.python")}, out));
  EXPECT_EQ(json_of(out), R"({"python":{"files":["python/b.py","python/a.py"]}})");
  // None did: not found, rather than an empty object.
  PB::Facts::Value none;
  EXPECT_FALSE(on_demand_facts::assemble(plan, {missing, missing}, none));
}

TEST(on_demand_facts, a_parameterized_set_is_left_out_of_the_merge) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::plan plan = sets.resolve("pdh");
  ASSERT_EQ(plan.targets.size(), 1u);
  EXPECT_EQ(plan.targets[0].id, "pdh.objects");
}

TEST(on_demand_facts, an_undeclared_key_of_an_on_demand_set_has_no_producer) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::plan plan = sets.resolve("scripts.lua");
  // Still on demand - which is what keeps the document's grant from reading
  // it - but nobody to ask.
  EXPECT_TRUE(plan.on_demand);
  EXPECT_TRUE(plan.targets.empty());
  PB::Facts::Value out;
  EXPECT_FALSE(on_demand_facts::assemble(plan, {}, out));
}

TEST(on_demand_facts, the_rest_of_the_path_is_the_parameter_verbatim) {
  on_demand_facts sets;
  declare_all(sets);
  // A performance object name has dots and spaces in it; neither is path
  // syntax below a parameterized set.
  const on_demand_facts::plan plan = sets.resolve("pdh.instances..NET CLR Memory");
  ASSERT_TRUE(plan.error.empty()) << plan.error;
  ASSERT_EQ(plan.targets.size(), 1u);
  EXPECT_EQ(plan.targets[0].id, "pdh.instances");
  EXPECT_EQ(plan.targets[0].plugin_id, system_module);
  EXPECT_EQ(plan.targets[0].parameter, ".NET CLR Memory");
  EXPECT_TRUE(plan.targets[0].remainder.empty());
}

TEST(on_demand_facts, a_missing_or_malformed_parameter_is_refused_before_anyone_is_asked) {
  on_demand_facts sets;
  declare_all(sets);
  for (const std::string &path :
       {std::string("pdh.instances"), std::string("pdh.instances."), std::string("pdh.instances.a\nb"), std::string("pdh.instances.a\tb"),
        "pdh.instances." + std::string(on_demand_facts::max_parameter_length + 1, 'x'), std::string("pdh.instances.a") + std::string(1, '\0') + "b"}) {
    const on_demand_facts::plan plan = sets.resolve(path);
    EXPECT_TRUE(plan.on_demand);
    EXPECT_FALSE(plan.error.empty()) << path;
    EXPECT_TRUE(plan.targets.empty()) << path;
  }
  EXPECT_TRUE(sets.resolve("pdh.instances." + std::string(on_demand_facts::max_parameter_length, 'x')).error.empty());
}

TEST(on_demand_facts, a_whole_set_can_be_declared_too) {
  on_demand_facts sets;
  EXPECT_TRUE(sets.declare(7, {{"counters", false}}).empty());
  const on_demand_facts::plan plan = sets.resolve("counters.list");
  ASSERT_EQ(plan.targets.size(), 1u);
  EXPECT_EQ(plan.targets[0].id, "counters");
  EXPECT_EQ(plan.targets[0].remainder, std::vector<std::string>{"list"});

  nscapi::facts::response out;
  out.set("counters").strings("list", {"a"});
  PB::Facts::Value value;
  ASSERT_TRUE(on_demand_facts::assemble(plan, {on_demand_facts::extract(out.serialize(), "counters")}, value));
  EXPECT_EQ(json_of(value), R"(["a"])");
}

TEST(on_demand_facts, one_id_has_one_producer) {
  on_demand_facts sets;
  EXPECT_TRUE(sets.declare(1, {{"scripts.python", false}}).empty());
  // A second instance of the module, loaded under another alias.
  const std::vector<std::string> refused = sets.declare(2, {{"scripts.python", false}});
  ASSERT_EQ(refused.size(), 1u);
  EXPECT_NE(refused[0].find("already produced"), std::string::npos);
  EXPECT_EQ(sets.resolve("scripts.python").targets[0].plugin_id, 1u);
}

TEST(on_demand_facts, a_set_is_whole_or_keyed_never_both) {
  on_demand_facts sets;
  EXPECT_TRUE(sets.declare(1, {{"scripts.python", false}}).empty());
  EXPECT_EQ(sets.declare(2, {{"scripts", false}}).size(), 1u);
  on_demand_facts other;
  EXPECT_TRUE(other.declare(1, {{"scripts", false}}).empty());
  EXPECT_EQ(other.declare(2, {{"scripts.python", false}}).size(), 1u);
}

TEST(on_demand_facts, an_id_is_a_set_or_one_key_below_it) {
  on_demand_facts sets;
  EXPECT_EQ(sets.declare(1, {{"", false}, {"a.b.c", false}, {"Scripts", false}, {"a..b", false}, {"a.b c", false}, {"*", false}}).size(), 6u);
  EXPECT_TRUE(sets.get_declared().empty());
}

TEST(on_demand_facts, a_later_declaration_replaces_the_earlier_one) {
  on_demand_facts sets;
  sets.declare(1, {{"scripts.python", false}, {"scripts.lua", false}});
  sets.declare(1, {{"scripts.python", false}});
  EXPECT_EQ(sets.get_declared(), (std::vector<std::pair<std::string, bool>>{{"scripts.python", false}}));
}

TEST(on_demand_facts, an_unloaded_module_answers_for_nothing) {
  on_demand_facts sets;
  declare_all(sets);
  EXPECT_TRUE(sets.has_seen(python));
  sets.forget(python);
  EXPECT_FALSE(sets.has_seen(python));
  EXPECT_TRUE(sets.resolve("scripts.python").targets.empty());
  // The other runtime is untouched.
  EXPECT_EQ(sets.resolve("scripts.external").targets.size(), 1u);
}

TEST(on_demand_facts, a_module_with_nothing_to_declare_is_still_seen) {
  on_demand_facts sets;
  EXPECT_FALSE(sets.has_seen(9));
  sets.declare(9, {});
  EXPECT_TRUE(sets.has_seen(9));
}

TEST(on_demand_facts, extract_reads_one_key_out_of_the_shared_set) {
  const on_demand_facts::answer answer = on_demand_facts::extract(python_listing(), "scripts.python");
  ASSERT_TRUE(answer.found);
  EXPECT_EQ(json_of(answer.value), R"({"files":["python/b.py","python/a.py"]})");
  EXPECT_TRUE(answer.error.empty());
  // The same response does not answer for another module's key.
  EXPECT_FALSE(on_demand_facts::extract(python_listing(), "scripts.lua").found);
}

TEST(on_demand_facts, extract_sorts_the_keys_as_the_document_does) {
  nscapi::facts::response out;
  out.set("scripts").object("external").strings("files", {"x"}).strings("commands", {"y"});
  EXPECT_EQ(json_of(on_demand_facts::extract(out.serialize(), "scripts.external").value), R"({"commands":["y"],"files":["x"]})");
}

TEST(on_demand_facts, extract_carries_the_producers_error_and_age) {
  nscapi::facts::response failed;
  failed.error("scripts.python", "Failed to list /nowhere");
  const on_demand_facts::answer answer = on_demand_facts::extract(failed.serialize(), "scripts.python");
  EXPECT_FALSE(answer.found);
  EXPECT_EQ(answer.error, "Failed to list /nowhere");

  nscapi::facts::response aged;
  aged.set("scripts").object("python").strings("files", {});
  aged.gathered("scripts", 1700000000);
  EXPECT_EQ(on_demand_facts::extract(aged.serialize(), "scripts.python").gathered, "2023-11-14T22:13:20Z");
}

TEST(on_demand_facts, extract_ignores_a_declaration_riding_along) {
  nscapi::facts::response out;
  out.on_demand("scripts.python");
  const on_demand_facts::answer answer = on_demand_facts::extract(out.serialize(), "scripts.python");
  EXPECT_FALSE(answer.found);
  EXPECT_TRUE(answer.error.empty());
}

TEST(on_demand_facts, extract_holds_an_answer_to_the_document_rules) {
  // A record without an id: refused whole, as a stored set would be.
  PB::Facts::FactsMessage message;
  PB::Facts::FactsMessage::Response *payload = message.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  PB::Facts::FactSet *set = payload->add_sets();
  set->set_id("pdh");
  PB::Facts::Field *field = set->mutable_facts()->add_fields();
  field->set_key("objects");
  field->mutable_value()->mutable_list_value()->add_values()->mutable_object_value();
  const on_demand_facts::answer answer = on_demand_facts::extract(message.SerializeAsString(), "pdh.objects");
  EXPECT_FALSE(answer.found);
  EXPECT_NE(answer.error.find("id"), std::string::npos);
}

TEST(on_demand_facts, extract_reads_a_failed_or_unreadable_answer_as_an_error) {
  nscapi::facts::response failed;
  failed.failed("boom");
  EXPECT_EQ(on_demand_facts::extract(failed.serialize(), "scripts.python").error, "boom");
  EXPECT_FALSE(on_demand_facts::extract("", "scripts.python").error.empty());
  EXPECT_FALSE(on_demand_facts::extract("\xff\xff not protobuf", "scripts.python").found);
}

TEST(on_demand_facts, an_answer_is_cached_for_a_few_seconds) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::clock::time_point t0 = on_demand_facts::clock::now();
  const on_demand_facts::answer answer = on_demand_facts::extract(python_listing(), "scripts.python");
  on_demand_facts::answer cached;
  EXPECT_FALSE(sets.get_cached("scripts.python", "", t0, cached));
  sets.store("scripts.python", "", answer, t0);
  ASSERT_TRUE(sets.get_cached("scripts.python", "", t0 + std::chrono::seconds(1), cached));
  EXPECT_EQ(json_of(cached.value), json_of(answer.value));
  EXPECT_FALSE(sets.get_cached("scripts.python", "", t0 + on_demand_facts::cache_ttl, cached));
  // Another set, and another parameter of the same set, are other entries.
  EXPECT_FALSE(sets.get_cached("scripts.external", "", t0, cached));
  EXPECT_FALSE(sets.get_cached("scripts.python", "x", t0, cached));
}

TEST(on_demand_facts, a_failed_answer_is_not_cached) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::clock::time_point t0 = on_demand_facts::clock::now();
  on_demand_facts::answer failed;
  failed.error = "access denied";
  sets.store("scripts.python", "", failed, t0);
  on_demand_facts::answer cached;
  EXPECT_FALSE(sets.get_cached("scripts.python", "", t0, cached));
}

TEST(on_demand_facts, a_truncated_answer_is_cached_with_its_note) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::clock::time_point t0 = on_demand_facts::clock::now();
  on_demand_facts::answer truncated = on_demand_facts::extract(python_listing(), "scripts.python");
  truncated.error = "Script listing truncated: found 6000, reporting 5000";
  sets.store("scripts.python", "", truncated, t0);
  on_demand_facts::answer cached;
  ASSERT_TRUE(sets.get_cached("scripts.python", "", t0, cached));
  EXPECT_EQ(cached.error, truncated.error);
}

TEST(on_demand_facts, the_cache_is_cleared_by_a_refresh_and_by_an_unload) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::clock::time_point t0 = on_demand_facts::clock::now();
  const on_demand_facts::answer answer = on_demand_facts::extract(python_listing(), "scripts.python");
  on_demand_facts::answer cached;
  sets.store("scripts.python", "", answer, t0);
  sets.clear_cache();
  EXPECT_FALSE(sets.get_cached("scripts.python", "", t0, cached));

  sets.store("scripts.python", "", answer, t0);
  sets.forget(python);
  EXPECT_FALSE(sets.get_cached("scripts.python", "", t0, cached));
  // And an answer that arrives after the unload is not kept.
  sets.store("scripts.python", "", answer, t0);
  EXPECT_FALSE(sets.get_cached("scripts.python", "", t0, cached));
}

TEST(on_demand_facts, the_cache_is_bounded) {
  on_demand_facts sets;
  declare_all(sets);
  const on_demand_facts::clock::time_point t0 = on_demand_facts::clock::now();
  on_demand_facts::answer answer;
  answer.found = true;
  answer.value.set_string_value("x");
  // One entry per parameter: a caller walking every object must not grow it
  // without limit.
  for (std::size_t i = 0; i < on_demand_facts::max_cache_entries + 10; ++i) {
    sets.store("pdh.instances", "object " + std::to_string(i), answer, t0 + std::chrono::milliseconds(i));
  }
  std::size_t held = 0;
  on_demand_facts::answer cached;
  for (std::size_t i = 0; i < on_demand_facts::max_cache_entries + 10; ++i) {
    if (sets.get_cached("pdh.instances", "object " + std::to_string(i), t0 + std::chrono::milliseconds(100), cached)) ++held;
  }
  EXPECT_EQ(held, on_demand_facts::max_cache_entries);
  // The newest survive.
  EXPECT_TRUE(
      sets.get_cached("pdh.instances", "object " + std::to_string(on_demand_facts::max_cache_entries + 9), t0 + std::chrono::milliseconds(100), cached));
  EXPECT_FALSE(sets.get_cached("pdh.instances", "object 0", t0 + std::chrono::milliseconds(100), cached));
}
