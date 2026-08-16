// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_printjobs turns the spooler's per-job rows into something a threshold
// can be written against: what is stuck, whose it is, and how long it has been
// waiting. WMI only supplies the raw fields; everything below is the judgement.

#include "check_printjobs.hpp"

#include <gtest/gtest.h>

using printjobs_check::build_jobs;
using printjobs_check::job_info;
using printjobs_check::status_mask_to_string;

namespace {

const long long now = 1760000000;

job_info make_job(const std::string &printer, const std::string &document, const std::string &owner, const long long status_mask,
                  const long long submitted_ago) {
  job_info job;
  job.printer = printer;
  job.document = document;
  job.owner = owner;
  job.id = 7;
  job.status_mask = status_mask;
  job.size = 65536;
  job.pages = 4;
  job.pages_printed = 1;
  job.submitted_epoch = submitted_ago < 0 ? 0 : now - submitted_ago;
  return job;
}

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run(const std::vector<job_info> &jobs, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_printjobs");
  for (const std::string &a : args) request.add_arguments(a);
  printjobs_check::check_printjobs_from(request, &response, jobs, now);
  return response.result();
}

}  // namespace

TEST(PrintJobs, StatusMaskWords) {
  EXPECT_EQ("queued", status_mask_to_string(0));
  EXPECT_EQ("printing", status_mask_to_string(printjobs_check::job_printing));
  EXPECT_EQ("spooling", status_mask_to_string(printjobs_check::job_spooling));
  EXPECT_EQ("paused", status_mask_to_string(printjobs_check::job_paused));
  // Several bits at once are listed with the actionable ones first.
  EXPECT_EQ("error, paused", status_mask_to_string(printjobs_check::job_error | printjobs_check::job_paused));
  EXPECT_EQ("blocked, printing", status_mask_to_string(printjobs_check::job_blocked | printjobs_check::job_printing));
  // An unknown bit alone still leaves the job as plain queued.
  EXPECT_EQ("queued", status_mask_to_string(0x40000000));
}

TEST(PrintJobs, StatusBitsAreExposedIndividually) {
  const job_info job = make_job("HP LaserJet", "quarterly.pdf", "CORP\\ann",
                                printjobs_check::job_error | printjobs_check::job_user_intervention | printjobs_check::job_paper_out, 120);

  EXPECT_EQ(1, job.get_error());
  EXPECT_EQ(1, job.get_user_intervention());
  EXPECT_EQ(1, job.get_paper_out());
  EXPECT_EQ(0, job.get_paused());
  EXPECT_EQ(0, job.get_printing());
  EXPECT_EQ(0, job.get_blocked());
}

TEST(PrintJobs, AgeIsMeasuredFromTheSubmitTime) {
  const std::vector<job_info> jobs = build_jobs({make_job("HP LaserJet", "a.pdf", "ann", 0, 900)}, now);
  ASSERT_EQ(1u, jobs.size());
  EXPECT_EQ(900, jobs[0].get_age());
}

TEST(PrintJobs, AnUnknownSubmitTimeGivesNoAge) {
  // -1 rather than a huge age, so `age > 30m` cannot trip on a job whose submit
  // time the driver never reported.
  const std::vector<job_info> jobs = build_jobs({make_job("HP LaserJet", "a.pdf", "ann", 0, -1)}, now);
  ASSERT_EQ(1u, jobs.size());
  EXPECT_EQ(-1, jobs[0].get_age());
  EXPECT_EQ("unknown", jobs[0].get_submitted());
}

TEST(PrintJobs, EachJobIsItsOwnRow) {
  PB::Commands::QueryResponseMessage::Response response;
  run({make_job("HP LaserJet", "a.pdf", "ann", 0, 10), make_job("HP LaserJet", "b.pdf", "bob", 0, 20)},
      {"warning=none", "critical=none", "top-syntax=${count}: ${list}", "detail-syntax=${document}/${owner}"}, response);

  EXPECT_EQ("2: a.pdf/ann, b.pdf/bob", join_lines(response));
}

TEST(PrintJobs, AnEmptyQueueIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::OK, run({}, {}, response));
  EXPECT_NE(std::string::npos, join_lines(response).find("No print jobs queued"));
}

TEST(PrintJobs, DefaultsAreOkForAFreshQueue) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::OK, run({make_job("HP LaserJet", "a.pdf", "ann", printjobs_check::job_printing, 30)}, {}, response));
}

TEST(PrintJobs, DefaultsWarnOnAJobThatHasBeenWaiting) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run({make_job("HP LaserJet", "a.pdf", "ann", 0, 1200)}, {}, response));
}

