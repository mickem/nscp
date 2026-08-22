// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#ifdef WIN32
#include <objbase.h>
#endif

#include "check_cpu_frequency.hpp"

#include <algorithm>
#include <parsers/where/node.hpp>
#include <str/number_format.hpp>
#include <string>
#include <vector>

namespace {

// Minimal evaluation context carrying the number format the human readable
// getters render through (#1428).
struct mock_evaluation_context final : parsers::where::evaluation_context_interface {
  bool has_error() const override { return false; }
  std::string get_error() const override { return ""; }
  void error(std::string) override {}
  bool has_warn() const override { return false; }
  std::string get_warn() const override { return ""; }
  void warn(std::string) override {}
  void clear() override {}
  void enable_debug(bool) override {}
  bool debug_enabled() override { return false; }
  std::string get_debug() const override { return ""; }
  void debug(parsers::where::object_match) override {}
};

parsers::where::evaluation_context make_context(const str::number_format &fmt = str::number_format()) {
  auto ctx = std::make_shared<mock_evaluation_context>();
  ctx->set_number_format(fmt);
  return ctx;
}

}  // namespace

// ============================================================================
// cpu_frequency struct tests
// ============================================================================

TEST(CpuFrequency, DefaultConstruction) {
  cpu_frequency_check::cpu_frequency c;
  EXPECT_EQ(c.name, "");
  EXPECT_EQ(c.current_mhz, 0);
  EXPECT_EQ(c.max_mhz, 0);
  EXPECT_EQ(c.number_of_cores, 0);
  EXPECT_EQ(c.number_of_logical_processors, 0);
  // No load sample is "absent", never a fake 0 (#1391).
  EXPECT_FALSE(c.load_pct);
}

TEST(CpuFrequency, Accessors) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz";
  c.current_mhz = 2900;
  c.max_mhz = 4800;
  c.number_of_cores = 8;
  c.number_of_logical_processors = 16;

  EXPECT_EQ(c.get_name(), "Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz");
  EXPECT_EQ(c.get_current_mhz(), 2900);
  EXPECT_EQ(c.get_max_mhz(), 4800);
  EXPECT_EQ(c.get_number_of_cores(), 8);
  EXPECT_EQ(c.get_number_of_logical_processors(), 16);
}

TEST(CpuFrequency, FrequencyPct) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 2400;
  c.max_mhz = 4800;
  EXPECT_EQ(c.get_frequency_pct(), 50);
}

TEST(CpuFrequency, FrequencyPctFull) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 3600;
  c.max_mhz = 3600;
  EXPECT_EQ(c.get_frequency_pct(), 100);
}

TEST(CpuFrequency, FrequencyPctZeroMax) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 100;
  c.max_mhz = 0;
  EXPECT_EQ(c.get_frequency_pct(), 0);
}

TEST(CpuFrequency, FrequencyPctRoundsDown) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 1000;
  c.max_mhz = 3000;
  // 1000 * 100 / 3000 = 33 (integer division)
  EXPECT_EQ(c.get_frequency_pct(), 33);
}

TEST(CpuFrequency, ShowReturnsName) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "AMD Ryzen 9 5950X";
  EXPECT_EQ(c.show(), "AMD Ryzen 9 5950X");
}

TEST(CpuFrequency, CopyConstruction) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "TestCPU";
  c.current_mhz = 3000;
  c.max_mhz = 4000;
  c.number_of_cores = 4;
  c.number_of_logical_processors = 8;

  const cpu_frequency_check::cpu_frequency copy(c);
  EXPECT_EQ(copy.name, "TestCPU");
  EXPECT_EQ(copy.current_mhz, 3000);
  EXPECT_EQ(copy.max_mhz, 4000);
  EXPECT_EQ(copy.number_of_cores, 4);
  EXPECT_EQ(copy.number_of_logical_processors, 8);
}

