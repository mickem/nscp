// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

namespace printqueue_check {

// One raw print job (Win32_PrintJob) after parsing.
struct raw_job {
  std::string printer;        // printer name parsed from Win32_PrintJob.Name
  long long submitted_epoch;  // TimeSubmitted parsed to epoch seconds; 0 if unknown
  bool error;                 // job is in an error state (StatusMask error bit)

  raw_job() : submitted_epoch(0), error(false) {}
};

// One printer plus its aggregated queue stats — the filter row.
struct printer_info {
  std::string name;
  long long printer_status;  // Win32_Printer.PrinterStatus (1..7)
  long long error_state;     // Win32_Printer.DetectedErrorState (0..11)
  bool work_offline;         // Win32_Printer.WorkOffline
  long long jobs;            // number of queued jobs
  long long error_jobs;      // jobs in an error state
  long long oldest_epoch;    // oldest job's submit time (epoch s); 0 if none/unknown
  long long now_epoch;       // reference "now" for age computation

  printer_info() : printer_status(0), error_state(0), work_offline(false), jobs(0), error_jobs(0), oldest_epoch(0), now_epoch(0) {}

  // PrinterStatus -> short string.
  static std::string status_to_string(long long s) {
    switch (s) {
      case 1:
        return "other";
      case 2:
        return "unknown";
      case 3:
        return "idle";
      case 4:
        return "printing";
      case 5:
        return "warmup";
      case 6:
        return "stopped_printing";
      case 7:
        return "offline";
      default:
        return "unknown";
    }
  }
  // DetectedErrorState -> short string.
  static std::string error_state_to_string(long long e) {
    switch (e) {
      case 0:
        return "unknown";
      case 1:
        return "other";
      case 2:
        return "no_error";
      case 3:
        return "low_paper";
      case 4:
        return "no_paper";
      case 5:
        return "low_toner";
      case 6:
        return "no_toner";
      case 7:
        return "door_open";
      case 8:
        return "jammed";
      case 9:
        return "offline";
      case 10:
        return "service_requested";
      case 11:
        return "output_bin_full";
      default:
        return "unknown";
    }
  }
  // A "real" error state — paper/toner/door/jam/service — as opposed to offline
  // (tracked separately) or the no-error/unknown states.
  static bool is_error_state(long long e) {
    switch (e) {
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 10:
      case 11:
        return true;
      default:
        return false;
    }
  }

  std::string get_printer() const { return name; }
  std::string get_status() const { return status_to_string(printer_status); }
  std::string get_error_state() const { return error_state_to_string(error_state); }
  // Offline: the printer says so, or the status/error state reports it.
  long long get_offline() const { return (work_offline || printer_status == 7 || error_state == 9) ? 1 : 0; }
  long long get_error() const { return is_error_state(error_state) ? 1 : 0; }
  long long get_jobs() const { return jobs; }
  long long get_error_jobs() const { return error_jobs; }
  // Seconds since the oldest queued job; -1 when the queue is empty or the
  // submit time is unknown (so a "oldest_job_age > 30m" threshold cannot trip
  // on an empty queue).
  long long get_oldest_job_age() const { return oldest_epoch == 0 ? -1 : (now_epoch - oldest_epoch); }

  std::string show() const { return name; }
};

typedef std::list<printer_info> printers_type;

// Parse a CIM_DATETIME ("yyyymmddHHMMSS.ffffff±UUU") to epoch seconds (UTC), or
// 0 if it cannot be parsed. Exposed for unit testing.
long long parse_cim_datetime(const std::string &s);

// Parse the printer name out of a Win32_PrintJob.Name ("<Printer>, <JobId>").
// Exposed for unit testing.
std::string job_printer_name(const std::string &job_name);

// Join queued jobs onto their printers and compute queue depth / oldest-job age
// against now_epoch. Pure and testable without WMI.
printers_type build_printers(const std::vector<printer_info> &printers, const std::vector<raw_job> &jobs, long long now_epoch);

// Gather printers and jobs from WMI (Win32_Printer + Win32_PrintJob).
void gather(std::vector<printer_info> &printers, std::vector<raw_job> &jobs);

namespace check {
void check_printqueue(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check

}  // namespace printqueue_check