TEST(PrintJobs, DefaultsAreCriticalOnAJobTheSpoolerCannotClear) {
  // The three states that need a person: an error, a blocked device queue, and
  // one waiting for someone at the printer.
  for (const long long mask : {static_cast<long long>(printjobs_check::job_error), static_cast<long long>(printjobs_check::job_blocked),
                               static_cast<long long>(printjobs_check::job_user_intervention)}) {
    PB::Commands::QueryResponseMessage::Response response;
    EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({make_job("HP LaserJet", "a.pdf", "ann", mask, 5)}, {}, response)) << "mask " << mask;
  }
}

TEST(PrintJobs, APausedJobIsNotCriticalByDefault) {
  // Someone paused it on purpose; that is not the spooler failing.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::OK, run({make_job("HP LaserJet", "a.pdf", "ann", printjobs_check::job_paused, 30)}, {}, response));
}

TEST(PrintJobs, TheDefaultDetailNamesTheDocumentOwnerAndAge) {
  PB::Commands::QueryResponseMessage::Response response;
  run({make_job("HP LaserJet", "quarterly.pdf", "CORP\\ann", printjobs_check::job_error, 61)}, {}, response);

  EXPECT_NE(std::string::npos, join_lines(response).find("HP LaserJet: 'quarterly.pdf' by CORP\\ann (error, 61s)"));
}

TEST(PrintJobs, ThresholdsCanBeWrittenAgainstTheJobDetail) {
  const std::vector<job_info> jobs = {make_job("HP LaserJet", "huge.ps", "CORP\\ann", 0, 30)};

  PB::Commands::QueryResponseMessage::Response big;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(jobs, {"warning=none", "critical=size > 1K"}, big));

  PB::Commands::QueryResponseMessage::Response owned;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run(jobs, {"warning=owner = 'CORP\\ann'", "critical=none"}, owned));

  PB::Commands::QueryResponseMessage::Response pages;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(jobs, {"warning=pages > 100", "critical=none"}, pages));
}

TEST(PrintJobs, EmitsPerJobPerfdataKeyedByPrinterAndJobId) {
  PB::Commands::QueryResponseMessage::Response response;
  run({make_job("HP LaserJet", "a.pdf", "ann", 0, 42)}, {"warning=age > 600"}, response);

  ASSERT_EQ(1, response.lines_size());
  bool age = false, count = false;
  for (const auto &perf : response.lines(0).perf()) {
    if (perf.alias() == "HP LaserJet_7_age") age = true;
    if (perf.alias() == "count") count = true;
  }
  EXPECT_TRUE(age);
  EXPECT_TRUE(count);  // the queue depth is always worth graphing
}

TEST(PrintJobs, SizeThresholdsAcceptByteUnits) {
  // `size` is a size-typed keyword, so "500M" must mean 500 megabytes rather
  // than failing to parse or comparing against the literal number 500.
  std::vector<job_info> jobs = {make_job("HP LaserJet", "plot.ps", "bob", 0, 30)};
  jobs[0].size = 700LL * 1024 * 1024;  // 700MB

  PB::Commands::QueryResponseMessage::Response over;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(jobs, {"warning=none", "critical=size > 500M"}, over));

  PB::Commands::QueryResponseMessage::Response under;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(jobs, {"warning=none", "critical=size > 2G"}, under));
}

TEST(PrintJobs, AgeThresholdsAcceptDurationUnits) {
  // Same for durations: "30m" is thirty minutes, matching how check_printqueue
  // documents oldest_job_age.
  const std::vector<job_info> jobs = {make_job("HP LaserJet", "a.pdf", "ann", 0, 45 * 60)};

  PB::Commands::QueryResponseMessage::Response over;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(jobs, {"warning=none", "critical=age > 30m"}, over));

  PB::Commands::QueryResponseMessage::Response under;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(jobs, {"warning=none", "critical=age > 2h"}, under));
}

TEST(PrintJobs, TheMinusOneAgeSentinelIsComparable) {
  // The docs promise -1 for a job without a submit time; comparing against the
  // literal must work even though age otherwise takes duration literals.
  const std::vector<job_info> jobs = {make_job("HP LaserJet", "a.pdf", "ann", 0, -1)};

  PB::Commands::QueryResponseMessage::Response fires;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(jobs, {"warning=none", "critical=age = -1"}, fires));

  PB::Commands::QueryResponseMessage::Response quiet;
  EXPECT_EQ(PB::Common::ResultCode::OK, run({make_job("HP LaserJet", "a.pdf", "ann", 0, 30)}, {"warning=none", "critical=age = -1"}, quiet));
}
