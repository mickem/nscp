// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace patch_age_check {

// One installed hotfix as reported by Win32_QuickFixEngineering.
struct hotfix_entry {
  std::string id;             // HotFixID, e.g. "KB5034441"
  std::string installed_str;  // raw InstalledOn string as WMI reports it
  long long installed_epoch;  // parsed install date (epoch seconds, 00:00 UTC); 0 if unknown
  std::string description;    // Description, e.g. "Security Update"

  hotfix_entry() : installed_epoch(0) {}
};

// A single aggregate row summarising installed-patch hygiene: how many hotfixes
// are installed, how long ago the newest one landed, and whether a set of
// explicitly-required hotfixes is present. This is the installed-side
// counterpart to check_os_updates (which reports *pending* updates).
struct patch_obj {
  long long count;               // total installed hotfixes
  long long age_days;            // days since the newest hotfix was installed; -1 if unknown
  std::string newest_id;         // HotFixID of the newest hotfix
  std::string newest_installed;  // its InstalledOn string
  std::string ids;               // ';'-joined list of all installed HotFixIDs
  long long required;            // number of hotfixes requested via hotfix=
  long long missing;             // requested hotfixes that are not installed
  std::string missing_ids;       // ';'-joined list of the missing requested hotfixes

  patch_obj() : count(0), age_days(-1), required(0), missing(0) {}

  long long get_count() const { return count; }
  long long get_age() const { return age_days; }
  long long get_required() const { return required; }
  long long get_missing() const { return missing; }
  std::string get_newest_id() const { return newest_id; }
  std::string get_newest_installed() const { return newest_installed; }
  std::string get_ids() const { return ids; }
  std::string get_missing_ids() const { return missing_ids; }
  std::string get_message() const;

  std::string show() const { return get_message(); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<patch_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<patch_obj, filter_obj_handler> filter_type;

// Parse a Win32_QuickFixEngineering InstalledOn string into epoch seconds
// (00:00 UTC on that day), or 0 if it cannot be parsed. Handles the common
// "M/D/YYYY" (US) form and the 8-digit "YYYYMMDD" form; other/empty values
// yield 0. Exposed for unit testing.
long long parse_installed_on(const std::string &s);

// Build the aggregate row from a list of hotfixes, the set of required
// HotFixIDs, and the current time (epoch seconds). Pure and side-effect free so
// it can be unit tested without WMI. Required IDs match case-insensitively and
// tolerate a missing "KB" prefix on numeric requests.
patch_obj build_patch_obj(const std::vector<hotfix_entry> &entries, const std::vector<std::string> &required, long long now_epoch);

// Gather installed hotfixes from Win32_QuickFixEngineering (Windows).
std::vector<hotfix_entry> gather_hotfixes();

// Testable core: parse the hotfix= option, build the aggregate row from the
// supplied hotfix list and current time, then render / threshold it. Takes raw
// entries (not a pre-built row) because the required-hotfix set is parsed from
// the request here.
void check_patch_age_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                          const std::vector<hotfix_entry> &entries, long long now_epoch);

// Live check: enumerates installed hotfixes via WMI and thresholds them.
void check_patch_age(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace patch_age_check
