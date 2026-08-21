// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_file_security.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace po = boost::program_options;

namespace file_security_filter {

namespace {

// Groups whose write access effectively means "any local user can change this".
const char *world_sids[] = {
    "S-1-1-0",       // Everyone
    "S-1-5-7",       // ANONYMOUS LOGON
    "S-1-5-11",      // Authenticated Users
    "S-1-5-32-545",  // BUILTIN\Users
    "S-1-5-32-546",  // BUILTIN\Guests
    "S-1-5-32-547",  // BUILTIN\Power Users
};
// Fallback for hosts where the SID could not be resolved back to a SID string.
const char *world_names[] = {"Everyone", "ANONYMOUS LOGON", "Authenticated Users", "Users", "Guests", "Power Users"};

// Trustees that are expected to own and control system content.
const char *trusted_sids[] = {
    "S-1-5-18",      // NT AUTHORITY\SYSTEM
    "S-1-5-32-544",  // BUILTIN\Administrators
    "S-1-3-0",       // CREATOR OWNER
    "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464",  // NT SERVICE\TrustedInstaller
};
const char *trusted_names[] = {"SYSTEM", "Administrators", "CREATOR OWNER", "TrustedInstaller"};

// The bare name of a "DOMAIN\name" trustee (the whole string when undecorated).
std::string bare_name(const std::string &trustee) {
  const std::size_t slash = trustee.find_last_of('\\');
  return slash == std::string::npos ? trustee : trustee.substr(slash + 1);
}

// A trustee matches an allow-list entry by SID, by full "DOMAIN\name" or by the
// bare name - whichever the user found natural to write.
bool matches(const ace &entry, const std::string &candidate) {
  return boost::iequals(entry.sid, candidate) || boost::iequals(entry.trustee, candidate) || boost::iequals(bare_name(entry.trustee), candidate);
}

std::string join(const std::vector<std::string> &items) { return boost::algorithm::join(items, ", "); }

// Expand the GENERIC_* bits into the file-specific rights they map to
// (FILE_GENERIC_READ/WRITE/EXECUTE, FILE_ALL_ACCESS) and drop the generic
// bits. Allow and deny entries routinely mix the two forms - icacls /grant
// writes GENERIC_WRITE where /deny writes the specific bits - and a raw
// bitwise comparison between the forms would let a deny fail to cancel the
// grant it plainly covers (or vice versa).
unsigned long map_generic(unsigned long mask) {
  if ((mask & mask_generic_all) != 0) mask |= mask_all_access;
  if ((mask & mask_generic_read) != 0) mask |= mask_read_data | mask_read_ea | mask_read_attrs | mask_read_control;
  if ((mask & mask_generic_write) != 0) mask |= mask_write_data | mask_append | mask_write_ea | mask_write_attrs | mask_read_control;
  if ((mask & mask_generic_exec) != 0) mask |= mask_execute | mask_read_attrs | mask_read_control;
  return mask & ~(mask_generic_all | mask_generic_read | mask_generic_write | mask_generic_exec);
}

}  // namespace

bool is_world_trustee(const ace &entry) {
  for (const char *sid : world_sids) {
    if (boost::iequals(entry.sid, sid)) return true;
  }
  if (!entry.sid.empty()) return false;
  for (const char *name : world_names) {
    if (boost::iequals(bare_name(entry.trustee), name)) return true;
  }
  return false;
}

bool is_allowed_trustee(const ace &entry, const std::vector<std::string> &allowed) {
  for (const char *sid : trusted_sids) {
    if (boost::iequals(entry.sid, sid)) return true;
  }
  if (entry.sid.empty()) {
    for (const char *name : trusted_names) {
      if (boost::iequals(bare_name(entry.trustee), name)) return true;
    }
  }
  for (const std::string &candidate : allowed) {
    if (matches(entry, candidate)) return true;
  }
  return false;
}

std::string rights_summary(const unsigned long mask) {
  if ((mask & mask_generic_all) != 0 || (mask & mask_all_access) == mask_all_access) return "F";
  std::string rights;
  if ((mask & (mask_read_data | mask_read_ea | mask_read_attrs | mask_generic_read)) != 0) rights += "R";
  if ((mask & (mask_write_data | mask_append | mask_write_ea | mask_write_attrs | mask_generic_write)) != 0) rights += "W";
  if ((mask & (mask_execute | mask_generic_exec)) != 0) rights += "X";
  if ((mask & (mask_delete | mask_delete_child)) != 0) rights += "D";
  if ((mask & (mask_write_dac | mask_write_owner)) != 0) rights += "P";
  return rights.empty() ? "-" : rights;
}

void apply_aces(filter_obj &obj, const std::vector<ace> &raw_aces, const std::vector<std::string> &allow_write) {
  // Normalise every mask to specific rights before comparing anything.
  std::vector<ace> aces = raw_aces;
  for (ace &entry : aces) entry.mask = map_generic(entry.mask);

  std::vector<std::string> writers, unexpected, rendered;
  obj.ace_count = static_cast<long long>(aces.size());
  obj.world_writable = 0;
  obj.unexpected_write = 0;

  for (const ace &entry : aces) {
    const std::string label = entry.trustee.empty() ? entry.sid : entry.trustee;
    rendered.push_back(std::string(entry.allow ? "" : "!") + (entry.inherited ? "~" : "") + label + "(" + rights_summary(entry.mask) + ")");
    if (!entry.allow || (entry.mask & write_bits) == 0) continue;

    // Deny entries for the same trustee win: canonical DACLs evaluate deny
    // first, so subtract what is denied before deciding what is granted.
    unsigned long effective = entry.mask;
    for (const ace &other : aces) {
      if (!other.allow && matches(entry, other.sid.empty() ? other.trustee : other.sid)) effective &= ~other.mask;
    }
    if ((effective & write_bits) == 0) continue;

    if (std::find(writers.begin(), writers.end(), label) != writers.end()) continue;
    writers.push_back(label);
    if (is_allowed_trustee(entry, allow_write)) continue;
    unexpected.push_back(label);
    obj.unexpected_write = 1;
    if (is_world_trustee(entry)) obj.world_writable = 1;
  }

  obj.writable = join(writers);
  obj.unexpected = join(unexpected);
  obj.dacl = join(rendered);
}

void apply_owner(filter_obj &obj, const std::vector<std::string> &expected_owners) {
  if (expected_owners.empty()) {
    obj.owner_expected = 1;
    return;
  }
  const ace owner_entry(obj.owner, obj.owner_sid, 0, true);
  obj.owner_expected = 0;
  for (const std::string &candidate : expected_owners) {
    if (matches(owner_entry, candidate)) obj.owner_expected = 1;
  }
}

std::string filter_obj::get_state() const {
  if (exists == 0) return error.empty() ? "does not exist" : error;
  if (readable == 0) return error.empty() ? "security descriptor unreadable" : "security descriptor unreadable: " + error;
  std::vector<std::string> problems;
  if (owner_expected == 0) problems.push_back("unexpected owner " + owner);
  if (world_writable != 0)
    problems.push_back("world writable by " + unexpected);
  else if (unexpected_write != 0)
    problems.push_back("writable by " + unexpected);
  if (problems.empty()) return "owner " + owner + ", no unexpected write access";
  return join(problems);
}

using parsers::where::type_bool;
using parsers::where::type_int;
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("path", &filter_obj::get_path, "The inspected file or directory")
      .add_string_var("service", &filter_obj::get_service, "Service the path was resolved from (empty for a path= entry)")
      .add_string_var("owner", &filter_obj::get_owner, "Owner as 'DOMAIN\\name' (the SID when it cannot be resolved)")
      .add_string_var("owner_sid", &filter_obj::get_owner_sid, "Owner SID")
      .add_string_var("writable", &filter_obj::get_writable, "Comma separated trustees holding write access")
      .add_string_var("unexpected", &filter_obj::get_unexpected, "The trustees with write access that are not allow-listed")
      .add_string_var("dacl", &filter_obj::get_dacl, "The DACL rendered as 'trustee(rights)'; deny entries are prefixed with '!' and inherited ones with '~'; rights are F (full) or a subset of R (read), W (write), X (execute), D (delete), P (permissions)")
      .add_string_var("error", &filter_obj::get_error, "Why the security descriptor could not be read (empty when it could)")
      .add_string_var("state", &filter_obj::get_state, "One-line verdict: missing, unreadable, unexpected owner, world writable or ok");
  registry_.add_int_var("exists", type_bool, &filter_obj::get_exists, "True when the path exists")
      .no_perf()
      .add_int_var("readable", type_bool, &filter_obj::get_readable, "True when the security descriptor could be read")
      .no_perf()
      .add_int_var("is_dir", type_bool, &filter_obj::get_is_dir, "True when the path is a directory")
      .no_perf()
      .add_int_var("owner_expected", type_bool, &filter_obj::get_owner_expected, "True when the owner is on the expected-owner list (or no list was given)")
      .no_perf()
      .add_int_var("unexpected_write", type_bool, &filter_obj::get_unexpected_write, "True when a trustee outside the allow-list holds write access")
      .no_perf()
      .add_int_var("world_writable", type_bool, &filter_obj::get_world_writable, "True when Everyone, Users, Authenticated Users, Guests, Power Users or Anonymous Logon holds write access")
      .no_perf()
      .add_int_var("ace_count", type_int, &filter_obj::get_ace_count, "Number of explicit entries in the DACL")
      .add_int_perf("", "", "_aces");
  // clang-format on
}
}  // namespace file_security_filter

