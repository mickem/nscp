// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_printjobs.hpp"

#include <Windows.h>
#include <comdef.h>

#include <ctime>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_printqueue.hpp"
#include <check/duration_keyword.hpp>

namespace printjobs_check {

namespace {

struct status_name {
  long long bit;
  const char *name;
};
// Ordered so the words that explain a stuck job come first.
const status_name status_names[] = {
    {job_error, "error"},
    {job_blocked, "blocked"},
    {job_user_intervention, "user_intervention"},
    {job_paper_out, "paper_out"},
    {job_offline, "offline"},
    {job_paused, "paused"},
    {job_deleting, "deleting"},
    {job_restart, "restart"},
    {job_printing, "printing"},
    {job_spooling, "spooling"},
    {job_printed, "printed"},
    {job_complete, "complete"},
    {job_retained, "retained"},
    {job_deleted, "deleted"},
};

// Optional Win32_PrintJob properties: drivers vary in what they populate, so a
// missing one must not cost us the job.
std::string read_string(const wmi_impl::row &r, const char *column) {
  try {
    const std::string value = r.get_string(column);
    return value == "<NULL>" ? std::string() : value;
  } catch (...) {
    return {};
  }
}
long long read_int(const wmi_impl::row &r, const char *column) {
  try {
    return r.get_int(column);
  } catch (...) {
    return 0;
  }
}

}  // namespace

std::string status_mask_to_string(const long long mask) {
  std::string out;
  for (const status_name &entry : status_names) {
    if ((mask & entry.bit) == 0) continue;
    if (!out.empty()) out += ", ";
    out += entry.name;
  }
  // No bit set is the normal state of a job waiting its turn.
  return out.empty() ? "queued" : out;
}

std::string job_info::get_status() const { return status_mask_to_string(status_mask); }

std::string job_info::get_submitted() const {
  if (submitted_epoch == 0) return "unknown";
  return str::format::format_date(static_cast<std::time_t>(submitted_epoch));
}

std::vector<job_info> build_jobs(const std::vector<job_info> &jobs, const long long now_epoch) {
  std::vector<job_info> out;
  out.reserve(jobs.size());
  for (job_info job : jobs) {
    job.now_epoch = now_epoch;
    out.push_back(job);
  }
  return out;
}

using parsers::where::type_bool;
using parsers::where::type_int;

// `age` is seconds, but nobody thinks in seconds about a stuck print job, so it
// gets the duration converter that makes `age > 30m` mean what it reads like.
static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("printer", &job_info::get_printer, "Printer / queue the job is waiting on")
      .add_string_var("document", &job_info::get_document, "Document name as the application submitted it")
      .add_string_var("owner", &job_info::get_owner, "User who submitted the job")
      .add_string_var("job_status", &job_info::get_status, "Job status words from the spooler: queued, printing, spooling, error, paused, blocked, ...")
      .add_string_var("status", &job_info::get_status, "Deprecated alias for job_status (the name clashes with the generic status summary keyword).")
      .add_string_var("submitted", &job_info::get_submitted, "When the job was submitted, in UTC, or 'unknown'; threshold on age instead");
  registry_.add_int_var("id", type_int, &job_info::get_id, "Spooler job id")
      .no_perf()
      .add_int_var("age", type_custom_age, &job_info::get_age,
                   "Seconds since the job was submitted (-1 when the spooler did not report a submit time); threshold with durations, e.g. age > 30m")
      .add_int_perf("s", "", "_age")
      .add_int_var("size", parsers::where::type_size, &job_info::get_size, "Job size in bytes; threshold with byte units, e.g. size > 500M")
      .add_int_perf("B", "", "_size")
      .add_int_var("pages", type_int, &job_info::get_pages, "Total pages in the job (0 when the driver does not report it)")
      .add_int_perf("", "", "_pages")
      .add_int_var("pages_printed", type_int, &job_info::get_pages_printed, "Pages printed so far")
      .add_int_perf("", "", "_pages_printed")
      .add_int_var("priority", type_int, &job_info::get_priority, "Job priority")
      .no_perf()
      .add_int_var("status_mask", type_int, &job_info::get_status_mask, "Raw StatusMask bit field, for statuses without their own keyword")
      .no_perf()
      .add_int_var("error", type_bool, &job_info::get_error, "1 when the job is in an error state")
      .no_perf()
      .add_int_var("paused", type_bool, &job_info::get_paused, "1 when the job is paused")
      .no_perf()
      .add_int_var("printing", type_bool, &job_info::get_printing, "1 when the job is printing")
      .no_perf()
      .add_int_var("spooling", type_bool, &job_info::get_spooling, "1 when the job is still spooling")
      .no_perf()
      .add_int_var("blocked", type_bool, &job_info::get_blocked, "1 when the job is blocked on the device queue")
      .no_perf()
      .add_int_var("user_intervention", type_bool, &job_info::get_user_intervention, "1 when the job needs someone at the printer")
      .no_perf()
      .add_int_var("offline", type_bool, &job_info::get_offline, "1 when the job's printer is offline")
      .no_perf()
      .add_int_var("paper_out", type_bool, &job_info::get_paper_out, "1 when the job is waiting for paper")
      .no_perf();
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<job_info> >);
  // clang-format on
}

