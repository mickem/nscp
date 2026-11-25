// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux data source for check_secureboot: the UEFI global variables SecureBoot
// and SetupMode, read from efivarfs (/sys/firmware/efi/efivars) or, on kernels
// that predate it, the legacy sysfs interface (/sys/firmware/efi/vars). Both are
// world-readable for these two variables, so no privilege is needed.

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "check_secureboot.hpp"

namespace secureboot_source {

namespace {

const char *const global_guid = "8be4df61-93ca-11d2-aa0d-00e098032b8c";

enum class read_result { ok, missing, failed };

// Read a whole (small) file. `missing` only for ENOENT/ENOTDIR, so that a
// permission or I/O error is never mistaken for "the variable does not exist".
read_result read_file(const std::string &path, std::string &content, std::string &error) {
  content.clear();
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    const int e = errno;
    if (e == ENOENT || e == ENOTDIR) return read_result::missing;
    error = "Failed to read " + path + ": " + std::strerror(e);
    return read_result::failed;
  }
  char buf[256];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      const int e = errno;
      ::close(fd);
      error = "Failed to read " + path + ": " + std::strerror(e);
      return read_result::failed;
    }
    content.append(buf, static_cast<std::size_t>(n));
    if (content.size() > 4096) break;  // these variables are a byte; never slurp an unbounded file
  }
  ::close(fd);
  return read_result::ok;
}

// One EFI global variable. `found` is false when it is absent from both
// interfaces; `error` is set when it exists but cannot be read or parsed.
struct variable {
  bool found = false;
  long long value = 0;
};

variable read_variable(const std::string &efi_dir, const std::string &name, std::string &error) {
  variable v;
  std::string content;
  const std::string efivarfs_path = efi_dir + "/efivars/" + name + "-" + global_guid;
  read_result r = read_file(efivarfs_path, content, error);
  if (r == read_result::failed) return v;
  if (r == read_result::ok) {
    if (!parse_efivarfs_byte(content, v.value, error)) {
      error = "Malformed EFI variable " + efivarfs_path + ": " + error;
      return v;
    }
    v.found = true;
    return v;
  }
  // Legacy sysfs interface: the data file holds the value without attributes.
  const std::string sysfs_path = efi_dir + "/vars/" + name + "-" + global_guid + "/data";
  r = read_file(sysfs_path, content, error);
  if (r == read_result::failed) return v;
  if (r == read_result::ok) {
    if (content.empty()) {
      error = "Malformed EFI variable " + sysfs_path + ": no data";
      return v;
    }
    v.value = static_cast<unsigned char>(content[0]);
    v.found = true;
  }
  return v;
}

bool dir_exists(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) return false;
  ::close(fd);
  return true;
}

// True if either variable interface lists any variable. The efivars directory
// exists on every UEFI boot as the mount point; empty, efivarfs is not mounted.
bool variables_accessible(const std::string &efi_dir) {
  for (const char *sub : {"/efivars", "/vars"}) {
    DIR *dir = ::opendir((efi_dir + sub).c_str());
    if (dir == nullptr) continue;
    bool any = false;
    while (const struct dirent *entry = ::readdir(dir)) {
      if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0) {
        any = true;
        break;
      }
    }
    ::closedir(dir);
    if (any) return true;
  }
  return false;
}

}  // namespace

bool parse_efivarfs_byte(const std::string &content, long long &value, std::string &error) {
  if (content.size() < 5) {
    error = "expected 4 attribute bytes and a value byte, got " + std::to_string(content.size()) + " byte(s)";
    return false;
  }
  value = static_cast<unsigned char>(content[4]);
  return true;
}

void gather_from(const std::string &efi_dir, std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error) {
  auto o = std::make_shared<secureboot_filter::filter_obj>();
  if (!dir_exists(efi_dir)) {
    // Not booted through UEFI: a legacy BIOS boot cannot have Secure Boot.
    out.push_back(o);
    return;
  }
  const variable sb = read_variable(efi_dir, "SecureBoot", error);
  if (!error.empty()) return;
  if (!sb.found) {
    if (!variables_accessible(efi_dir)) {
      error = "Cannot read UEFI variables: " + efi_dir + "/efivars is empty (efivarfs not mounted?)";
      return;
    }
    // UEFI firmware that does not implement Secure Boot (pre-2.3.1).
    out.push_back(o);
    return;
  }
  o->supported = 1;
  o->enabled = sb.value == 1 ? 1 : 0;
  std::string setup_error;
  const variable sm = read_variable(efi_dir, "SetupMode", setup_error);
  // SetupMode is auxiliary: an unreadable one leaves the keyword unknown rather
  // than failing a check whose main answer is already known.
  if (setup_error.empty() && sm.found) o->setup_mode = sm.value == 1 ? 1 : 0;
  out.push_back(o);
}

}  // namespace secureboot_source
