// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_installed_software.hpp"

#include <gtest/gtest.h>

#include <ctime>

using installed_software_check::software_entry;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::string perf_of(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      const auto &p = r.lines(i).perf(j);
      if (!out.empty()) out += " ";
      out += "'" + p.alias() + "'=";
      if (p.has_float_value()) out += std::to_string(static_cast<long long>(p.float_value().value()));
    }
  }
  return out;
}

software_entry make_entry(const std::string &name, const std::string &version, const std::string &publisher, long long install_date_epoch = 0,
                          bool system_component = false) {
  software_entry e;
  e.name = name;
  e.version = version;
  e.publisher = publisher;
  e.install_date_epoch = install_date_epoch;
  e.hive = "machine";
  e.architecture = "x64";
  e.key = name;
  e.system_component = system_component;
  return e;
}

std::vector<software_entry> sample_entries() {
  return {
      make_entry("Good App", "1.2.3", "Example Corp"),
      make_entry("Old Java", "7.0.51", "Oracle"),
      // A driver-style SystemComponent entry: hidden by the default filter.
      make_entry("Runtime Component", "10.0", "Example Corp", 0, true),
  };
}

PB::Common::ResultCode run_check(const std::vector<software_entry> &entries, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_installed_software");
  for (const std::string &a : args) request.add_arguments(a);
  installed_software_check::check_from(request, &response, entries);
  return response.result();
}

}  // namespace

TEST(CheckInstalledSoftware, DefaultIsOkInventoryWithCount) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  // The SystemComponent entry is excluded by the default filter.
  EXPECT_NE(join_lines(response).find("2 software packages installed"), std::string::npos) << join_lines(response);
  EXPECT_NE(perf_of(response).find("'count'=2"), std::string::npos) << perf_of(response);
}

TEST(CheckInstalledSoftware, FilterOverrideIncludesSystemComponents) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"filter=none"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("3 software packages installed"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, UnwantedSoftwareByNameIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"crit=name like 'Java'"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Old Java 7.0.51 (Oracle)"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, AbsentUnwantedSoftwareIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"crit=name like 'BitTorrent'"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckInstalledSoftware, RecentInstallTripsDateExpression) {
  std::vector<software_entry> entries = sample_entries();
  // "Good App" was installed yesterday; the rest have no recorded date (epoch 0,
  // which date expressions never match).
  entries[0].install_date_epoch = static_cast<long long>(std::time(nullptr)) - 86400;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(entries, {"warn=install_date > -7d"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Good App"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, OldInstallDoesNotTripDateExpression) {
  std::vector<software_entry> entries = sample_entries();
  entries[0].install_date_epoch = static_cast<long long>(std::time(nullptr)) - 90LL * 86400;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(entries, {"warn=install_date > -7d"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckInstalledSoftware, HiveAndUserKeywordsFilterPerUserInstalls) {
  std::vector<software_entry> entries = sample_entries();
  software_entry user_app = make_entry("User Tool", "2.0", "Someone");
  user_app.hive = "user";
  user_app.user = "EXAMPLE\\jdoe";
  user_app.architecture = "";
  entries.push_back(user_app);

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(entries, {"filter=hive = 'user'", "warn=name like 'User'"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("User Tool"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, EmptyMatchSetTakesEmptyState) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"filter=name like 'zz_no_such_product_zz'"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No installed software found"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, NoSoftwareAtAllTakesEmptyState) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({}, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No installed software found"), std::string::npos) << join_lines(response);
}
