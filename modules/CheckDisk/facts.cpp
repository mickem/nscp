// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckDisk.h"

#include <nscapi/nscapi_facts_helper.hpp>
#include <string>
#include <vector>

#include "facts_volumes.hpp"

// CheckDisk's contribution to the host inventory: `storage.volumes`, one
// record per mounted filesystem.
//
// Platform-neutral on purpose - the mapping from a gathered volume to the
// document is the same everywhere, and only the gathering is `#ifdef`'d
// (facts_volumes_win.cpp / facts_volumes_unix.cpp).
void CheckDisk::fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response) {
  if (!request.wants("storage.volumes")) return;

  std::string error;
  const std::vector<check_disk_facts::volume> found = check_disk_facts::gather_volumes(error);
  if (!error.empty()) {
    // Say why rather than publishing an empty list: "this host has no
    // volumes" and "the mount table could not be read" are different facts,
    // and the core keeps the previous value when a round reports an error.
    response.error("storage.volumes", error);
    return;
  }

  nscapi::facts::list volumes = response.set("storage").list("volumes");
  for (const check_disk_facts::volume &entry : found) {
    // The id is the same string check_drivesize's `drive` keyword reports, so
    // a failing check on a volume and this record name the same thing.
    nscapi::facts::section record = volumes.record(entry.id);
    record.value("device", entry.device);
    record.value("fs", entry.fs);
    record.value("type", entry.type);
    record.value("label", entry.label);
    // A volume whose size could not be read is still a volume; writing 0
    // would be a claim the host never made.
    if (entry.has_size) record.value("size_bytes", entry.size_bytes);
  }
}
