// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

namespace shadowcopy_check {

// One raw Win32_ShadowCopy row after parsing.
struct raw_shadow {
  std::string volume;       // VolumeName, e.g. "\\?\Volume{GUID}\"
  long long install_epoch;  // parsed InstallDate as epoch seconds (UTC); 0 if unknown

  raw_shadow() : install_epoch(0) {}
};

// One raw Win32_ShadowStorage row after parsing (per-volume shadow-storage usage).
struct raw_storage {
  std::string volume;    // Volume ref device path (contains a "Volume{GUID}" token)
  long long used;        // UsedSpace (bytes)
  long long allocated;   // AllocatedSpace (bytes)
  long long max_size;    // MaxSpace (bytes; may be a very large "unbounded" sentinel)

  raw_storage() : used(0), allocated(0), max_size(0) {}
};

// Aggregated per-volume shadow-copy state — the filter row.
struct shadowcopy {
  std::string volume;         // the volume the shadow copies belong to
  long long count;            // number of shadow copies on this volume
  long long newest_epoch;     // newest snapshot install time (epoch s); 0 if unknown
  std::string newest_date;    // newest snapshot as a human-readable string ("" if unknown)
  long long used;             // shadow storage used (bytes; 0 if not resolved)
  long long allocated;        // shadow storage allocated (bytes)
  long long max_size;         // shadow storage max (bytes; 0 if unbounded/unresolved)
  long long now_epoch;        // reference "now" for age computation

  shadowcopy() : count(0), newest_epoch(0), used(0), allocated(0), max_size(0), now_epoch(0) {}

  std::string get_volume() const { return volume; }
  long long get_count() const { return count; }
  // Seconds since the newest snapshot; -1 when no snapshot has a parseable date
  // (so a "newest > 26h" threshold does not falsely trip on an unknown date).
  long long get_newest() const { return newest_epoch == 0 ? -1 : (now_epoch - newest_epoch); }
  std::string get_newest_date() const { return newest_date.empty() ? "unknown" : newest_date; }
  long long get_used() const { return used; }
  long long get_allocated() const { return allocated; }
  long long get_max_size() const { return max_size; }
  long long get_used_pct() const { return max_size <= 0 ? 0 : (used * 100 / max_size); }

  std::string show() const { return volume; }
};

typedef std::list<shadowcopy> volumes_type;

// Parse a CIM_DATETIME ("yyyymmddHHMMSS.ffffff±UUU") into epoch seconds (UTC);
// returns 0 when the string cannot be parsed. Exposed for unit testing.
long long parse_cim_datetime(const std::string &s);

// Extract the "Volume{GUID}" token from a device path (used to join shadow
// copies to their shadow-storage row); returns "" if none is present. Exposed
// for unit testing.
std::string volume_guid(const std::string &path);

// Group raw shadow copies by volume, join per-volume shadow-storage usage, and
// compute recency against now_epoch. Pure and testable without WMI.
volumes_type build_volumes(const std::vector<raw_shadow> &shadows, const std::vector<raw_storage> &storages, long long now_epoch);

// Platform data acquisition: WMI on Windows, empty on Unix.
void query(std::vector<raw_shadow> &shadows, std::vector<raw_storage> &storages);

namespace check {
void check_shadowcopy(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check

}  // namespace shadowcopy_check
