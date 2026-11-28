// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_test_helper.hpp>
#include <scripts/script_facts.hpp>

using namespace nscapi::facts::testing;

TEST(script_facts, each_runtime_declares_its_own_key_of_the_set) {
  EXPECT_EQ(scripts::facts::id_of(scripts::facts::key_external), "scripts.external");
  EXPECT_EQ(scripts::facts::id_of(scripts::facts::key_python), "scripts.python");
  EXPECT_EQ(scripts::facts::id_of(scripts::facts::key_lua), "scripts.lua");
}

TEST(script_facts, files_keep_the_order_the_folder_walk_found_them) {
  // Not sorted: this is the array `list --json` printed, and the scripts API
  // still answers with it.
  scripts::facts::listing listing;
  listing.files = {"python/b.py", "python/a.py"};
  nscapi::facts::response out;
  scripts::facts::publish(scripts::facts::key_python, listing, 1700000000, out);
  EXPECT_EQ(json_of(out, "scripts"), R"({"python":{"files":["python/b.py","python/a.py"]}})");
  EXPECT_EQ(gathered_of(out, "scripts"), "2023-11-14T22:13:20Z");
}

TEST(script_facts, an_empty_folder_is_an_empty_list_not_an_absent_runtime) {
  nscapi::facts::response out;
  scripts::facts::publish(scripts::facts::key_lua, scripts::facts::listing(), 1, out);
  EXPECT_EQ(json_of(out, "scripts"), R"({"lua":{"files":[]}})");
}

TEST(script_facts, only_a_runtime_with_commands_of_its_own_lists_them) {
  scripts::facts::listing listing;
  listing.files = {"scripts/check_ok.sh"};
  listing.commands = {"check_ok"};
  listing.has_commands = true;
  nscapi::facts::response out;
  scripts::facts::publish(scripts::facts::key_external, listing, 1, out);
  EXPECT_EQ(json_of(out, "scripts"), R"({"external":{"files":["scripts/check_ok.sh"],"commands":["check_ok"]}})");
}

TEST(script_facts, a_listing_is_names_and_nothing_else) {
  // Whatever a module fills in, the set has two keys to put it under: there
  // is nowhere for a script's source or its command line to go.
  scripts::facts::listing listing;
  listing.files = {"a"};
  listing.commands = {"b"};
  listing.has_commands = true;
  nscapi::facts::response out;
  scripts::facts::publish(scripts::facts::key_external, listing, 1, out);
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, "scripts");
  ASSERT_NE(set, nullptr);
  const PB::Facts::Value *runtime = nscapi::facts::tree::get(set->facts(), "external");
  ASSERT_NE(runtime, nullptr);
  EXPECT_EQ(runtime->object_value().fields_size(), 2);
}

TEST(script_facts, a_long_listing_is_capped_and_says_so) {
  scripts::facts::listing listing;
  for (std::size_t i = 0; i < scripts::facts::max_entries + 2; ++i) listing.files.push_back("f" + std::to_string(i));
  nscapi::facts::response out;
  scripts::facts::publish(scripts::facts::key_python, listing, 1, out);
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, "scripts");
  ASSERT_NE(set, nullptr);
  const PB::Facts::Value *runtime = nscapi::facts::tree::get(set->facts(), "python");
  ASSERT_NE(runtime, nullptr);
  EXPECT_EQ(static_cast<std::size_t>(nscapi::facts::tree::get(runtime->object_value(), "files")->list_value().values_size()), scripts::facts::max_entries);
  EXPECT_NE(error_of(out, "scripts.python").find("truncated"), std::string::npos);
}
