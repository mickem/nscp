// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The pure half of the Unix check_file_security source: turning mode bits and a
// POSIX ACL into the access entries the shared classification judges, and
// finding the program a systemd unit starts. Nothing here touches the file
// system except unit_program(), which reads the directories it is handed - so
// all of it is unit tested with made-up owners and scratch directories.

#include <functional>
#include <string>
#include <vector>

#include "check_file_security.hpp"

namespace file_security_posix {

// One entry of a POSIX ACL as the kernel stores it (posix_acl_xattr.h).
struct acl_entry {
  acl_entry() : tag(0), perm(0), id(0) {}
  acl_entry(const unsigned tag, const unsigned perm, const unsigned long id) : tag(tag), perm(perm), id(id) {}
  unsigned tag;      // one of the tag_* values below
  unsigned perm;     // rwx as 4/2/1
  unsigned long id;  // uid or gid for tag_user / tag_group, unused otherwise
};
enum acl_tag : unsigned {
  tag_user_obj = 0x01,   // the owner (mirrors the owner mode bits)
  tag_user = 0x02,       // a named user
  tag_group_obj = 0x04,  // the owning group
  tag_group = 0x08,      // a named group
  tag_mask = 0x10,       // upper bound for everything but the owner and other
  tag_other = 0x20,      // everybody else (mirrors the other mode bits)
};

// Decode a system.posix_acl_access / system.posix_acl_default attribute value.
// False when the value is not a version 2 ACL; an empty value is a valid "no
// ACL" and leaves `entries` empty.
bool parse_acl(const std::vector<unsigned char> &blob, std::vector<acl_entry> &entries);

// The permission bits, set-id and sticky included, as four octal digits.
std::string mode_string(unsigned long mode);
// One permission class as `ls` writes it: "rwx", "r-x", "---".
std::string rights_string(unsigned perm);

// What inspect() learned from stat(2) about one path.
struct identity {
  identity() : uid(0), gid(0), mode(0), is_dir(false) {}
  unsigned long uid;
  unsigned long gid;
  unsigned long mode;  // st_mode, file type bits included or not
  bool is_dir;
};
// Resolves a uid (or gid) to its name; returns an empty string when it has none.
typedef std::function<std::string(unsigned long)> name_lookup;

// Build the access entries of one path from its mode bits and, when it has
// them, its access ACL and (for a directory) its default ACL. Named ACL entries
// and the owning group are reported with the ACL mask already applied, so what
// is rendered is what the kernel grants. Returns true when the path carries an
// ACL that says more than the mode bits do.
bool build_entries(const identity &id, const std::vector<acl_entry> &access, const std::vector<acl_entry> &defaults, const name_lookup &user_name,
                   const name_lookup &group_name, std::vector<file_security_filter::ace> &out);

// The program of one ExecStart= value: the special prefixes (@ - : + !) and the
// arguments stripped, quotes honoured.
std::string exec_start_program(const std::string &value);
// The program the systemd unit `service` starts, looked up in `dirs` in the
// order given (first match wins, as systemd does), drop-ins included. Returns
// an empty string and sets `error` when there is no such unit or it starts
// nothing.
std::string unit_program(const std::vector<std::string> &dirs, const std::string &service, std::string &error);

// Read the raw access (or default) ACL of a path. Platform specific: the
// extended attribute on Linux, nothing elsewhere. True with an empty `blob`
// when the path has no such ACL; false with `error` set when it could not be
// read.
bool read_acl(const std::string &path, bool default_acl, std::vector<unsigned char> &blob, std::string &error);

}  // namespace file_security_posix