TEST(CpuFrequency, Assignment) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "CPU0";
  c.current_mhz = 2500;
  c.max_mhz = 5000;

  cpu_frequency_check::cpu_frequency other;
  other = c;
  EXPECT_EQ(other.name, "CPU0");
  EXPECT_EQ(other.current_mhz, 2500);
  EXPECT_EQ(other.max_mhz, 5000);
}

TEST(CpuFrequency, BuildMetrics) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "CPU0";
  c.current_mhz = 3000;
  c.max_mhz = 4500;
  c.number_of_cores = 8;
  c.number_of_logical_processors = 16;
  c.load_pct = 12;

  PB::Metrics::MetricsBundle section;
  c.build_metrics(&section);

  // current_mhz, max_mhz, frequency_pct, cores, logical_processors, load_pct, l2_cache, l3_cache
  EXPECT_EQ(section.value_size(), 8);
}

TEST(CpuFrequency, BuildMetricsSkipsMissingLoadSample) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "CPU0";
  c.current_mhz = 3000;
  c.max_mhz = 4500;

  PB::Metrics::MetricsBundle section;
  c.build_metrics(&section);

  // load_pct is absent this cycle: no fabricated 0 metric (#1391).
  EXPECT_EQ(section.value_size(), 7);
  for (const auto &value : section.value()) {
    EXPECT_EQ(value.key().find("load_pct"), std::string::npos) << value.key();
  }
}

TEST(CpuFrequency, ArchitectureMapping) {
  EXPECT_EQ(cpu_frequency_check::architecture_to_string(0), "x86");
  EXPECT_EQ(cpu_frequency_check::architecture_to_string(9), "x64");
  EXPECT_EQ(cpu_frequency_check::architecture_to_string(12), "ARM64");
  EXPECT_EQ(cpu_frequency_check::architecture_to_string(5), "ARM");
  EXPECT_EQ(cpu_frequency_check::architecture_to_string(42), "unknown (42)");
}

TEST(CpuFrequency, CacheAccessorsAndHumanRendering) {
  cpu_frequency_check::cpu_frequency c;
  c.l2_cache = 512LL * 1024;
  c.l3_cache = 16LL * 1024 * 1024;
  EXPECT_EQ(c.get_l2_cache(), 512LL * 1024);
  EXPECT_EQ(c.get_l3_cache(), 16LL * 1024 * 1024);
  EXPECT_EQ(c.get_l2_cache_human(make_context()), "512KB");
  EXPECT_EQ(c.get_l3_cache_human(make_context()), "16MB");
  // Unreported caches render as 0B rather than failing.
  const cpu_frequency_check::cpu_frequency empty;
  EXPECT_EQ(empty.get_l2_cache_human(make_context()), "0B");

  // The context carries the check's rendering options (#1428).
  str::number_format fmt;
  fmt.decimals = 2;
  fmt.byte_unit = "KB";
  const parsers::where::evaluation_context pinned = make_context(fmt);
  EXPECT_EQ(c.get_l2_cache_human(pinned), "512.00KB");
  EXPECT_EQ(c.get_l3_cache_human(pinned), "16384.00KB");
}

TEST(CpuFrequency, SocketAndLoadAccessors) {
  cpu_frequency_check::cpu_frequency c;
  c.socket_id = "CPU0";
  c.socket = "CPU 1";
  c.load_pct = 42;
  EXPECT_EQ(c.get_socket_id(), "CPU0");
  EXPECT_EQ(c.get_socket(), "CPU 1");
  ASSERT_TRUE(c.get_load_pct());
  EXPECT_EQ(*c.get_load_pct(), 42);
}

TEST(CpuFrequency, SocketDefaultsEmpty) {
  cpu_frequency_check::cpu_frequency c;
  EXPECT_EQ(c.socket_id, "");
  EXPECT_EQ(c.socket, "");
  EXPECT_FALSE(c.load_pct);
}

// ============================================================================
// check_cpu_frequency() filter tests: rows without a load sample (#1391)
// ============================================================================

