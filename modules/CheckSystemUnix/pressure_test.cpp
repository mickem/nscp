// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "pressure.h"

#include <gtest/gtest.h>

#include <clocale>
#include <map>
#include <string>

namespace {

const char *const psi_io =
    "some avg10=1.53 avg60=0.87 avg300=0.25 total=2507349\n"
    "full avg10=0.71 avg60=0.40 avg300=0.11 total=1204871\n";

// The cpu file before Linux 5.13 has no `full` line.
const char *const psi_cpu_old = "some avg10=12.50 avg60=8.00 avg300=3.25 total=987654321\n";

std::map<std::string, const PB::Metrics::Metric *> by_key(const PB::Metrics::MetricsBundle &section) {
  std::map<std::string, const PB::Metrics::Metric *> out;
  for (const PB::Metrics::Metric &m : section.value()) out[m.key()] = &m;
  return out;
}

double number(const PB::Metrics::Metric *m) {
  if (m->has_gauge_value()) return m->gauge_value().value();
  if (m->has_counter_value()) return m->counter_value().value();
  return -1;
}

}  // namespace

TEST(Pressure, ParsesBothLinesOfAPsiFile) {
  pressure::resource r;
  ASSERT_TRUE(pressure::parse_psi(psi_io, r));
  ASSERT_TRUE(r.has_some);
  ASSERT_TRUE(r.has_full);
  EXPECT_DOUBLE_EQ(1.53, r.some.avg10);
  EXPECT_DOUBLE_EQ(0.87, r.some.avg60);
  EXPECT_DOUBLE_EQ(0.25, r.some.avg300);
  EXPECT_EQ(2507349ull, r.some.total_us);
  EXPECT_DOUBLE_EQ(0.71, r.full.avg10);
  EXPECT_EQ(1204871ull, r.full.total_us);
}

TEST(Pressure, ACpuFileWithoutFullIsSomeOnly) {
  pressure::resource r;
  ASSERT_TRUE(pressure::parse_psi(psi_cpu_old, r));
  EXPECT_TRUE(r.has_some);
  EXPECT_FALSE(r.has_full);
  EXPECT_DOUBLE_EQ(12.5, r.some.avg10);
}

TEST(Pressure, RejectsWhatIsNotAPsiFile) {
  pressure::resource r;
  EXPECT_FALSE(pressure::parse_psi("", r));
  EXPECT_FALSE(pressure::parse_psi("garbage\n", r));
  // A line missing a field is not half-read.
  EXPECT_FALSE(pressure::parse_psi("some avg10=1.00 avg60=1.00 total=5\n", r));
  EXPECT_FALSE(pressure::parse_psi("other avg10=1 avg60=1 avg300=1 total=1\n", r));
  EXPECT_FALSE(r.has_some);
}

TEST(Pressure, ReadsNumbersInTheClassicLocaleWhateverTheProcessUses) {
  // A decimal-comma locale must not truncate 1.53 at the '.'. Not every host
  // has one installed; the classic-locale imbue is what is under test, and
  // it holds either way.
  const char *previous = std::setlocale(LC_ALL, nullptr);
  const std::string saved = previous != nullptr ? previous : "C";
  std::setlocale(LC_ALL, "sv_SE.UTF-8");
  pressure::resource r;
  const bool parsed = pressure::parse_psi(psi_io, r);
  std::setlocale(LC_ALL, saved.c_str());
  ASSERT_TRUE(parsed);
  EXPECT_DOUBLE_EQ(1.53, r.some.avg10);
}

TEST(Pressure, ReadsOneVmstatCounter) {
  const std::string vmstat = "pgpgin 123\npgmajfault 4567\npgmajfault_s 1\nnr_free_pages 9\n";
  unsigned long long value = 0;
  ASSERT_TRUE(pressure::parse_vmstat_value(vmstat, "pgmajfault", value));
  EXPECT_EQ(4567ull, value);
  EXPECT_FALSE(pressure::parse_vmstat_value(vmstat, "pswpin", value));
  EXPECT_FALSE(pressure::parse_vmstat_value("", "pgmajfault", value));
}

TEST(Pressure, PublishesNothingForAnEmptySample) {
  PB::Metrics::MetricsBundle system;
  pressure::build_pressure_metrics(&system, pressure::sample());
  EXPECT_EQ(0, system.children_size());
}

TEST(Pressure, PublishesOneLabelledFamilyPerFigure) {
  pressure::sample s;
  pressure::resource io;
  io.name = "io";
  ASSERT_TRUE(pressure::parse_psi(psi_io, io));
  pressure::resource cpu;
  cpu.name = "cpu";
  ASSERT_TRUE(pressure::parse_psi(psi_cpu_old, cpu));
  s.resources = {cpu, io};
  s.has_major_faults = true;
  s.major_faults = 4567;

  PB::Metrics::MetricsBundle system;
  pressure::build_pressure_metrics(&system, s);
  ASSERT_EQ(1, system.children_size());
  const PB::Metrics::MetricsBundle &section = system.children(0);
  EXPECT_EQ("pressure", section.key());
  const std::map<std::string, const PB::Metrics::Metric *> m = by_key(section);

  // cpu: some only (4 metrics); io: some and full (8); major faults (1).
  EXPECT_EQ(13u, m.size());
  EXPECT_EQ(0u, m.count("cpu.full_avg10"));
  ASSERT_EQ(1u, m.count("io.some_avg10"));
  const PB::Metrics::Metric *avg = m.at("io.some_avg10");
  EXPECT_TRUE(avg->has_gauge_value());
  EXPECT_DOUBLE_EQ(1.53, number(avg));
  EXPECT_EQ("percent", avg->unit());
  EXPECT_EQ("some_avg10", avg->alias());
  ASSERT_EQ(1, avg->dims_size());
  EXPECT_EQ("resource", avg->dims(0).key());
  EXPECT_EQ("io", avg->dims(0).value());
  // One family, one help: the resource is the label, not part of the text.
  EXPECT_EQ(m.at("cpu.some_avg10")->desc(), avg->desc());

  const PB::Metrics::Metric *total = m.at("io.full_stall_seconds");
  EXPECT_TRUE(total->has_counter_value());
  EXPECT_DOUBLE_EQ(1.204871, number(total));
  EXPECT_EQ("seconds", total->unit());

  const PB::Metrics::Metric *faults = m.at("major_faults");
  EXPECT_TRUE(faults->has_counter_value());
  EXPECT_DOUBLE_EQ(4567, number(faults));
  EXPECT_EQ("", faults->unit());
  for (const auto &e : m) EXPECT_FALSE(e.second->desc().empty()) << e.first;
}

TEST(Pressure, MajorFaultsAloneStillMakeASection) {
  // A kernel without PSI still has /proc/vmstat.
  pressure::sample s;
  s.has_major_faults = true;
  s.major_faults = 1;
  PB::Metrics::MetricsBundle system;
  pressure::build_pressure_metrics(&system, s);
  ASSERT_EQ(1, system.children_size());
  ASSERT_EQ(1, system.children(0).value_size());
  EXPECT_EQ("major_faults", system.children(0).value(0).key());
}
