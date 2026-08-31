// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_cpu_frequency.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <string>
#include <vector>

using cpu_frequency_check::cpu_frequency;
using cpu_frequency_check::cpus_type;

namespace {

namespace fs = boost::filesystem;

// A scratch /sys/devices/system/cpu look-alike, deleted on teardown.
struct sysfs_fixture {
  fs::path base;
  sysfs_fixture() {
    base = fs::temp_directory_path() / fs::unique_path("nscp-cpufreq-%%%%-%%%%");
    fs::create_directories(base);
  }
  ~sysfs_fixture() {
    boost::system::error_code ec;
    fs::remove_all(base, ec);
  }
  void write(const std::string &rel, const std::string &content) const {
    const fs::path p = base / rel;
    fs::create_directories(p.parent_path());
    std::ofstream os(p.string().c_str());
    os << content;
  }
  void mkdir(const std::string &rel) const { fs::create_directories(base / rel); }
};

const cpu_frequency *find_cpu(const cpus_type &data, const std::string &name) {
  for (const cpu_frequency &c : data) {
    if (c.name == name) return &c;
  }
  return nullptr;
}

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

cpu_frequency make_cpu(const std::string &name, long long current, long long max, long long min) {
  cpu_frequency c;
  c.name = name;
  c.current_mhz = current;
  c.max_mhz = max;
  c.min_mhz = min;
  return c;
}

PB::Common::ResultCode run(const cpus_type &data, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_cpu_frequency");
  for (const std::string &a : args) request.add_arguments(a);
  cpu_frequency_check::check_cpu_frequency_evaluate(request, &response, data);
  return response.result();
}

}  // namespace

// ---- cpu_frequency struct ----------------------------------------------------

TEST(CheckCpuFrequency, FrequencyPctFromCurrentAndMax) {
  const cpu_frequency half = make_cpu("cpu0", 1800, 3600, 400);
  EXPECT_EQ(half.get_name(), "cpu0");
  EXPECT_EQ(half.get_current_mhz(), 1800);
  EXPECT_EQ(half.get_max_mhz(), 3600);
  EXPECT_EQ(half.get_min_mhz(), 400);
  EXPECT_EQ(half.get_frequency_pct(), 50);
  EXPECT_EQ(half.show(), "cpu0");

  // Unknown max frequency must read as 0%, not divide by zero.
  const cpu_frequency no_max = make_cpu("cpu1", 1800, 0, 0);
  EXPECT_EQ(no_max.get_frequency_pct(), 0);
}

// ---- read_cpu_frequency against a fixture sysfs tree -------------------------

TEST(CheckCpuFrequency, ReadsScalingCurFreqInMhz) {
  sysfs_fixture sys;
  sys.write("cpu0/cpufreq/scaling_cur_freq", "1800000\n");
  sys.write("cpu0/cpufreq/cpuinfo_max_freq", "3600000\n");
  sys.write("cpu0/cpufreq/cpuinfo_min_freq", "400000\n");

  const cpus_type data = cpu_frequency_check::read_cpu_frequency(sys.base.string());
  ASSERT_EQ(data.size(), 2u);  // cpu0 + synthetic total

  const cpu_frequency *cpu0 = find_cpu(data, "cpu0");
  ASSERT_NE(cpu0, nullptr);
  EXPECT_EQ(cpu0->current_mhz, 1800);
  EXPECT_EQ(cpu0->max_mhz, 3600);
  EXPECT_EQ(cpu0->min_mhz, 400);
  EXPECT_EQ(cpu0->get_frequency_pct(), 50);
}

TEST(CheckCpuFrequency, FallsBackToCpuinfoCurFreq) {
  // Hosts where scaling_cur_freq is absent still expose cpuinfo_cur_freq.
  sysfs_fixture sys;
  sys.write("cpu0/cpufreq/cpuinfo_cur_freq", "2400000\n");

  const cpus_type data = cpu_frequency_check::read_cpu_frequency(sys.base.string());
  const cpu_frequency *cpu0 = find_cpu(data, "cpu0");
  ASSERT_NE(cpu0, nullptr);
  EXPECT_EQ(cpu0->current_mhz, 2400);
  // No max/min files: stay 0 and pct reads 0.
  EXPECT_EQ(cpu0->max_mhz, 0);
  EXPECT_EQ(cpu0->min_mhz, 0);
  EXPECT_EQ(cpu0->get_frequency_pct(), 0);
}

