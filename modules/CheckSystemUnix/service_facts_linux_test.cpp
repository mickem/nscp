// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "service_facts_linux.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

#include <stdexcept>

#include "check_service.h"

TEST(service_facts_linux, discovers_unloaded_disabled_templates_and_transient_instances) {
  int calls = 0;
  const auto services = service_facts::gather_systemd([&](const std::vector<std::string> &argv) {
    ++calls;
    if (argv[0] == "list-unit-files") return std::string("backup.service disabled enabled\nworker@.service static -\n");
    if (argv[0] == "list-units") return std::string("worker@night.service loaded inactive dead Worker\nmissing.service not-found inactive dead Missing\n");
    EXPECT_EQ(argv[0], "show");
    EXPECT_EQ(argv[2], "--property=Id,Description,UnitFileState,LoadState");
    EXPECT_EQ(argv[4], "backup.service");
    EXPECT_EQ(argv[5], "worker@night.service");
    return std::string(
        "Id=backup.service\nDescription=Backups\nUnitFileState=disabled\nLoadState=loaded\n\n"
        "Id=worker@night.service\nDescription=Worker\nUnitFileState=transient\nLoadState=loaded\n");
  });
  EXPECT_EQ(calls, 3);
  ASSERT_EQ(services.size(), 3u);
  EXPECT_EQ(services[0].name, "worker@");
  EXPECT_EQ(services[0].start_type, "static");
  EXPECT_EQ(services[1].name, "backup");
  EXPECT_EQ(services[1].display_name, "Backups");
  EXPECT_EQ(services[1].start_type, "disabled");
  EXPECT_EQ(services[2].name, "worker@night");
}

TEST(service_facts_linux, empty_success_is_distinct_from_failed_or_incomplete_queries) {
  EXPECT_TRUE(service_facts::gather_systemd([](const auto &) { return std::string(); }).empty());
  EXPECT_THROW(service_facts::gather_systemd([](const auto &) -> std::string { throw std::runtime_error("no systemd"); }), std::runtime_error);
  EXPECT_THROW(service_facts::gather_systemd([](const auto &argv) {
                 if (argv[0] == "list-unit-files") return std::string("demo.service disabled\n");
                 return std::string();
               }),
               std::runtime_error);
  EXPECT_THROW(service_facts::gather_systemd([](const auto &argv) {
                 if (argv[0] == "list-unit-files") return std::string("demo.service disabled\n");
                 if (argv[0] == "list-units") return std::string();
                 return std::string("Id=demo.service\nLoadState=not-found\n");
               }),
               std::runtime_error);
}

TEST(service_facts_linux, show_queries_are_batched) {
  int shows = 0;
  const auto services = service_facts::gather_systemd([&](const auto &argv) {
    std::string output;
    if (argv[0] == "list-unit-files") {
      for (int i = 0; i < 260; ++i) output += "svc" + std::to_string(i) + ".service disabled\n";
    } else if (argv[0] == "show") {
      ++shows;
      EXPECT_LE(argv.size(), 132u);
      for (std::size_t i = 4; i < argv.size(); ++i) output += "Id=" + argv[i] + "\nLoadState=loaded\nUnitFileState=disabled\n\n";
    }
    return output;
  });
  EXPECT_EQ(shows, 3);
  EXPECT_EQ(services.size(), 260u);
}

TEST(service_facts_linux, live_systemd_inventory) {
  if (access("/run/systemd/system", F_OK) != 0) GTEST_SKIP() << "This host does not run a systemd system manager";
  const auto services = service_facts::gather();
  EXPECT_FALSE(services.empty());
  for (const auto &service : services) {
    EXPECT_FALSE(service.name.empty());
    EXPECT_EQ(service.name.find('\n'), std::string::npos);
  }
}
