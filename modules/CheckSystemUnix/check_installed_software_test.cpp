// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_installed_software.h"

#include <gtest/gtest.h>

#include <ctime>

using installed_software::software_entry;

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

software_entry make_entry(const std::string &name, const std::string &version, const std::string &publisher, long long install_date_epoch = 0) {
  software_entry e;
  e.name = name;
  e.version = version;
  e.publisher = publisher;
  e.manager = "dpkg";
  e.architecture = "amd64";
  e.install_date_epoch = install_date_epoch;
  return e;
}

std::vector<software_entry> sample_entries() {
  return {make_entry("good-app", "1.2.3", "Example Corp"), make_entry("openjdk-7-jre", "7u51", "Debian Java Maintainers")};
}

PB::Common::ResultCode run_check(const std::vector<software_entry> &entries, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_installed_software");
  for (const std::string &a : args) request.add_arguments(a);
  installed_software::check_from(request, &response, entries);
  return response.result();
}

}  // namespace

// --- parsers -----------------------------------------------------------------

TEST(CheckInstalledSoftware, ParsesDpkgOutput) {
  const std::string output =
      "bash\t5.1-6ubuntu1\tamd64\tUbuntu Developers <ubuntu-devel-discuss@lists.ubuntu.com>\t1864\tinstall ok installed\t1717200000\n"
      "removed-pkg\t1.0\tamd64\tSomeone <x@y.z>\t10\tdeinstall ok config-files\t1717200000\n"
      "libc6\t2.35-0ubuntu3\tamd64\tUbuntu Developers <ubuntu-devel-discuss@lists.ubuntu.com>\t13597\tinstall ok installed\t1700000000\n";
  const std::vector<software_entry> entries = installed_software::parse_dpkg_output(output);
  ASSERT_EQ(entries.size(), 2u);  // config-files leftover is skipped
  EXPECT_EQ(entries[0].name, "bash");
  EXPECT_EQ(entries[0].version, "5.1-6ubuntu1");
  EXPECT_EQ(entries[0].architecture, "amd64");
  EXPECT_EQ(entries[0].publisher, "Ubuntu Developers");  // email stripped
  EXPECT_EQ(entries[0].size_bytes, 1864LL * 1024);
  EXPECT_EQ(entries[0].manager, "dpkg");
  EXPECT_EQ(entries[0].install_date_epoch, 1717200000);
  EXPECT_EQ(entries[0].install_date_str, "2024-06-01");
  EXPECT_EQ(entries[1].name, "libc6");
  EXPECT_EQ(entries[1].install_date_epoch, 1700000000);
}

TEST(CheckInstalledSoftware, SkipsNonInstalledDpkgStates) {
  // Every dpkg state that is not exactly "installed" is a package that is not
  // (fully) on disk — including the two that end in the word "installed".
  const std::string output =
      "purged-pkg\t1.0\tamd64\tX\t0\tunknown ok not-installed\n"
      "broken-pkg\t1.0\tamd64\tX\t0\tinstall ok half-installed\n"
      "unpacked-pkg\t1.0\tamd64\tX\t0\tinstall ok unpacked\n"
      "held-pkg\t2.0\tamd64\tX\t7\thold ok installed\n";
  const std::vector<software_entry> entries = installed_software::parse_dpkg_output(output);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].name, "held-pkg");  // held packages are still installed
}

TEST(CheckInstalledSoftware, FetchInstalledPropagatesCommandFailure) {
  installed_software::package_manager pm;
  pm.name = "dpkg";
  pm.binary = "/usr/bin/dpkg-query";

  // A failed query must not be reported as an empty package database.
  const installed_software::fetch_result failed =
      installed_software::fetch_installed(pm, [](const std::string &) { return installed_software::command_result("", false); });
  EXPECT_FALSE(failed.ok);
  EXPECT_TRUE(failed.entries.empty());

  const installed_software::fetch_result ok = installed_software::fetch_installed(pm, [](const std::string &cmd) {
    // Commands are invoked by absolute path, never by bare name.
    EXPECT_EQ(cmd.compare(0, 20, "/usr/bin/dpkg-query "), 0) << cmd;
    return installed_software::command_result("bash\t5.1\tamd64\tX\t1\tinstall ok installed\n", true);
  });
  EXPECT_TRUE(ok.ok);
  ASSERT_EQ(ok.entries.size(), 1u);
  EXPECT_EQ(ok.entries[0].name, "bash");
}

