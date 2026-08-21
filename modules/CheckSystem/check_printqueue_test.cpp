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

TEST(PrintQueue, DeviceInventorySurvivesTheJoin) {
  // The driver/port inventory is what tells "the queue is fine" from "the queue
  // now points at a different driver", so it must come through build_printers
  // untouched alongside the job counts.
  printer_info p = make_printer("HP LaserJet", 3, 2);
  p.driver = "HP Universal Printing PCL 6";
  p.port = "IP_10.0.0.20";
  p.location = "Floor 2";
  p.share = "HP2";
  p.server = "\\print01";
  p.is_default = true;
  p.is_shared = true;
  p.is_network = true;

  const printers_type out = build_printers({p}, {make_job("HP LaserJet", "20240115080000.000000-000")},
                                           parse_cim_datetime("20240115090000.000000-000"));
  ASSERT_EQ(out.size(), 1u);
  const printer_info &row = out.front();
  EXPECT_EQ(row.get_driver(), "HP Universal Printing PCL 6");
  EXPECT_EQ(row.get_port(), "IP_10.0.0.20");
  EXPECT_EQ(row.get_location(), "Floor 2");
  EXPECT_EQ(row.get_share(), "HP2");
  EXPECT_EQ(row.get_server(), "\\print01");
  EXPECT_EQ(row.get_default(), 1);
  EXPECT_EQ(row.get_shared(), 1);
  EXPECT_EQ(row.get_network(), 1);
  EXPECT_EQ(row.get_jobs(), 1);  // and the queue stats are still computed
}

TEST(PrintQueue, DeviceInventoryDefaultsToEmptyRatherThanUnset) {
  // Location/share/server are NULL on most local queues; they must read as empty
  // strings and false flags, never as garbage.
  const printers_type out = build_printers({make_printer("Local", 3, 2)}, {}, parse_cim_datetime("20240115090000.000000-000"));
  ASSERT_EQ(out.size(), 1u);
  const printer_info &row = out.front();
  EXPECT_EQ(row.get_driver(), "");
  EXPECT_EQ(row.get_location(), "");
  EXPECT_EQ(row.get_server(), "");
  EXPECT_EQ(row.get_default(), 0);
  EXPECT_EQ(row.get_shared(), 0);
  EXPECT_EQ(row.get_network(), 0);
}

namespace {
PB::Common::ResultCode run_queue(const std::vector<printer_info> &printers, const std::vector<raw_job> &jobs, long long now_epoch,
                                 const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_printqueue");
  for (const std::string &a : args) request.add_arguments(a);
  printqueue_check::check::check_printqueue_from(request, &response, printers, jobs, now_epoch);
  return response.result();
}
const long long at_0900 = parse_cim_datetime("20240115090000.000000-000");
}  // namespace

TEST(PrintQueue, OldestJobAgeAcceptsDurationThresholds) {
  // The documented `oldest_job_age > 30m` has to mean thirty minutes; a plain
  // int keyword reads "30m" as the number 30 and fires on almost any queue.
  const std::vector<printer_info> printers = {make_printer("HP LaserJet", 3, 2)};
  const std::vector<raw_job> jobs = {make_job("HP LaserJet", "20240115080000.000000-000")};  // one hour old

  PB::Commands::QueryResponseMessage::Response over;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_queue(printers, jobs, at_0900, {"warning=none", "critical=oldest_job_age > 30m"}, over));

  PB::Commands::QueryResponseMessage::Response under;
  EXPECT_EQ(PB::Common::ResultCode::OK, run_queue(printers, jobs, at_0900, {"warning=none", "critical=oldest_job_age > 2h"}, under));

  // Bare numbers keep meaning seconds, so existing thresholds are unaffected.
  PB::Commands::QueryResponseMessage::Response seconds;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_queue(printers, jobs, at_0900, {"warning=none", "critical=oldest_job_age > 3599"}, seconds));
}

TEST(PrintQueue, AnEmptyQueueNeverTripsAnAgeThreshold) {
  // The age is -1 with nothing queued, which must stay below every duration.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::OK,
            run_queue({make_printer("Idle", 3, 2)}, {}, at_0900, {"warning=none", "critical=oldest_job_age > 1s"}, response));
}

TEST(PrintQueue, DeviceKeywordsCanBeFilteredOn) {
  std::vector<printer_info> printers = {make_printer("HP LaserJet", 3, 2), make_printer("Microsoft Print to PDF", 3, 2)};
  printers[0].driver = "HP Universal Printing PCL 6";
  printers[0].is_shared = true;
  printers[1].driver = "Microsoft Print To PDF";

  PB::Commands::QueryResponseMessage::Response response;
  run_queue(printers, {}, at_0900, {"filter=shared = 1", "warning=none", "critical=none", "top-syntax=${count}: ${list}", "detail-syntax=${printer}/${driver}"},
            response);
  ASSERT_EQ(1, response.lines_size());
  EXPECT_EQ("1: HP LaserJet/HP Universal Printing PCL 6", response.lines(0).message());
}

TEST(PrintQueue, PrinterStatusKeywordAndDeprecatedStatusAlias) {
  const std::vector<printer_info> printers = {make_printer("HP LaserJet", 3, 2)};  // idle

  PB::Commands::QueryResponseMessage::Response renamed;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_queue(printers, {}, at_0900, {"warning=printer_status = 'idle'", "critical=none"}, renamed));

  // The old name keeps working as a deprecated alias.
  PB::Commands::QueryResponseMessage::Response alias;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_queue(printers, {}, at_0900, {"warning=status = 'idle'", "critical=none"}, alias));
}

TEST(PrintQueue, TheMinusOneOldestAgeSentinelIsComparable) {
  // "-1 if the queue is empty" is documented; the duration converter must pass
  // the signed literal through rather than silently evaluating it as false.
  PB::Commands::QueryResponseMessage::Response fires;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL,
            run_queue({make_printer("Idle", 3, 2)}, {}, at_0900, {"warning=none", "critical=oldest_job_age = -1"}, fires));
}