TEST(CheckCpuFrequency, SkipsPseudoEntriesAndUnreadableCores) {
  sysfs_fixture sys;
  // Real core.
  sys.write("cpu0/cpufreq/scaling_cur_freq", "1000000\n");
  // Pseudo-entries that also start with "cpu" must be ignored.
  sys.mkdir("cpuidle");
  sys.mkdir("cpufreq");
  sys.write("cpu_capacity", "1024\n");
  // A cpu directory without a cpufreq subdir (VM without cpufreq support).
  sys.mkdir("cpu1");
  // A cpufreq dir whose current-frequency file holds garbage: the core is
  // skipped rather than reported as a bogus 0 MHz.
  sys.write("cpu2/cpufreq/scaling_cur_freq", "notanumber\n");

  const cpus_type data = cpu_frequency_check::read_cpu_frequency(sys.base.string());
  ASSERT_EQ(data.size(), 2u) << "expected only cpu0 + total";
  EXPECT_NE(find_cpu(data, "cpu0"), nullptr);
  EXPECT_NE(find_cpu(data, "total"), nullptr);
  EXPECT_EQ(find_cpu(data, "cpu1"), nullptr);
  EXPECT_EQ(find_cpu(data, "cpu2"), nullptr);
}

TEST(CheckCpuFrequency, TotalIsTheAverageAcrossCores) {
  sysfs_fixture sys;
  sys.write("cpu0/cpufreq/scaling_cur_freq", "1000000\n");
  sys.write("cpu0/cpufreq/cpuinfo_max_freq", "4000000\n");
  sys.write("cpu0/cpufreq/cpuinfo_min_freq", "400000\n");
  sys.write("cpu1/cpufreq/scaling_cur_freq", "3000000\n");
  sys.write("cpu1/cpufreq/cpuinfo_max_freq", "4000000\n");
  sys.write("cpu1/cpufreq/cpuinfo_min_freq", "400000\n");

  const cpus_type data = cpu_frequency_check::read_cpu_frequency(sys.base.string());
  ASSERT_EQ(data.size(), 3u);
  const cpu_frequency *total = find_cpu(data, "total");
  ASSERT_NE(total, nullptr);
  EXPECT_EQ(total->current_mhz, 2000);
  EXPECT_EQ(total->max_mhz, 4000);
  EXPECT_EQ(total->min_mhz, 400);
  EXPECT_EQ(total->get_frequency_pct(), 50);
}

TEST(CheckCpuFrequency, MissingBaseDirectoryYieldsEmptyData) {
  const cpus_type data = cpu_frequency_check::read_cpu_frequency("/nonexistent/nscp-cpufreq-test");
  EXPECT_TRUE(data.empty());
}

// ---- check evaluation --------------------------------------------------------

TEST(CheckCpuFrequency, EmptyDataReportsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run({}, {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No CPU frequency data"), std::string::npos) << join_lines(response);
}

TEST(CheckCpuFrequency, DefaultCheckIsOkWithoutThresholds) {
  PB::Commands::QueryResponseMessage::Response response;
  const cpus_type data = {make_cpu("cpu0", 1800, 3600, 400), make_cpu("total", 1800, 3600, 400)};
  EXPECT_EQ(run(data, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckCpuFrequency, FrequencyPctThresholdTrips) {
  PB::Commands::QueryResponseMessage::Response response;
  const cpus_type data = {make_cpu("cpu0", 3600, 3600, 400)};  // pinned at 100%
  EXPECT_EQ(run(data, {"critical=frequency_pct > 90"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("cpu0"), std::string::npos) << join_lines(response);
}

TEST(CheckCpuFrequency, WarningOnLowCurrentMhz) {
  PB::Commands::QueryResponseMessage::Response response;
  const cpus_type data = {make_cpu("cpu0", 400, 3600, 400)};
  EXPECT_EQ(run(data, {"warning=current_mhz < 500"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckCpuFrequency, DefaultFilterExcludesTheTotalAggregate) {
  // The synthetic "total" row runs at 100% here; only the real core (25%) is
  // matched by the default filter, so no threshold trips.
  PB::Commands::QueryResponseMessage::Response response;
  const cpus_type data = {make_cpu("cpu0", 1000, 4000, 400), make_cpu("total", 3600, 3600, 400)};
  EXPECT_EQ(run(data, {"critical=frequency_pct > 90"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

// ---- metrics -----------------------------------------------------------------

TEST(CheckCpuFrequency, BuildMetricsEmitsPerCoreValues) {
  PB::Metrics::MetricsBundle parent;
  const cpus_type data = {make_cpu("cpu0", 1800, 3600, 400), make_cpu("total", 1800, 3600, 400)};
  cpu_frequency_check::build_cpu_frequency_metrics(&parent, data);

  ASSERT_EQ(parent.children_size(), 1);
  const PB::Metrics::MetricsBundle &bundle = parent.children(0);
  EXPECT_EQ(bundle.key(), "cpu_frequency");
  ASSERT_EQ(bundle.value_size(), 6);  // 3 metrics per entry

  bool saw_current = false;
  for (int i = 0; i < bundle.value_size(); ++i) {
    if (bundle.value(i).key() == "cpu0.current_mhz") {
      saw_current = true;
      EXPECT_DOUBLE_EQ(bundle.value(i).gauge_value().value(), 1800.0);
    }
  }
  EXPECT_TRUE(saw_current);
}
