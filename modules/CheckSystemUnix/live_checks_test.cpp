// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The checks whose command reads this machine directly, run the way the
// module runs them. Their parsers and renderers are tested on captured input
// elsewhere; what is covered here is the part between the command and the
// kernel: that the reader finds its data, or that a machine without the
// hardware gets the documented no-data answer.
//
// Thresholds are pinned so the state says nothing about the host's health.

#include <gtest/gtest.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <string>
#include <vector>

#include "check_battery.h"
#include "check_hostname.h"
#include "check_load.h"
#include "check_os_version.h"
#include "check_temperature.h"
#include "check_test_support.hpp"
#include "check_uptime.h"

using check_test::contains;
using check_test::make_request;
using check_test::message_of;
using check_test::response_type;

// --- check_os_version ---------------------------------------------------------

TEST(CheckOsVersionCommand, ReportsTheRunningKernel) {
  utsname name{};
  ASSERT_EQ(0, uname(&name));

  response_type r;
  os_version::check_os_version(make_request("check_os_version", {}), &r);
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_EQ(0u, message_of(r).find("OK: ")) << message_of(r);
  EXPECT_TRUE(contains(message_of(r), std::string("(kernel ") + name.release + ")")) << message_of(r);
}

TEST(CheckOsVersionCommand, EveryKeywordResolves) {
  utsname name{};
  ASSERT_EQ(0, uname(&name));

  response_type r;
  os_version::check_os_version(
      make_request("check_os_version", {"top-syntax=${list}",
                                        "detail-syntax=${kernel_name}|${kernel_release}|${machine}|${processor}|${nodename}|${kernel_version}|${os}|"
                                        "${distribution}|${distribution_name}|${version}|${family}"}),
      &r);
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  const std::string expected = std::string(name.sysname) + "|" + name.release + "|" + name.machine + "|" + name.machine + "|" + name.nodename + "|";
  EXPECT_EQ(0u, message_of(r).find(expected)) << message_of(r);
}

TEST(CheckOsVersionCommand, KeywordsAreThresholdable) {
  utsname name{};
  ASSERT_EQ(0, uname(&name));

  response_type hit;
  os_version::check_os_version(make_request("check_os_version", {std::string("critical=kernel_name = '") + name.sysname + "'"}), &hit);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, hit.result()) << message_of(hit);

  response_type miss;
  os_version::check_os_version(make_request("check_os_version", {"critical=kernel_name = 'no-such-kernel'"}), &miss);
  EXPECT_EQ(PB::Common::ResultCode::OK, miss.result()) << message_of(miss);
}

TEST(CheckOsVersionCommand, ABadExpressionIsUnknown) {
  response_type r;
  os_version::check_os_version(make_request("check_os_version", {"warning=no_such_keyword = 1"}), &r);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
}

TEST(CheckOsVersion, ReadsAnOsReleaseFileAndToleratesAMissingOne) {
  const boost::filesystem::path file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-os-release-%%%%-%%%%");
  {
    std::ofstream out(file.string().c_str());
    out << "ID=debian\nNAME=\"Debian GNU/Linux\"\nVERSION_ID=\"12\"\nPRETTY_NAME=\"Debian GNU/Linux 12 (bookworm)\"\n";
  }
  const os_version::os_release_info info = os_version::read_os_release_from(file.string());
  boost::filesystem::remove(file);
  EXPECT_EQ("debian", info.distribution);
  EXPECT_EQ("Debian GNU/Linux", info.distribution_name);
  EXPECT_EQ("12", info.version);
  EXPECT_EQ("debian", info.family);
  EXPECT_EQ("Debian GNU/Linux 12 (bookworm)", info.pretty);

  // A missing file is an OS that does not say what it is, not an error.
  const os_version::os_release_info none = os_version::read_os_release_from(file.string());
  EXPECT_EQ("", none.distribution);
  EXPECT_EQ("", none.pretty);
}

// --- check_hostname -----------------------------------------------------------

