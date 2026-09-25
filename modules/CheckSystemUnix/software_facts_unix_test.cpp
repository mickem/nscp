// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/software_facts.hpp>
#include <set>
#include <string>
#include <vector>

#include "check_installed_software.h"

// The live gather, on whatever host runs the tests. What it finds depends on
// the host - and on a host with no package manager it is an error rather than
// an empty list - so this asserts the rules every record has to follow
// wherever it runs.

namespace {
bool has_package_manager() { return !installed_software::detect_manager().empty(); }
}  // namespace

TEST(software_facts_unix, a_host_without_a_package_manager_says_so_rather_than_reporting_nothing) {
  if (has_package_manager()) return;
  // An empty list would claim this host has no software installed, which is
  // never what "I cannot read the package database" means.
  EXPECT_THROW(software_facts::gather(), std::exception);
}

TEST(software_facts_unix, every_record_follows_the_document_rules) {
  if (!has_package_manager()) return;
  const std::vector<software_facts::package> packages = software_facts::prepare(software_facts::gather());
  // A host with a package manager has packages: the agent's own dependencies
  // are among them.
  EXPECT_GT(packages.size(), 0u);
  std::set<std::string> ids;
  for (const software_facts::package &p : packages) {
    EXPECT_FALSE(p.id.empty());
    EXPECT_TRUE(ids.insert(p.id).second) << "duplicate id " << p.id;
    EXPECT_FALSE(p.name.empty());
    // The manager the record came from, which is the one detect_manager found.
    EXPECT_EQ(p.source, installed_software::detect_manager().name) << p.id;
    // A unix package is installed for the machine; `scope` is a Windows hive
    // distinction and is not published here.
    EXPECT_EQ(p.scope, "") << p.id;
    if (!p.architecture.empty()) {
      EXPECT_EQ(p.architecture, software_facts::normalize_architecture(p.architecture)) << p.id;
    }
  }
}

TEST(software_facts_unix, the_same_host_is_the_same_list) {
  if (!has_package_manager()) return;
  // Sorted and deduplicated, so two reads of an unchanged host compare equal -
  // which is what keeps the document's revision from moving every round.
  const std::vector<software_facts::package> first = software_facts::prepare(software_facts::gather());
  const std::vector<software_facts::package> second = software_facts::prepare(software_facts::gather());
  ASSERT_EQ(first.size(), second.size());
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_EQ(first[i].id, second[i].id);
    EXPECT_EQ(first[i].version, second[i].version) << first[i].id;
    if (i > 0) {
      EXPECT_LE(first[i - 1].name, first[i].name);
    }
  }
}
