// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_printqueue.hpp"

#include <gtest/gtest.h>

using printqueue_check::build_printers;
using printqueue_check::job_printer_name;
using printqueue_check::parse_cim_datetime;
using printqueue_check::printer_info;
using printqueue_check::printers_type;
using printqueue_check::raw_job;

namespace {
printer_info make_printer(const std::string &name, long long status, long long error_state, bool offline = false) {
  printer_info p;
  p.name = name;
  p.printer_status = status;
  p.error_state = error_state;
  p.work_offline = offline;
  return p;
}
raw_job make_job(const std::string &printer, const std::string &submitted, bool error = false) {
  raw_job j;
  j.printer = printer;
  j.submitted_epoch = parse_cim_datetime(submitted);
  j.error = error;
  return j;
}
}  // namespace

TEST(PrintQueue, JobPrinterNameParsing) {
  EXPECT_EQ(job_printer_name("Microsoft Print to PDF, 3"), "Microsoft Print to PDF");
  EXPECT_EQ(job_printer_name("HP LaserJet, 42"), "HP LaserJet");
  // No ", <id>" suffix: return as-is.
  EXPECT_EQ(job_printer_name("SomePrinter"), "SomePrinter");
}

TEST(PrintQueue, StatusAndErrorStateStrings) {
  EXPECT_EQ(printer_info::status_to_string(3), "idle");
  EXPECT_EQ(printer_info::status_to_string(7), "offline");
  EXPECT_EQ(printer_info::error_state_to_string(2), "no_error");
  EXPECT_EQ(printer_info::error_state_to_string(8), "jammed");
  EXPECT_TRUE(printer_info::is_error_state(8));   // jammed is a real error
  EXPECT_FALSE(printer_info::is_error_state(9));  // 9 = offline, tracked separately
  EXPECT_FALSE(printer_info::is_error_state(2));  // no_error
}

TEST(PrintQueue, OfflineDerivation) {
  EXPECT_EQ(make_printer("p", 3, 2, false).get_offline(), 0);        // idle, no error
  EXPECT_EQ(make_printer("p", 7, 2, false).get_offline(), 1);        // status=offline
  EXPECT_EQ(make_printer("p", 3, 9, false).get_offline(), 1);        // error_state=offline
  EXPECT_EQ(make_printer("p", 3, 2, /*offline*/ true).get_offline(), 1);  // WorkOffline
}

TEST(PrintQueue, ErrorDerivation) {
  EXPECT_EQ(make_printer("p", 3, 2).get_error(), 0);  // no_error
  EXPECT_EQ(make_printer("p", 3, 4).get_error(), 1);  // no_paper
  EXPECT_EQ(make_printer("p", 3, 9).get_error(), 0);  // offline is not a "real" error
}

TEST(PrintQueue, JoinsJobsAndComputesOldestAge) {
  const std::vector<printer_info> printers = {make_printer("HP LaserJet", 4, 2), make_printer("Idle Printer", 3, 2)};
  const std::vector<raw_job> jobs = {
      make_job("HP LaserJet", "20240115080000.000000-000"),
      make_job("HP LaserJet", "20240115083000.000000-000", /*error*/ true),  // newer
      make_job("Idle Printer", ""),                                          // unknown submit time
  };
  // now = 2024-01-15 09:00:00 UTC
  const long long now = parse_cim_datetime("20240115090000.000000-000");
  const printers_type out = build_printers(printers, jobs, now);
  ASSERT_EQ(out.size(), 2u);

  const printer_info &hp = out.front();
  EXPECT_EQ(hp.get_printer(), "HP LaserJet");
  EXPECT_EQ(hp.get_jobs(), 2);
  EXPECT_EQ(hp.get_error_jobs(), 1);
  // Oldest job is 08:00, now is 09:00 -> 3600s.
  EXPECT_EQ(hp.get_oldest_job_age(), 3600);

  const printer_info &idle = out.back();
  EXPECT_EQ(idle.get_jobs(), 1);
  EXPECT_EQ(idle.get_error_jobs(), 0);
  // The only job has no parseable submit time -> unknown age.
  EXPECT_EQ(idle.get_oldest_job_age(), -1);
}

TEST(PrintQueue, EmptyQueueHasNegativeAge) {
  const std::vector<printer_info> printers = {make_printer("Empty", 3, 2)};
  const printers_type out = build_printers(printers, {}, parse_cim_datetime("20240115090000.000000-000"));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out.front().get_jobs(), 0);
  EXPECT_EQ(out.front().get_oldest_job_age(), -1);
}

TEST(PrintQueue, JobForUnknownPrinterIsIgnored) {
  const std::vector<printer_info> printers = {make_printer("Known", 3, 2)};
  const std::vector<raw_job> jobs = {make_job("Ghost", "20240115080000.000000-000")};
  const printers_type out = build_printers(printers, jobs, parse_cim_datetime("20240115090000.000000-000"));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out.front().get_jobs(), 0);  // the ghost job attaches to no enumerated printer
}
