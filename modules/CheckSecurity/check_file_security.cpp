// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_file_security.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>

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

// The bare name of a "DOMAIN\name" trustee, or of a Unix "user:name" /
// "group:name" one, default-ACL entries included (the whole string when
// undecorated).
std::string bare_name(const std::string &trustee) {
  static const char *kinds[] = {"default:user:", "default:group:", "default:", "user:", "group:"};
  for (const char *kind : kinds) {
    if (boost::starts_with(trustee, kind)) return trustee.substr(std::char_traits<char>::length(kind));
  }
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
  if (entry.world) return true;
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
  if (entry.trusted) return true;
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

void apply_aces(filter_obj &obj, const std::vector<ace> &raw_aces, const std::vector<std::string> &allow_write,
                const std::vector<std::string> &expected_owners) {
  // Normalise every mask to specific rights before comparing anything.
  std::vector<ace> aces = raw_aces;
  for (ace &entry : aces) entry.mask = map_generic(entry.mask);

  std::vector<std::string> writers, unexpected, rendered;
  // The owner class (Unix) is no surprise as a writer when the caller named
  // that owner as an expected one; a uid matches here just as it does there.
  bool owner_listed = false;
  const ace owner_entry(obj.owner, obj.owner_sid, 0, true);
  for (const std::string &candidate : expected_owners) {
    if (matches(owner_entry, candidate)) owner_listed = true;
  }
  obj.ace_count = static_cast<long long>(aces.size());
  obj.world_writable = 0;
  obj.unexpected_write = 0;

  for (const ace &entry : aces) {
    const std::string label = entry.trustee.empty() ? entry.sid : entry.trustee;
    rendered.push_back(std::string(entry.allow ? "" : "!") + (entry.inherited ? "~" : "") + label + "(" +
                       (entry.rights.empty() ? rights_summary(entry.mask) : entry.rights) + ")");
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
    if (entry.owner && owner_listed) continue;
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

void apply_group(filter_obj &obj, const std::vector<std::string> &expected_groups) {
  if (expected_groups.empty()) {
    obj.group_expected = 1;
    return;
  }
  const ace group_entry(obj.group, obj.group_sid, 0, true);
  obj.group_expected = 0;
  for (const std::string &candidate : expected_groups) {
    if (matches(group_entry, candidate)) obj.group_expected = 1;
  }
}

namespace {

boost::filesystem::path to_path(const std::string &path) {
#ifdef WIN32
  return boost::filesystem::path(utf8::cvt<std::wstring>(path));
#else
  return boost::filesystem::path(path);
#endif
}
std::string from_path(const boost::filesystem::path &path) {
#ifdef WIN32
  return utf8::cvt<std::string>(path.wstring());
#else
  return path.string();
#endif
}

void walk_level(const boost::filesystem::path &dir, const int depth_left, std::vector<std::string> &found,
                std::vector<std::pair<std::string, std::string>> &failed) {
  boost::system::error_code ec;
  boost::filesystem::directory_iterator it(dir, ec);
  if (ec) {
    failed.emplace_back(from_path(dir), "cannot list directory: " + ec.message());
    return;
  }
  std::vector<boost::filesystem::path> entries;
  const boost::filesystem::directory_iterator end;
  while (it != end) {
    entries.push_back(it->path());
    it.increment(ec);
    if (ec) {
      failed.emplace_back(from_path(dir), "cannot list directory: " + ec.message());
      break;
    }
  }
  // Directory order is whatever the file system hands out; sort so the same
  // tree always renders the same line.
  std::sort(entries.begin(), entries.end());
  for (const boost::filesystem::path &entry : entries) {
    const boost::filesystem::file_status status = boost::filesystem::symlink_status(entry, ec);
    if (!ec && boost::filesystem::is_symlink(status)) continue;
    found.push_back(from_path(entry));
    if (depth_left > 1 && !ec && boost::filesystem::is_directory(status)) walk_level(entry, depth_left - 1, found, failed);
  }
}

}  // namespace

void walk(const std::string &root, const int max_depth, std::vector<std::string> &found, std::vector<std::pair<std::string, std::string>> &failed) {
  if (max_depth <= 0) return;
  boost::system::error_code ec;
  const boost::filesystem::path dir = to_path(root);
  // Follows a link given as the root on purpose: the caller named it.
  if (!boost::filesystem::is_directory(dir, ec) || ec) return;
  walk_level(dir, max_depth, found, failed);
}

long long filter_obj::get_age() const { return written == 0 ? 0 : parsers::where::constants::get_now() - written; }
std::string filter_obj::get_written_s() const { return written == 0 ? "" : str::format::format_date(static_cast<std::time_t>(written)); }
std::string filter_obj::get_access_s() const { return access == 0 ? "" : str::format::format_date(static_cast<std::time_t>(access)); }
std::string filter_obj::get_creation_s() const { return creation == 0 ? "" : str::format::format_date(static_cast<std::time_t>(creation)); }

std::string filter_obj::get_state() const {
  if (exists == 0) return error.empty() ? "does not exist" : error;
  if (readable == 0) return error.empty() ? "security descriptor unreadable" : "security descriptor unreadable: " + error;
  std::vector<std::string> problems;
  if (owner_expected == 0) problems.push_back("unexpected owner " + owner);
  if (group_expected == 0) problems.push_back("unexpected group " + group);
  if (world_writable != 0)
    problems.push_back("world writable by " + unexpected);
  else if (unexpected_write != 0)
    problems.push_back("writable by " + unexpected);
  // A Unix path is summed up the way `ls -l` would: owner, group and mode.
  if (problems.empty() && !mode.empty()) return "owner " + owner + ":" + group + " mode " + mode + ", no unexpected write access";
  if (problems.empty()) return "owner " + owner + ", no unexpected write access";
  return join(problems);
}

using parsers::where::type_bool;
using parsers::where::type_date;
using parsers::where::type_int;
using parsers::where::type_size;
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("path", &filter_obj::get_path, "The inspected file or directory")
      .add_string_var("service", &filter_obj::get_service, "Service the path was resolved from (empty for a path= entry)")
      .add_string_var("owner", &filter_obj::get_owner, "Owner: 'DOMAIN\\name' on Windows (the SID when it cannot be resolved), the user name on Unix (the uid when it has none)")
      .add_string_var("owner_sid", &filter_obj::get_owner_sid, "Owner SID on Windows, the numeric uid on Unix")
      .add_string_var("group", &filter_obj::get_group, "Owning group on Unix (the gid when it has no name); the primary group of the security descriptor on Windows")
      .add_string_var("group_sid", &filter_obj::get_group_sid, "Group SID on Windows, the numeric gid on Unix")
      .add_string_var("mode", &filter_obj::get_mode, "Unix permission bits as four octal digits, e.g. '0644' or '4755' (empty on Windows, see sddl)")
      .add_string_var("sddl", &filter_obj::get_sddl, "Windows security descriptor (owner, group and DACL with its raw access masks) in SDDL form (empty on Unix, see mode)")
      .add_string_var("writable", &filter_obj::get_writable, "Comma separated trustees holding write access")
      .add_string_var("unexpected", &filter_obj::get_unexpected, "The trustees with write access that are not allow-listed")
      .add_string_var("dacl", &filter_obj::get_dacl, "The access list rendered as 'trustee(rights)'. Windows: deny entries are prefixed with '!' and inherited ones with '~'; rights are F (full) or a subset of R (read), W (write), X (execute), D (delete), P (permissions). Unix: 'user:name(rwx)', 'group:name(r-x)' and 'other(r--)', POSIX ACL entries included with the ACL mask already applied")
      .add_string_var("error", &filter_obj::get_error, "Why the path or its permissions could not be read (empty when they could)")
      .add_string_var("state", &filter_obj::get_state, "One-line verdict: missing, unreadable, unexpected owner or group, world writable or ok");
  registry_.add_int_var("exists", type_bool, &filter_obj::get_exists, "True when the path exists")
      .no_perf()
      .add_int_var("readable", type_bool, &filter_obj::get_readable, "True when the owner and the access list could be read")
      .no_perf()
      .add_int_var("is_dir", type_bool, &filter_obj::get_is_dir, "True when the path is a directory")
      .no_perf()
      .add_int_var("owner_expected", type_bool, &filter_obj::get_owner_expected, "True when the owner is on the expected-owner list (or no list was given)")
      .no_perf()
      .add_int_var("group_expected", type_bool, &filter_obj::get_group_expected, "True when the group is on the expected-group list (or no list was given)")
      .no_perf()
      .add_int_var("unexpected_write", type_bool, &filter_obj::get_unexpected_write, "True when a trustee outside the allow-list holds write access")
      .no_perf()
      .add_int_var("world_writable", type_bool, &filter_obj::get_world_writable, "True when everybody can write: Everyone, Users, Authenticated Users, Guests, Power Users or Anonymous Logon on Windows, the 'other' permission class on Unix")
      .no_perf()
      .add_int_var("has_acl", type_bool, &filter_obj::get_has_acl, "True when a Unix path carries a POSIX ACL in addition to its mode bits (always false on Windows)")
      .no_perf()
      .add_int_var("setuid", type_bool, &filter_obj::get_setuid, "True when the Unix set-user-ID bit is set (always false on Windows)")
      .no_perf()
      .add_int_var("setgid", type_bool, &filter_obj::get_setgid, "True when the Unix set-group-ID bit is set (always false on Windows)")
      .no_perf()
      .add_int_var("sticky", type_bool, &filter_obj::get_sticky, "True when the Unix sticky bit is set, as on /tmp (always false on Windows)")
      .no_perf()
      .add_int_var("written", type_date, &filter_obj::get_written, "When the path was last written to")
      .no_perf()
      .add_int_var("access", type_date, &filter_obj::get_access, "When the path was last accessed")
      .no_perf()
      .add_int_var("creation", type_date, &filter_obj::get_creation, "When the path was created on Windows; when its inode (owner, mode, links) last changed on Unix")
      .no_perf()
      .add_int_var("age", type_int, &filter_obj::get_age, "Seconds since the path was last written to")
      .add_int_perf("s", "", "_age")
      .add_int_var("size", type_size, &filter_obj::get_size, "Size in bytes")
      .add_scaled_byte_perf("", "_size")
      .add_int_var("ace_count", type_int, &filter_obj::get_ace_count, "Number of access entries: the DACL entries on Windows, the permission classes plus any POSIX ACL entries on Unix")
      .add_int_perf("", "", "_aces");
  registry_.add_human_string("written", &filter_obj::get_written_s, "")
      .add_human_string("access", &filter_obj::get_access_s, "")
      .add_human_string("creation", &filter_obj::get_creation_s, "");
  // clang-format on
}
}  // namespace file_security_filter

namespace check_file_security_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<file_security_filter::filter> filter_helper(request, response, data);

  std::vector<std::string> paths, services, expected_owners, expected_groups, allow_write;
  int max_depth = 0;

  file_security_filter::filter filter;
  // Default: CRITICAL when the path is gone, its descriptor cannot be read, the
  // owner or group is off the expected list or everybody can write it; WARNING
  // for any other write access outside the allow-list. With no expected-owner
  // (or expected-group) given every owner is accepted and only reported.
  filter_helper.add_options("unexpected_write = 1", "exists = 0 or readable = 0 or owner_expected = 0 or group_expected = 0 or world_writable = 1", "",
                            filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${path}: ${state}", "${path}", "%(status): No paths checked",
                           "%(status): all %(count) path(s) have the expected owner and no unexpected write access");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("path", po::value<std::vector<std::string>>(&paths), "File or directory to inspect (repeatable).")
    ("file", po::value<std::vector<std::string>>(&paths), "Alias for path (repeatable).")
    ("service", po::value<std::vector<std::string>>(&services),
     "Service whose binary is inspected (repeatable): the image path of a Windows service, the ExecStart= program of a systemd unit.")
    ("expected-owner", po::value<std::vector<std::string>>(&expected_owners),
     "An acceptable owner (repeatable), matched against the SID, 'DOMAIN\\name' or the bare name on Windows and against the user name or the "
     "numeric uid on Unix. Any other owner is CRITICAL. When omitted the owner is only reported.")
    ("expected-group", po::value<std::vector<std::string>>(&expected_groups),
     "An acceptable group (repeatable), matched like expected-owner: the group name or the numeric gid on Unix. Any other group is CRITICAL. "
     "When omitted the group is only reported.")
    ("allow-write", po::value<std::vector<std::string>>(&allow_write),
     "A trustee allowed to hold write access (repeatable). Windows: in addition to SYSTEM, Administrators, TrustedInstaller and CREATOR OWNER. "
     "Unix: in addition to root, the root group and an owner named with expected-owner; written as a name, 'user:name', 'group:name', 'uid:N', "
     "'gid:N' or 'other'.")
    ("max-depth", po::value<int>(&max_depth)->default_value(0),
     "Also inspect what is inside a directory, this many levels down (1 = its direct children). 0, the default, inspects only the paths given. "
     "Symbolic links inside the tree are not followed.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (paths.empty() && services.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No path specified, use path=<file or folder> and/or service=<service name>");
  }

  std::vector<file_security_filter::filter_obj_ptr> targets;
  std::vector<std::pair<std::string, std::string>> unlistable;
  for (const std::string &path : paths) {
    std::vector<std::string> expanded;
    expanded.push_back(path);
    file_security_filter::walk(path, max_depth, expanded, unlistable);
    for (const std::string &entry : expanded) {
      auto o = std::make_shared<file_security_filter::filter_obj>();
      o->path = entry;
      targets.push_back(o);
    }
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
    // A directory the walk could not list is a finding on that directory: what
    // is inside it went unchecked, which must not read as a clean result.
    for (const auto &failure : unlistable) {
      if (failure.first == o->path && o->error.empty() && o->exists != 0) {
        o->readable = 0;
        o->error = failure.second;
      }
    }
    file_security_filter::apply_aces(*o, aces, allow_write, expected_owners);
    file_security_filter::apply_owner(*o, expected_owners);
    file_security_filter::apply_group(*o, expected_groups);
  }

  parsers::where::constants::reset();
  for (const file_security_filter::filter_obj_ptr &o : targets) filter.match(o);
  filter_helper.post_process(filter);
}

}  // namespace check_file_security_command
