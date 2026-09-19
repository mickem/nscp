// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_volumes.hpp"

#include <Windows.h>

#include <str/utf8.hpp>
#include <string>
#include <vector>

namespace check_disk_facts {

namespace {

// The words `check_drivesize`'s `type` keyword uses, so a record and the
// check agree about what a volume is.
std::string classify(const UINT type) {
  switch (type) {
    case DRIVE_FIXED:
      return "fixed";
    case DRIVE_CDROM:
      return "cdrom";
    case DRIVE_REMOVABLE:
      return "removable";
    case DRIVE_REMOTE:
      return "remote";
    case DRIVE_RAMDISK:
      return "ramdisk";
    default:
      return "unknown";
  }
}

}  // namespace

std::vector<volume> gather_volumes(std::string &error) {
  std::vector<volume> volumes;

  wchar_t buffer[1024];
  const DWORD length = ::GetLogicalDriveStringsW(static_cast<DWORD>(sizeof(buffer) / sizeof(wchar_t)), buffer);
  if (length == 0 || length > sizeof(buffer) / sizeof(wchar_t)) {
    error = "could not enumerate logical drives (GetLogicalDriveStrings failed: " + std::to_string(::GetLastError()) + ")";
    return volumes;
  }

  for (const wchar_t *root = buffer; *root != L'\0'; root += wcslen(root) + 1) {
    const std::wstring path(root);  // "C:\"
    volume found;
    // The drive letter with its colon and no trailing slash: what the check's
    // `drive` keyword reports, so the two name the same volume.
    found.id = utf8::cvt<std::string>(path.substr(0, path.size() > 1 && path[1] == L':' ? 2 : path.size()));
    found.type = classify(::GetDriveTypeW(path.c_str()));

    wchar_t label[MAX_PATH + 1] = {0};
    wchar_t filesystem[MAX_PATH + 1] = {0};
    // An empty CD drive or a disconnected share answers with an error rather
    // than blanks; the volume still exists, so the record is published with
    // what is known and nothing invented for the rest.
    if (::GetVolumeInformationW(path.c_str(), label, MAX_PATH, nullptr, nullptr, nullptr, filesystem, MAX_PATH)) {
      found.label = utf8::cvt<std::string>(std::wstring(label));
      found.fs = utf8::cvt<std::string>(std::wstring(filesystem));
    }

    ULARGE_INTEGER total;
    total.QuadPart = 0;
    if (::GetDiskFreeSpaceExW(path.c_str(), nullptr, &total, nullptr)) {
      found.size_bytes = total.QuadPart;
      found.has_size = true;
    }
    volumes.push_back(found);
  }
  return volumes;
}

}  // namespace check_disk_facts
