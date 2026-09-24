// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/software_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/protobuf/facts.hpp>
#include <string>
#include <vector>

// What a package record looks like is decided once, here, for both platforms:
// the architecture vocabulary, the order of the list, which installs are the
// same install, and what a record id is.

namespace {

software_facts::package make(const std::string &name, const std::string &version = "", const std::string &architecture = "") {
  software_facts::package p;
  p.name = name;
  p.version = version;
  p.architecture = architecture;
  return p;
}

std::vector<std::string> ids_of(const std::vector<software_facts::package> &packages) {
  std::vector<std::string> ids;
  for (const software_facts::package &p : packages) ids.push_back(p.id);
  return ids;
}

// The `installed` list of the published set, or an empty list when the set
// carries none.
PB::Facts::List installed_list(const nscapi::facts::response &response) {
  const PB::Facts::FactsMessage message = response.to_message();
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() != software_facts::set_software) continue;
    const PB::Facts::Value *list = nscapi::facts::tree::get(set.facts(), software_facts::key_installed);
    if (list != nullptr && list->has_list_value()) return list->list_value();
  }
  return PB::Facts::List();
}

std::string error_for(const nscapi::facts::response &response, const std::string &set_id) {
  const PB::Facts::FactsMessage message = response.to_message();
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() == set_id) return set.error();
  }
  return "";
}

std::string field_of(const PB::Facts::Value &record, const std::string &key) {
  const PB::Facts::Value *value = nscapi::facts::tree::get(record.object_value(), key);
  return value == nullptr ? "" : value->string_value();
}

}  // namespace

TEST(software_facts, an_architecture_means_the_same_thing_on_every_platform) {
  // dpkg says amd64, rpm and pacman say x86_64, the registry view says x64.
  EXPECT_EQ(software_facts::normalize_architecture("amd64"), "x86_64");
  EXPECT_EQ(software_facts::normalize_architecture("x86_64"), "x86_64");
  EXPECT_EQ(software_facts::normalize_architecture("x64"), "x86_64");
  EXPECT_EQ(software_facts::normalize_architecture("i386"), "x86");
  EXPECT_EQ(software_facts::normalize_architecture("aarch64"), "arm64");
  EXPECT_EQ(software_facts::normalize_architecture("ARM64"), "arm64");
  // A package that runs anywhere, in all three spellings.
  EXPECT_EQ(software_facts::normalize_architecture("noarch"), "noarch");
  EXPECT_EQ(software_facts::normalize_architecture("all"), "noarch");
  EXPECT_EQ(software_facts::normalize_architecture("any"), "noarch");
  // Unknown is published lower-cased rather than dropped, and unknown stays
  // unknown.
  EXPECT_EQ(software_facts::normalize_architecture("Loongarch64"), "loongarch64");
  EXPECT_EQ(software_facts::normalize_architecture(""), "");
  // What rpm prints for a package that has no architecture at all: its
  // spelling of "unknown", which the document leaves out rather than
  // publishing as a word no other platform uses.
  EXPECT_EQ(software_facts::normalize_architecture("(none)"), "");
  EXPECT_EQ(software_facts::normalize_architecture("(null)"), "");
}

TEST(software_facts, the_list_is_sorted_so_an_unchanged_host_is_an_unchanged_document) {
  const std::vector<software_facts::package> prepared = software_facts::prepare({make("zlib", "1.3"), make("bash", "5.2"), make("acl", "2.3")});
  EXPECT_EQ(ids_of(prepared), (std::vector<std::string>{"acl", "bash", "zlib"}));
}

TEST(software_facts, a_name_is_its_own_id_when_only_one_install_has_it) {
  const std::vector<software_facts::package> prepared = software_facts::prepare({make("Google Chrome", "129.0.6668.101", "x86_64")});
  EXPECT_EQ(ids_of(prepared), (std::vector<std::string>{"Google Chrome"}));
}

TEST(software_facts, two_installs_of_one_name_are_told_apart_by_version_then_architecture) {
  const std::vector<software_facts::package> prepared = software_facts::prepare(
      {make("python3", "3.12.3", "x86_64"), make("python3", "3.11.9", "x86_64"), make("zlib1g", "1.3", "x86_64"), make("zlib1g", "1.3", "x86")});
  EXPECT_EQ(ids_of(prepared), (std::vector<std::string>{"python3 3.11.9", "python3 3.12.3", "zlib1g 1.3 (x86)", "zlib1g 1.3 (x86_64)"}));
}

