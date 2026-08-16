// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace printjobs_check {

// JOB_STATUS_* bits of Win32_PrintJob.StatusMask (winspool.h), named here so the
// decoding and its tests build without pulling the spooler headers in.
enum job_status_bits : long long {
  job_paused = 0x00000001,
  job_error = 0x00000002,
  job_deleting = 0x00000004,
  job_spooling = 0x00000008,
  job_printing = 0x00000010,
  job_offline = 0x00000020,
  job_paper_out = 0x00000040,
  job_printed = 0x00000080,
  job_deleted = 0x00000100,
  job_blocked = 0x00000200,
  job_user_intervention = 0x00000400,
  job_restart = 0x00000800,
  job_complete = 0x00001000,
  job_retained = 0x00002000,
};

// One queued print job (Win32_PrintJob), as the filter sees it.
struct job_info {
  job_info() : id(0), status_mask(0), size(0), pages(0), pages_printed(0), priority(0), submitted_epoch(0), now_epoch(0) {}

  std::string get_printer() const { return printer; }
  std::string get_document() const { return document; }
  std::string get_owner() const { return owner; }
  std::string get_status() const;
  std::string get_submitted() const;  // rendered in UTC, like every other date in NSClient++
  long long get_id() const { return id; }
  long long get_status_mask() const { return status_mask; }
  long long get_size() const { return size; }
  long long get_pages() const { return pages; }
  long long get_pages_printed() const { return pages_printed; }
  long long get_priority() const { return priority; }
  // Seconds since the job was submitted; -1 when the spooler did not report a
  // submit time, so an `age > 30m` threshold cannot trip on an unknown one.
  long long get_age() const { return submitted_epoch == 0 ? -1 : now_epoch - submitted_epoch; }
  long long get_error() const { return has(job_error); }
  long long get_paused() const { return has(job_paused); }
  long long get_printing() const { return has(job_printing); }
  long long get_spooling() const { return has(job_spooling); }
  long long get_blocked() const { return has(job_blocked); }
  long long get_user_intervention() const { return has(job_user_intervention); }
  long long get_offline() const { return has(job_offline); }
  long long get_paper_out() const { return has(job_paper_out); }
  long long has(const long long bit) const { return (status_mask & bit) != 0 ? 1 : 0; }

  std::string show() const { return printer + ": " + document; }

  std::string printer;        // printer name, parsed out of Win32_PrintJob.Name
  std::string document;       // Document
  std::string owner;          // Owner
  long long id;               // JobId
  long long status_mask;      // StatusMask
  long long size;             // Size, bytes
  long long pages;            // TotalPages
  long long pages_printed;    // PagesPrinted
  long long priority;         // Priority
  long long submitted_epoch;  // TimeSubmitted as epoch seconds; 0 when unknown
  long long now_epoch;        // reference "now" for the age
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<job_info> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<job_info, filter_obj_handler> filter_type;

// Render a StatusMask as the spooler's own words ("printing", "error, paused"),
// or "queued" when no bit is set. Exposed for unit testing.
std::string status_mask_to_string(long long mask);

// Stamp the reference time onto the gathered jobs. Pure and testable without WMI.
std::vector<job_info> build_jobs(const std::vector<job_info> &jobs, long long now_epoch);

// Gather the queued jobs from WMI (Win32_PrintJob).
std::vector<job_info> gather();

// Testable core: renders and thresholds the supplied jobs.
void check_printjobs_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                          const std::vector<job_info> &jobs, long long now_epoch);

// Live check.
void check_printjobs(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace printjobs_check
