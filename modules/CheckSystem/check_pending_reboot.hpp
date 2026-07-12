// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace pending_reboot_check {

// A single aggregate row describing whether the machine is waiting for a reboot
// and why. No single Windows API answers "is a reboot pending"; the signal is
// the union of several independent indicators, each set by a different
// subsystem (servicing, Windows Update, file renames, computer rename, domain
// join). The field names mirror those indicators so a filter/threshold
// expression can single out an individual cause (e.g. `crit=servicing = 1`).
struct reboot_obj {
  bool servicing;        // Component Based Servicing (CBS) queued a reboot
  bool windows_update;   // Windows Update queued a reboot (RebootRequired key)
  bool file_rename;      // PendingFileRenameOperations is present and non-empty
  bool computer_rename;  // ActiveComputerName differs from the pending ComputerName
  bool domain_join;      // Netlogon has a pending domain join / SPN update

  reboot_obj() : servicing(false), windows_update(false), file_rename(false), computer_rename(false), domain_join(false) {}

  bool any() const { return servicing || windows_update || file_rename || computer_rename || domain_join; }
  long long signal_count() const {
    return (servicing ? 1 : 0) + (windows_update ? 1 : 0) + (file_rename ? 1 : 0) + (computer_rename ? 1 : 0) + (domain_join ? 1 : 0);
  }

  long long get_pending() const { return any() ? 1 : 0; }
  long long get_count() const { return signal_count(); }
  long long get_servicing() const { return servicing ? 1 : 0; }
  long long get_windows_update() const { return windows_update ? 1 : 0; }
  long long get_file_rename() const { return file_rename ? 1 : 0; }
  long long get_computer_rename() const { return computer_rename ? 1 : 0; }
  long long get_domain_join() const { return domain_join ? 1 : 0; }

  // Comma-separated list of the human-readable reasons a reboot is pending, or
  // "none" when nothing is queued.
  std::string get_reasons() const;
  // Full status sentence used as the default detail line.
  std::string get_message() const;

  std::string show() const { return get_message(); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<reboot_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<reboot_obj, filter_obj_handler> filter_type;

// Read every pending-reboot signal from the registry (Windows). Best-effort:
// an unreadable key simply leaves its signal false. Uses the 64-bit registry
// view so a 32-bit agent under WOW64 still reads the native keys.
reboot_obj gather_pending_reboot();

// Testable core: render / threshold a pre-populated row without touching the
// registry.
void check_pending_reboot_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               reboot_obj data);

// Live check: gathers the signals from the registry and thresholds them.
void check_pending_reboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace pending_reboot_check