TEST(CheckHostnameCommand, ReportsTheHostname) {
  char buf[256] = {0};
  ASSERT_EQ(0, gethostname(buf, sizeof(buf) - 1));

  response_type r;
  hostname_check::check_hostname(make_request("check_hostname", {"top-syntax=${list}", "detail-syntax=[${hostname}]"}), &r);
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  // The short name: everything before the first dot of what the kernel holds.
  const std::string full(buf);
  EXPECT_EQ("[" + full.substr(0, full.find('.')) + "]", message_of(r));
}

// --- check_load ---------------------------------------------------------------

TEST(CheckLoadCommand, ReportsTheLoadAverages) {
  response_type r;
  load_check::check_load(make_request("check_load", {"warning=load1 > 100000", "critical=load1 > 100000"}), &r);
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_TRUE(contains(message_of(r), "load average: ")) << message_of(r);
}

// --- check_uptime -------------------------------------------------------------

TEST(CheckUptime, TheMachineHasBeenUpForAWhile) {
  double seconds = 0;
  std::string error;
  ASSERT_TRUE(checks::read_uptime_seconds(seconds, error)) << error;
  EXPECT_GT(seconds, 0);
  EXPECT_EQ("", error);
}

// --- hardware that may not be there ---------------------------------------------

TEST(CheckTemperatureCommand, ReportsSensorsOrSaysThereAreNone) {
  response_type r;
  temperature_check::check_temperature(make_request("check_temperature", {"warning=temperature > 100000", "critical=temperature > 100000"}), &r);
  if (temperature_check::read_temperature().empty()) {
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
    EXPECT_EQ("No temperature sensors found", message_of(r));
  } else {
    EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
    EXPECT_EQ("OK: Temperature is ok.", message_of(r));
  }
}

TEST(CheckTemperatureCommand, ABadExpressionIsReportedBeforeTheSensorsAreRead) {
  response_type r;
  temperature_check::check_temperature(make_request("check_temperature", {"warning=no_such_keyword > 1"}), &r);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
  EXPECT_NE("No temperature sensors found", message_of(r));
}

TEST(TemperatureMetrics, OneSectionWithAnInstancePerZone) {
  PB::Metrics::MetricsBundle parent;
  temperature_check::build_temperature_metrics(&parent);
  // The section is there with or without sensors: an empty one says the
  // machine has none, a missing one would say the reader never ran.
  ASSERT_EQ(1, parent.children_size());
  const PB::Metrics::MetricsBundle &section = parent.children(0);
  EXPECT_EQ("temperature", section.key());
  EXPECT_EQ(static_cast<int>(temperature_check::read_temperature().size()), section.value_size());
  for (const auto &m : section.value()) {
    EXPECT_EQ("celsius", m.unit()) << m.key();
    ASSERT_EQ(1, m.dims_size()) << m.key();
    EXPECT_EQ("zone", m.dims(0).key());
  }
}

TEST(CheckBatteryCommand, ReportsABatteryOrTheNoBatteryContract) {
  response_type r;
  battery_check::check_battery(make_request("check_battery", {"warning=charge < -1", "critical=charge < -1"}), &r);
  // A machine without a battery is not broken: the check says so and is OK
  // unless the caller asked for something else with empty-state.
  EXPECT_NE(PB::Common::ResultCode::CRITICAL, r.result()) << message_of(r);
  EXPECT_FALSE(message_of(r).empty());
}

TEST(BatteryMetrics, TheSectionIsOnlyThereWhenABatteryIs) {
  PB::Metrics::MetricsBundle parent;
  battery_check::build_battery_metrics(&parent);
  if (battery_check::read_battery().empty()) {
    EXPECT_EQ(0, parent.children_size());
  } else {
    ASSERT_EQ(1, parent.children_size());
    EXPECT_EQ("battery", parent.children(0).key());
    EXPECT_GT(parent.children(0).value_size(), 0);
  }
}