void check_printjobs_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                          const std::vector<job_info> &jobs, const long long now_epoch) {
  modern_filter::data_container container;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, container);
  filter_type filter;

  // Default: CRITICAL on a job the spooler cannot get rid of - an error, a
  // blocked device queue or one waiting for someone to walk to the printer -
  // and WARNING on a job that has been waiting more than ten minutes. An empty
  // queue is the normal state, so empty_state is ok.
  filter_helper.add_options("age > 600", "error = 1 or blocked = 1 or user_intervention = 1", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${printer}: '${document}' by ${owner} (${job_status}, ${age}s)", "${printer}_${id}",
                           "%(status): No print jobs queued", "%(status): All %(count) job(s) ok.");
  filter_helper.set_default_perf_config("extra(count)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  for (const job_info &job : build_jobs(jobs, now_epoch)) {
    const std::shared_ptr<job_info> record(new job_info(job));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

std::vector<job_info> gather() {
  std::vector<job_info> out;

  // Scoped COM init: balanced on every exit path, including a throwing query.
  const com_helper::mta_scope com;
  // All named columns are fixed core-schema Win32_PrintJob properties; the
  // per-column reads still tolerate a NULL value where a driver leaves one
  // unpopulated.
  wmi_impl::query q("select Name, Document, Owner, JobId, StatusMask, Size, TotalPages, PagesPrinted, Priority, TimeSubmitted from Win32_PrintJob",
                    "root\\CIMV2", "", "");
  wmi_impl::row_enumerator rows = q.execute();
  while (rows.has_next()) {
    const wmi_impl::row r = rows.get_next();
    job_info job;
    // Name is "<Printer>, <JobId>"; the printer half is parsed the same way
    // check_printqueue does it, so both checks agree on the queue name.
    job.printer = printqueue_check::job_printer_name(read_string(r, "Name"));
    job.document = read_string(r, "Document");
    job.owner = read_string(r, "Owner");
    job.id = read_int(r, "JobId");
    job.status_mask = read_int(r, "StatusMask");
    job.size = read_int(r, "Size");
    job.pages = read_int(r, "TotalPages");
    job.pages_printed = read_int(r, "PagesPrinted");
    job.priority = read_int(r, "Priority");
    job.submitted_epoch = str::format::parse_cim_datetime(read_string(r, "TimeSubmitted"));
    out.push_back(job);
  }
  return out;
}

void check_printjobs(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_printjobs_from(request, response, gather(), static_cast<long long>(std::time(nullptr)));
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to enumerate print jobs: " + std::string(e.what()));
  }
}

}  // namespace printjobs_check
