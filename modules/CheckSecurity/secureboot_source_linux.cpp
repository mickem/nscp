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

// More than any of these variables can hold, so an oversized file is seen as
// oversized without slurping it.
const std::size_t max_variable_bytes = 64;

std::string errno_text(const int e) { return std::strerror(e); }

// Read a whole (small) file. `missing` only for ENOENT/ENOTDIR, so that a
// permission or I/O error is never mistaken for "the variable does not exist".
read_result read_file(const std::string &path, std::string &content, std::string &error) {
  content.clear();
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    const int e = errno;
    if (e == ENOENT || e == ENOTDIR) return read_result::missing;
    error = "Failed to read " + path + ": " + errno_text(e);
    return read_result::failed;
  }
  char buf[max_variable_bytes + 1];
  while (content.size() <= max_variable_bytes) {
    const ssize_t n = ::read(fd, buf, sizeof(buf) - content.size());
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      const int e = errno;
      ::close(fd);
      error = "Failed to read " + path + ": " + errno_text(e);
      return read_result::failed;
    }
    content.append(buf, static_cast<std::size_t>(n));
  }
  ::close(fd);
  return read_result::ok;
}

efi_flag unreadable(const std::string &why) {
  efi_flag f;
  f.state = flag_state::unreadable;
  f.error = why;
  return f;
}

// One interface's file for a variable: `absent` when the file is not there,
// otherwise whatever `parse` and decode_flag() make of its content.
efi_flag read_flag_file(const std::string &path, bool (*parse)(const std::string &, long long &, std::string &)) {
  std::string content, error;
  switch (read_file(path, content, error)) {
    case read_result::missing:
      return efi_flag();
    case read_result::failed:
      return unreadable(error);
    case read_result::ok:
      break;
  }
  long long value = 0;
  if (!parse(content, value, error)) return unreadable("Malformed EFI variable " + path + ": " + error);
  efi_flag f = decode_flag(value);
  if (f.state == flag_state::unreadable) f.error = "Malformed EFI variable " + path + ": " + f.error;
  return f;
}

enum class dir_state { present, missing, failed };

dir_state probe_dir(const std::string &path, std::string &error) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd >= 0) {
    ::close(fd);
    return dir_state::present;
  }
  const int e = errno;
  if (e == ENOENT || e == ENOTDIR) return dir_state::missing;
  error = "Cannot examine " + path + ": " + errno_text(e);
  return dir_state::failed;
}

// Whether either variable interface lists any variable. The efivars directory
// exists on every UEFI boot as the mount point; empty, efivarfs is not mounted.
variable_listing list_variables(const std::string &efi_dir, std::string &error) {
  bool failed = false;
  for (const char *sub : {"/efivars", "/vars"}) {
    const std::string path = efi_dir + sub;
    DIR *dir = ::opendir(path.c_str());
    if (dir == nullptr) {
      const int e = errno;
      if (e != ENOENT && e != ENOTDIR && !failed) {
        failed = true;
        error = "cannot list " + path + ": " + errno_text(e);
      }
      continue;
    }
    bool any = false;
    while (const struct dirent *entry = ::readdir(dir)) {
      if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0) {
        any = true;
        break;
      }
    }
    ::closedir(dir);
    if (any) return variable_listing::some;
  }
  return failed ? variable_listing::failed : variable_listing::none;
}

bool parse_exact(const std::string &content, const std::size_t offset, const char *layout, long long &value, std::string &error) {
  if (content.size() != offset + 1) {
    error = std::string("expected ") + layout + ", got " +
            (content.size() > max_variable_bytes ? "more than " + std::to_string(max_variable_bytes) : std::to_string(content.size())) + " byte(s)";
    return false;
  }
  value = static_cast<unsigned char>(content[offset]);
  return true;
}

}  // namespace

bool parse_efivarfs_byte(const std::string &content, long long &value, std::string &error) {
  return parse_exact(content, 4, "4 attribute bytes and a value byte", value, error);
}

bool parse_sysfs_byte(const std::string &content, long long &value, std::string &error) { return parse_exact(content, 0, "a value byte", value, error); }

efi_flag decode_flag(const long long value) {
  efi_flag f;
  if (value == 0) {
    f.state = flag_state::off;
  } else if (value == 1) {
    f.state = flag_state::on;
  } else {
    f.state = flag_state::unreadable;
    f.error = "unexpected value " + std::to_string(value) + " (expected 0 or 1)";
  }
  return f;
}

efi_flag read_flag(const std::string &efi_dir, const std::string &name) {
  const efi_flag f = read_flag_file(efi_dir + "/efivars/" + name + "-" + global_guid, &parse_efivarfs_byte);
  if (f.state != flag_state::absent) return f;
  // Legacy sysfs interface: the data file holds the value without attributes.
  return read_flag_file(efi_dir + "/vars/" + name + "-" + global_guid + "/data", &parse_sysfs_byte);
}

bool build_state(const efi_flag &secure_boot, const efi_flag &setup_mode, const variable_listing listing, const std::string &efi_dir,
                 secureboot_filter::filter_obj &row, std::string &error) {
  row = secureboot_filter::filter_obj();
  switch (secure_boot.state) {
    case flag_state::unreadable:
      error = secure_boot.error;
      return false;
    case flag_state::absent:
      if (listing == variable_listing::none) {
        error = "Cannot read UEFI variables: " + efi_dir + "/efivars is empty (efivarfs not mounted?)";
        return false;
      }
      if (listing == variable_listing::failed) {
        error = "Cannot read UEFI variables";
        return false;
      }
      // UEFI firmware that does not implement Secure Boot (pre-2.3.1).
      return true;
    case flag_state::off:
    case flag_state::on:
      break;
  }
  row.supported = 1;
  row.enabled = secure_boot.state == flag_state::on ? 1 : 0;
  if (setup_mode.state == flag_state::on || setup_mode.state == flag_state::off) row.setup_mode = setup_mode.state == flag_state::on ? 1 : 0;
  return true;
}

void gather_from(const std::string &efi_dir, std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error) {
  auto o = std::make_shared<secureboot_filter::filter_obj>();
  switch (probe_dir(efi_dir, error)) {
    case dir_state::failed:
      return;
    case dir_state::missing:
      // Not booted through UEFI: a legacy BIOS boot cannot have Secure Boot.
      out.push_back(o);
      return;
    case dir_state::present:
      break;
  }
  const efi_flag secure_boot = read_flag(efi_dir, "SecureBoot");
  efi_flag setup_mode;
  variable_listing listing = variable_listing::some;
  std::string listing_error;
  if (secure_boot.state == flag_state::absent) {
    listing = list_variables(efi_dir, listing_error);
  } else if (secure_boot.state != flag_state::unreadable) {
    setup_mode = read_flag(efi_dir, "SetupMode");
  }
  if (!build_state(secure_boot, setup_mode, listing, efi_dir, *o, error)) {
    if (!listing_error.empty()) error += ": " + listing_error;
    return;
  }
  out.push_back(o);
}

}  // namespace secureboot_source