TEST(software_facts, two_products_whose_ids_would_collide_still_get_one_each) {
  // A product literally named "Widget 1.0" beside version 1.0 of one named
  // "Widget": no field tells them apart in an id, and the core rejects a whole
  // set over two records sharing one. A suffix is the cheaper answer.
  const std::vector<software_facts::package> prepared = software_facts::prepare({make("Widget", "1.0"), make("Widget", "2.0"), make("Widget 1.0", "1.0")});
  EXPECT_EQ(ids_of(prepared), (std::vector<std::string>{"Widget 1.0", "Widget 2.0", "Widget 1.0 #2"}));
}

TEST(software_facts, the_same_install_seen_twice_is_one_record) {
  // The same product in three user hives, and once machine-wide: one entry in
  // the host's inventory, and the machine-wide record is the one kept.
  software_facts::package user_a = make("Zoom", "6.1.0", "x86_64");
  user_a.scope = "user";
  software_facts::package user_b = user_a;
  software_facts::package machine = make("Zoom", "6.1.0", "x86_64");
  machine.scope = "machine";
  const std::vector<software_facts::package> prepared = software_facts::prepare({user_a, user_b, machine});
  ASSERT_EQ(prepared.size(), 1u);
  EXPECT_EQ(prepared[0].id, "Zoom");
  EXPECT_EQ(prepared[0].scope, "machine");
}

TEST(software_facts, a_record_carries_what_the_gather_knew_and_omits_what_it_did_not) {
  software_facts::package p = make("bash", "5.2.21-2ubuntu4", "x86_64");
  p.publisher = "Ubuntu Developers";
  p.source = "dpkg";
  p.install_date = 1758672000;  // 2025-09-24T00:00:00Z
  p.size_bytes = 1794048;
  nscapi::facts::response response;
  software_facts::publish({p, make("zlib1g")}, 1758672000, response);

  const PB::Facts::List list = installed_list(response);
  ASSERT_EQ(list.values_size(), 2);
  const PB::Facts::Value &bash = list.values(0);
  EXPECT_EQ(field_of(bash, "id"), "bash");
  EXPECT_EQ(field_of(bash, "name"), "bash");
  EXPECT_EQ(field_of(bash, "version"), "5.2.21-2ubuntu4");
  EXPECT_EQ(field_of(bash, "publisher"), "Ubuntu Developers");
  EXPECT_EQ(field_of(bash, "architecture"), "x86_64");
  EXPECT_EQ(field_of(bash, "source"), "dpkg");
  // A date, not a timestamp: nobody installs software at a time an inventory
  // needs to the second.
  EXPECT_EQ(field_of(bash, "install_date"), "2025-09-24");
  const PB::Facts::Value *size = nscapi::facts::tree::get(bash.object_value(), "size_bytes");
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->uint_value(), 1794048u);

  // What the gather did not know is absent, never present and empty.
  const PB::Facts::Value &zlib = list.values(1);
  EXPECT_EQ(field_of(zlib, "id"), "zlib1g");
  EXPECT_EQ(nscapi::facts::tree::get(zlib.object_value(), "version"), nullptr);
  EXPECT_EQ(nscapi::facts::tree::get(zlib.object_value(), "size_bytes"), nullptr);
  EXPECT_EQ(nscapi::facts::tree::get(zlib.object_value(), "install_date"), nullptr);
  EXPECT_EQ(error_for(response, software_facts::set_software), "");
}

TEST(software_facts, a_host_with_nothing_installed_still_publishes_the_list) {
  // "Collected, and there is nothing" is an answer; an absent list would read
  // as "not collected".
  nscapi::facts::response response;
  software_facts::publish({}, 1758672000, response);
  EXPECT_EQ(installed_list(response).values_size(), 0);
  EXPECT_EQ(error_for(response, software_facts::set_software), "");
}

TEST(software_facts, a_huge_inventory_is_truncated_and_says_so) {
  std::vector<software_facts::package> packages;
  for (std::size_t i = 0; i < software_facts::max_packages + 10; ++i) {
    // Zero-padded so sorting by name is sorting by number, which is what lets
    // the assertion below name the first record.
    std::string index = std::to_string(i);
    packages.push_back(make("package-" + std::string(6 - index.size(), '0') + index, "1.0"));
  }
  nscapi::facts::response response;
  software_facts::publish(packages, 1758672000, response);

  const PB::Facts::List list = installed_list(response);
  EXPECT_EQ(static_cast<std::size_t>(list.values_size()), software_facts::max_packages);
  EXPECT_EQ(field_of(list.values(0), "id"), "package-000000");
  // The set is published anyway: most of an inventory plus the reason it is
  // not all of it beats the core rejecting the set over the size budget.
  const std::string error = error_for(response, software_facts::set_software);
  EXPECT_NE(error.find(std::to_string(software_facts::max_packages + 10)), std::string::npos) << error;
  EXPECT_NE(error.find(std::to_string(software_facts::max_packages)), std::string::npos) << error;
}