namespace check_file_security_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<file_security_filter::filter> filter_helper(request, response, data);

  std::vector<std::string> paths, services, expected_owners, allow_write;

  file_security_filter::filter filter;
  // Default: CRITICAL when the path is gone, its descriptor cannot be read, the
  // owner is off the expected list or an untrusted group can write it; WARNING
  // for any other write access outside the allow-list. With no expected-owner
  // given every owner is accepted (the check then only reports it).
  filter_helper.add_options("unexpected_write = 1", "exists = 0 or readable = 0 or owner_expected = 0 or world_writable = 1", "",
                            filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${path}: ${state}", "${path}", "%(status): No paths checked",
                           "%(status): all %(count) path(s) have the expected owner and no unexpected write access");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("path", po::value<std::vector<std::string>>(&paths), "File or directory to inspect (repeatable).")
    ("file", po::value<std::vector<std::string>>(&paths), "Alias for path (repeatable).")
    ("service", po::value<std::vector<std::string>>(&services),
     "Windows service whose binary is inspected (repeatable); the image path is read from the service configuration.")
    ("expected-owner", po::value<std::vector<std::string>>(&expected_owners),
     "An acceptable owner (repeatable), matched against the SID, 'DOMAIN\\name' or the bare name. Any other owner is CRITICAL. "
     "When omitted the owner is only reported.")
    ("allow-write", po::value<std::vector<std::string>>(&allow_write),
     "A trustee allowed to hold write access (repeatable), in addition to SYSTEM, Administrators, TrustedInstaller and CREATOR OWNER.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (!file_security_source::supported()) {
    return nscapi::protobuf::functions::set_response_bad(*response,
                                                         "check_file_security is not supported on this platform (Windows security descriptors only)");
  }
  if (paths.empty() && services.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No path specified, use path=<file or folder> and/or service=<service name>");
  }

  std::vector<file_security_filter::filter_obj_ptr> targets;
  for (const std::string &path : paths) {
    auto o = std::make_shared<file_security_filter::filter_obj>();
    o->path = path;
    targets.push_back(o);
  }
  for (const std::string &service : services) {
    auto o = std::make_shared<file_security_filter::filter_obj>();
    o->service = service;
    std::string error;
    o->path = file_security_source::service_binary(service, error);
    if (o->path.empty()) {
      // Keep the row: an unresolvable service is a finding, not a silent pass.
      o->path = service;
      o->error = error;
    }
    targets.push_back(o);
  }

  for (const file_security_filter::filter_obj_ptr &o : targets) {
    std::vector<file_security_filter::ace> aces;
    if (o->error.empty()) file_security_source::inspect(*o, aces);
    file_security_filter::apply_aces(*o, aces, allow_write);
    file_security_filter::apply_owner(*o, expected_owners);
  }

  parsers::where::constants::reset();
  for (const file_security_filter::filter_obj_ptr &o : targets) filter.match(o);
  filter_helper.post_process(filter);
}

}  // namespace check_file_security_command