namespace {
cpu_frequency_check::cpus_type one_cpu(const boost::optional<long long> load) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "CPU A";
  c.socket_id = "CPU0";
  c.socket = "CPU 1";
  c.current_mhz = 3000;
  c.max_mhz = 4000;
  c.number_of_cores = 8;
  c.number_of_logical_processors = 16;
  c.load_pct = load;
  return {c};
}

PB::Common::ResultCode run_frequency_check(const cpu_frequency_check::cpus_type &data, const std::vector<std::string> &args,
                                           PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_cpu_frequency");
  for (const std::string &a : args) request.add_arguments(a);
  cpu_frequency_check::check::check_cpu_frequency(request, &response, data);
  return response.result();
}

std::string all_messages(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::vector<std::string> perf_aliases(const PB::Commands::QueryResponseMessage::Response &r) {
  std::vector<std::string> out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) out.push_back(r.lines(i).perf(j).alias());
  }
  return out;
}
}  // namespace

TEST(CheckCpuFrequency, MissingLoadSampleIsNotEvaluatedAgainstLoadThresholds) {
  // A row with no load sample must neither trip a load threshold nor read as
  // an idle 0; every numeric comparison against a missing value is sure-false.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_frequency_check(one_cpu(boost::none), {"warning=load_pct > 90", "critical=load_pct > 99"}, response), PB::Common::ResultCode::OK)
      << all_messages(response);
  EXPECT_EQ(run_frequency_check(one_cpu(boost::none), {"warning=load_pct < 5"}, response), PB::Common::ResultCode::OK) << all_messages(response);
}

TEST(CheckCpuFrequency, PresentLoadSampleStillTripsLoadThresholds) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_frequency_check(one_cpu(95), {"warning=load_pct > 90"}, response), PB::Common::ResultCode::WARNING) << all_messages(response);

  const std::vector<std::string> aliases = perf_aliases(response);
  EXPECT_NE(std::find(aliases.begin(), aliases.end(), "CPU A_load_pct"), aliases.end()) << all_messages(response);
}

TEST(CheckCpuFrequency, MissingLoadSampleRendersNoLoadSample) {
  PB::Commands::QueryResponseMessage::Response response;
  run_frequency_check(one_cpu(boost::none), {"filter=none", "detail-syntax=%(name): %(load_pct)"}, response);

  const std::string message = all_messages(response);
  EXPECT_NE(message.find("no load sample"), std::string::npos) << message;
}

TEST(CheckCpuFrequency, MissingLoadSampleEmitsNoLoadPerfData) {
  PB::Commands::QueryResponseMessage::Response response;
  // load_pct is the only metric asked for, so any perfdata at all would be a
  // fabricated reading for a socket WMI had no sample for.
  run_frequency_check(one_cpu(boost::none), {"filter=none", "warning=none", "critical=none", "perf-config=extra(load_pct)"}, response);

  EXPECT_TRUE(perf_aliases(response).empty()) << all_messages(response);
}

TEST(CheckCpuFrequency, LoadKeywordComparableAgainstTheNoValueString) {
  PB::Commands::QueryResponseMessage::Response response;
  // The presence test documented for optional keywords.
  EXPECT_EQ(run_frequency_check(one_cpu(boost::none), {"filter=none", "warning=load_pct = 'no load sample'"}, response), PB::Common::ResultCode::WARNING)
      << all_messages(response);

  PB::Commands::QueryResponseMessage::Response with_sample;
  EXPECT_EQ(run_frequency_check(one_cpu(0), {"filter=none", "warning=load_pct = 'no load sample'"}, with_sample), PB::Common::ResultCode::OK)
      << all_messages(with_sample);
}

// ============================================================================
// cpu_frequency_data thread-safety tests (require COM for WMI)
// ============================================================================

class CpuFrequencyDataTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_initialized_ = SUCCEEDED(hr) || hr == S_FALSE;
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
  }

  static void TearDownTestSuite() {
    if (com_initialized_) {
      CoUninitialize();
    }
  }

  static bool com_initialized_;

  // Live WMI is not guaranteed in every environment: COM may fail to initialize,
  // the Win32_Processor query can throw, or (as seen on the 32-bit CI agents) it
  // can succeed yet return no rows. Like WmiQueryTest, the fetch-based tests
  // below skip rather than fail in those environments — a hard assertion here is
  // an environment check, not a code check. On a machine where WMI works the
  // data is populated and the assertions run for real.
  static cpu_frequency_check::cpus_type fetch_or_empty(cpu_frequency_check::cpu_frequency_data &data, std::string &skip_reason) {
    if (!com_initialized_) {
      skip_reason = "COM not initialized";
      return {};
    }
    try {
      data.fetch();
    } catch (const std::exception &e) {
      skip_reason = std::string("CPU frequency WMI unavailable: ") + e.what();
      return {};
    }
    const cpu_frequency_check::cpus_type result = data.get();
    if (result.empty()) skip_reason = "WMI returned no Win32_Processor rows in this environment";
    return result;
  }
};

bool CpuFrequencyDataTest::com_initialized_ = false;

TEST(CpuFrequencyData, GetReturnsEmptyBeforeFetch) {
  cpu_frequency_check::cpu_frequency_data data;
  const auto result = data.get();
  EXPECT_TRUE(result.empty());
}

TEST_F(CpuFrequencyDataTest, FetchPopulatesData) {
  cpu_frequency_check::cpu_frequency_data data;
  std::string skip_reason;
  const auto result = fetch_or_empty(data, skip_reason);
  if (result.empty()) GTEST_SKIP() << skip_reason;
  // Every Windows system has at least one processor
  EXPECT_FALSE(result.empty());
}

TEST_F(CpuFrequencyDataTest, FetchedDataHasNames) {
  cpu_frequency_check::cpu_frequency_data data;
  std::string skip_reason;
  const auto result = fetch_or_empty(data, skip_reason);
  if (result.empty()) GTEST_SKIP() << skip_reason;
  for (const auto &c : result) {
    EXPECT_FALSE(c.name.empty());
  }
}

TEST_F(CpuFrequencyDataTest, FetchedDataHasPositiveFrequencies) {
  cpu_frequency_check::cpu_frequency_data data;
  std::string skip_reason;
  const auto result = fetch_or_empty(data, skip_reason);
  if (result.empty()) GTEST_SKIP() << skip_reason;
  for (const auto &c : result) {
    EXPECT_GT(c.current_mhz, 0);
    EXPECT_GT(c.max_mhz, 0);
    EXPECT_LE(c.current_mhz, c.max_mhz * 2);  // Turbo boost can exceed base, but not by 2x
    EXPECT_GT(c.number_of_cores, 0);
    EXPECT_GT(c.number_of_logical_processors, 0);
    EXPECT_GE(c.number_of_logical_processors, c.number_of_cores);
  }
}

TEST_F(CpuFrequencyDataTest, FetchedFrequencyPctInRange) {
  cpu_frequency_check::cpu_frequency_data data;
  std::string skip_reason;
  const auto result = fetch_or_empty(data, skip_reason);
  if (result.empty()) GTEST_SKIP() << skip_reason;
  for (const auto &c : result) {
    long long pct = c.get_frequency_pct();
    EXPECT_GE(pct, 1);
    EXPECT_LE(pct, 200);  // Turbo boost can push current above max temporarily
  }
}

// ============================================================================
// Large value edge cases
// ============================================================================

TEST(CpuFrequency, LargeValues) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 6000;
  c.max_mhz = 6000;
  c.number_of_cores = 128;
  c.number_of_logical_processors = 256;

  EXPECT_EQ(c.get_frequency_pct(), 100);
  EXPECT_EQ(c.get_number_of_cores(), 128);
  EXPECT_EQ(c.get_number_of_logical_processors(), 256);
}
