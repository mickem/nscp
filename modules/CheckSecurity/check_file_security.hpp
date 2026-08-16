// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace file_security_filter {

// Win32 access-mask bits (winnt.h values), redefined here so the classification
// logic - and its unit tests - build on every platform.
enum access_bits : unsigned long {
  mask_read_data = 0x00000001,     // FILE_READ_DATA / FILE_LIST_DIRECTORY
  mask_write_data = 0x00000002,    // FILE_WRITE_DATA / FILE_ADD_FILE
  mask_append = 0x00000004,        // FILE_APPEND_DATA / FILE_ADD_SUBDIRECTORY
  mask_read_ea = 0x00000008,       // FILE_READ_EA
  mask_write_ea = 0x00000010,      // FILE_WRITE_EA
  mask_execute = 0x00000020,       // FILE_EXECUTE / FILE_TRAVERSE
  mask_delete_child = 0x00000040,  // FILE_DELETE_CHILD
  mask_read_attrs = 0x00000080,    // FILE_READ_ATTRIBUTES
  mask_write_attrs = 0x00000100,   // FILE_WRITE_ATTRIBUTES
  mask_delete = 0x00010000,        // DELETE
  mask_read_control = 0x00020000,  // READ_CONTROL
  mask_write_dac = 0x00040000,     // WRITE_DAC
  mask_write_owner = 0x00080000,   // WRITE_OWNER
  mask_all_access = 0x001F01FF,    // FILE_ALL_ACCESS
  mask_generic_all = 0x10000000,   // GENERIC_ALL
  mask_generic_exec = 0x20000000,  // GENERIC_EXECUTE
  mask_generic_write = 0x40000000, // GENERIC_WRITE
  mask_generic_read = 0x80000000,  // GENERIC_READ
};

// The bits that let a trustee change the object (or take it over). Deliberately
// excludes the harmless FILE_WRITE_EA/FILE_WRITE_ATTRIBUTES bits, which Windows
// hands out widely and which cannot be used to alter content.
const unsigned long write_bits = mask_write_data | mask_append | mask_delete | mask_delete_child | mask_write_dac | mask_write_owner | mask_generic_write |
                                 mask_generic_all;

// One DACL entry, inherited ones included.
struct ace {
  ace() : mask(0), allow(true), inherited(false) {}
  ace(std::string trustee, std::string sid, const unsigned long mask, const bool allow, const bool inherited = false)
      : trustee(std::move(trustee)), sid(std::move(sid)), mask(mask), allow(allow), inherited(inherited) {}
  std::string trustee;  // resolved name, "DOMAIN\\name" (falls back to the SID)
  std::string sid;      // SID in string form, e.g. S-1-5-32-544
  unsigned long mask;   // access mask
  bool allow;           // true for GRANT/SET, false for DENY
  bool inherited;       // true when the entry comes from a parent folder
};

// One inspected path.
struct filter_obj {
  filter_obj() : exists(0), readable(0), is_dir(0), owner_expected(1), unexpected_write(0), world_writable(0), ace_count(0) {}

  std::string get_path() const { return path; }
  std::string get_service() const { return service; }
  std::string get_owner() const { return owner; }
  std::string get_owner_sid() const { return owner_sid; }
  std::string get_writable() const { return writable; }
  std::string get_unexpected() const { return unexpected; }
  std::string get_dacl() const { return dacl; }
  std::string get_error() const { return error; }
  std::string get_state() const;
  long long get_exists() const { return exists; }
  long long get_readable() const { return readable; }
  long long get_is_dir() const { return is_dir; }
  long long get_owner_expected() const { return owner_expected; }
  long long get_unexpected_write() const { return unexpected_write; }
  long long get_world_writable() const { return world_writable; }
  long long get_ace_count() const { return ace_count; }
  std::string show() const { return path; }

  std::string path;        // the inspected file or directory
  std::string service;     // service the path was resolved from (service= option), else empty
  std::string owner;       // owner as "DOMAIN\\name" (falls back to the SID)
  std::string owner_sid;   // owner SID in string form
  std::string writable;    // comma separated trustees holding write access
  std::string unexpected;  // the subset of `writable` that is not allow-listed
  std::string dacl;        // rendered DACL: "trustee(RWX)", '!' prefixes deny, '~' inherited
  std::string error;       // why the security descriptor could not be read (if it could not)

  long long exists;            // 1 when the path exists
  long long readable;          // 1 when the security descriptor could be read
  long long is_dir;            // 1 when the path is a directory
  long long owner_expected;    // 1 when the owner is on the expected-owner list (or no list was given)
  long long unexpected_write;  // 1 when a trustee outside the allow-list holds write access
  long long world_writable;    // 1 when an untrusted group (Everyone, Users, ...) holds write access
  long long ace_count;         // number of explicit DACL entries
};

// True when `trustee`/`sid` denotes a broad, untrusted group: Everyone,
// Authenticated Users, Users, Guests, Anonymous Logon or Power Users.
bool is_world_trustee(const ace &entry);
// True when the trustee is one Windows itself owns (SYSTEM, Administrators,
// TrustedInstaller, CREATOR OWNER) or one the caller allow-listed. Names match
// either the full "DOMAIN\\name", the bare name or the SID, case insensitively.
bool is_allowed_trustee(const ace &entry, const std::vector<std::string> &allowed);
// Render an access mask as "F" (full) or a subset of R(ead) W(rite) e(X)ecute
// D(elete) P(ermissions).
std::string rights_summary(unsigned long mask);
// Classify the explicit DACL entries onto the object: who can write, which of
// those are unexpected, and whether an untrusted group is among them.
void apply_aces(filter_obj &obj, const std::vector<ace> &aces, const std::vector<std::string> &allow_write);
// Set owner_expected from the expected-owner allow-list (empty list = anything goes).
void apply_owner(filter_obj &obj, const std::vector<std::string> &expected_owners);

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace file_security_filter

namespace file_security_source {
// Resolve the binary a Windows service runs, so its ownership/ACL can be checked.
// Returns an empty string and sets `error` when the service cannot be read.
std::string service_binary(const std::string &service, std::string &error);
// Read owner and DACL of one path into `obj` (obj.path must be set). Never fails
// hard: a missing path or an unreadable descriptor is reported on the object.
void inspect(file_security_filter::filter_obj &obj, std::vector<file_security_filter::ace> &aces);
// True when this platform implements the two functions above.
bool supported();
}  // namespace file_security_source

namespace check_file_security_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
