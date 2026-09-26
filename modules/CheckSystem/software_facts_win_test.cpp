// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/software_facts.hpp>
#include <set>
#include <string>
#include <vector>

#include "check_installed_software.hpp"

// The live gather, on whatever host runs the tests. What it finds depends on
// the host, so this asserts the rules every record has to follow wherever it
// runs - and the two that tie the fact set to the check it shares a source
// with.

TEST(software_facts_win, every_record_follows_the_document_rules) {
  const std::vector<software_facts::package> packages = software_facts::prepare(software_facts::gather());
  // A Windows host always has something in the Uninstall hives.
  EXPECT_GT(packages.size(), 0u);
  std::set<std::string> ids;
  for (const software_facts::package &p : packages) {
    EXPECT_FALSE(p.id.empty());
    EXPECT_TRUE(ids.insert(p.id).second) << "duplicate id " << p.id;
    EXPECT_FALSE(p.name.empty());
    EXPECT_EQ(p.source, "registry") << p.id;
    // Which hive it came out of, in the one vocabulary the document uses.
    EXPECT_TRUE(p.scope == "machine" || p.scope == "user") << p.id << ": " << p.scope;
    if (!p.architecture.empty()) EXPECT_TRUE(p.architecture == "x86_64" || p.architecture == "x86") << p.id << ": " << p.architecture;
  }
}

TEST(software_facts_win, reports_what_check_installed_software_reports_and_not_its_hidden_entries) {
  const std::vector<installed_software_check::software_entry> entries = installed_software_check::gather_installed_software();
  std::set<std::string> visible;
  std::set<std::string> hidden_only;
  for (const installed_software_check::software_entry &e : entries) {
    if (e.name.empty()) continue;
    if (e.system_component) {
      hidden_only.insert(e.name);
    } else {
      visible.insert(e.name);
    }
  }
  for (const std::string &name : visible) hidden_only.erase(name);

  std::set<std::string> published;
  for (const software_facts::package &p : software_facts::gather()) published.insert(p.name);
  // The fact set is the list Programs and Features shows: every visible entry
  // the check reports, and none of the SystemComponent bookkeeping keys it
  // also knows about.
  EXPECT_EQ(published, visible);
  for (const std::string &name : hidden_only) EXPECT_EQ(published.count(name), 0u) << name;
}