TEST(CheckInstalledSoftware, ParsesRpmOutput) {
  const std::string output =
      "bash\t5.2.26-3.fc40\tx86_64\tFedora Project\t8654321\t1717200000\n"
      "gpg-pubkey\ta15b79cc-63d04c2c\t(none)\t(none)\t0\t1717000000\n";
  const std::vector<software_entry> entries = installed_software::parse_rpm_output(output);
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].name, "bash");
  EXPECT_EQ(entries[0].version, "5.2.26-3.fc40");
  EXPECT_EQ(entries[0].architecture, "x86_64");
  EXPECT_EQ(entries[0].publisher, "Fedora Project");
  EXPECT_EQ(entries[0].size_bytes, 8654321);
  EXPECT_EQ(entries[0].install_date_epoch, 1717200000);
  EXPECT_EQ(entries[0].install_date_str, "2024-06-01");
  EXPECT_EQ(entries[1].publisher, "");  // "(none)" is normalised away
}

TEST(CheckInstalledSoftware, ParsesPacmanOutput) {
  const std::vector<software_entry> entries = installed_software::parse_pacman_output("bash 5.2.026-2\nlinux 6.9.3.arch1-1\n");
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].name, "bash");
  EXPECT_EQ(entries[0].version, "5.2.026-2");
  EXPECT_EQ(entries[0].manager, "pacman");
}

// A dpkg-query older than 1.19.3 does not know db-fsys:Last-Modified and leaves
// the column out (or empty): the entry keeps its unknown date rather than being
// dropped.
TEST(CheckInstalledSoftware, DpkgEntriesSurviveAMissingInstallDate) {
  const std::vector<software_entry> entries = installed_software::parse_dpkg_output(
      "bash\t5.1\tamd64\tX\t1\tinstall ok installed\n"
      "libc6\t2.35\tamd64\tX\t1\tinstall ok installed\t\n");
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].install_date_epoch, 0);
  EXPECT_EQ(entries[0].install_date_str, "");
  EXPECT_EQ(entries[1].install_date_epoch, 0);
  EXPECT_EQ(entries[1].install_date_str, "");
}

// --- check_from --------------------------------------------------------------

TEST(CheckInstalledSoftware, DefaultIsOkInventoryWithCount) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("2 software packages installed"), std::string::npos) << join_lines(response);
  EXPECT_NE(perf_of(response).find("'count'=2"), std::string::npos) << perf_of(response);
}

TEST(CheckInstalledSoftware, UnwantedSoftwareByNameIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"crit=name like 'openjdk-7'"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("openjdk-7-jre"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, AbsentUnwantedSoftwareIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"crit=name like 'bittorrent'"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckInstalledSoftware, RecentInstallTripsDateExpression) {
  std::vector<software_entry> entries = sample_entries();
  entries[0].install_date_epoch = static_cast<long long>(std::time(nullptr)) - 86400;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(entries, {"warn=install_date > -7d"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("good-app"), std::string::npos) << join_lines(response);
}

TEST(CheckInstalledSoftware, OldInstallDoesNotTripDateExpression) {
  std::vector<software_entry> entries = sample_entries();
  entries[0].install_date_epoch = static_cast<long long>(std::time(nullptr)) - 90LL * 86400;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(entries, {"warn=install_date > -7d"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckInstalledSoftware, PackageStatusKeywordAndDeprecatedStatusAliasBothParse) {
  // "status" is a deprecated alias for "package_status" (renamed to avoid
  // clashing with the generic status summary keyword); both must parse.
  for (const std::string &keyword : {std::string("package_status"), std::string("status")}) {
    PB::Commands::QueryResponseMessage::Response response;
    EXPECT_EQ(run_check(sample_entries(), {"filter=" + keyword + " = 'installed'"}, response), PB::Common::ResultCode::OK)
        << keyword << ": " << join_lines(response);
    EXPECT_NE(join_lines(response).find("2 software packages installed"), std::string::npos) << keyword << ": " << join_lines(response);
  }
}

TEST(CheckInstalledSoftware, EmptyMatchSetTakesEmptyState) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_entries(), {"filter=name like 'zz_no_such_package_zz'"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No installed software found"), std::string::npos) << join_lines(response);
}
