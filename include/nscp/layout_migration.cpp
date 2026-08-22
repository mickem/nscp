// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <boost/filesystem.hpp>
#include <nscp/layout_migration.hpp>
#include <nscp/path_defaults.hpp>
#include <win/acl.hpp>

namespace fs = boost::filesystem;

namespace nscp {
namespace paths {

namespace {

// Top-level entries that carry per-machine state and therefore travel.
//
// Everything not named here stays: nscp.exe and its libraries, modules\, web\,
// scripts\ and boot.ini, which stays beside the executable on purpose (it is
// what tells the agent where the shared folder is, so it cannot live inside it).
//
// web\ is the one that looks like an omission and is not. It is shipped by the
// installer and never downloaded on Windows, so it is program content, and
// moving it somewhere the service can write would turn a compromise of the
// service into injected browser-executed code. ${web-path} is anchored to
// ${exe-path} for the same reason.
const char *movable_directories[] = {"fleet", "cache", "crash-dumps", "log"};

// Old logs and a stale bundle cache are not worth failing a migration for, and
// the log file in particular is likely to be held open by a running service.
bool is_essential_directory(const std::string &name) { return name != "log" && name != "cache"; }

// Files under security\ that belong to the package rather than the machine.
// Shipped by the MSI, version-tied, and replaced on upgrade - moving them takes
// them out of the installer's hands and leaves it unable to clean up. Because
// they stay put, ${nrpe-dh} has to go looking for them; both sides share
// is_nrpe_dh_file so "kept here" and "found here" cannot drift apart.
bool is_shipped_security_file(const std::string &name) { return nscp::paths::is_nrpe_dh_file(name); }

// The Windows ROOT store export. The service rewrites it at every start, so
// moving it is pointless and moving a *stale* one is actively bad: it decides
// which CAs the agent trusts.
bool is_regenerated_security_file(const std::string &name) { return name == "windows-ca.pem"; }

// exists() and is_directory() answer false both when an entry is not there and
// when we could not find out, and for a migration those are opposite outcomes:
// the first means nothing to do, the second means we are about to report
// success for something we never looked at. fs::status() keeps them apart -
// a missing entry comes back file_not_found with `ec` cleared, anything else
// comes back status_error with `ec` set.
//
// This matters because run() already guards the top-level source folder for
// exactly this reason; without the same care further in, an unreadable
// security\ is recorded as absent, report.ok() stays true, and the caller
// writes `[layout] mode = modern` for an installation whose certificates and
// agent-state.json never moved.
bool unreadable(const fs::file_status &status) { return status.type() == fs::status_error; }

// A same-volume rename keeps the entry's old security descriptor, so a file
// moved into the locked-down shared folder arrives still carrying the
// `Users: Read & Execute` it inherited in Program Files - readable by every
// account on the machine while the folder around it claims otherwise. Reset it
// to inherit from its new parent instead (propagated through a renamed
// directory's contents by TreeResetNamedSecurityInfo). The cross-volume copy paths
// need none of this: copies are new files and inherit where they are created.
//
// Returns a detail for the step: empty on success, a warning when the reset
// failed. A failed reset does not fail the migration - the entry *is* at the
// destination (a retry would only report it blocked), and the service's boot
// re-protection of the folder re-propagates the inherited ACEs at the next
// start.
std::string reset_acl_after_rename(const fs::path &moved) {
#ifdef WIN32
  std::list<std::string> errors;
  if (nsclient::windows_acl::reset_to_inherited(moved.string(), errors)) return "";
  std::string joined;
  for (const std::string &e : errors) joined += (joined.empty() ? "" : "; ") + e;
  return "moved, but its permissions could not be reset to the destination's (" + joined + "); it keeps the access it had at the source until the next service start";
#else
  static_cast<void>(moved);
  return "";
#endif
}

migration_step make(const std::string &name, const migration_action action, const std::string &detail = "", const bool essential = true) {
  migration_step step;
  step.name = name;
  step.action = action;
  step.detail = detail;
  step.essential = essential;
  return step;
}

// Move one file, or say why not. `dry_run` answers the same question without
// touching the disk, so the CLI can show a plan that matches what will happen.
migration_step move_file(const fs::path &source, const fs::path &target, const std::string &name, const bool dry_run, const bool essential = true) {
  boost::system::error_code ec;
  const fs::file_status source_status = fs::status(source, ec);
  if (unreadable(source_status)) return make(name, migration_action::failed, "cannot read the source: " + ec.message(), essential);
  if (!fs::exists(source_status)) return make(name, migration_action::absent, "", essential);

  boost::system::error_code target_ec;
  const fs::file_status target_status = fs::status(target, target_ec);
  if (unreadable(target_status)) return make(name, migration_action::failed, "cannot read the destination: " + target_ec.message(), essential);
  if (fs::exists(target_status)) {
    // The copy already at the destination is the live one - this is what makes
    // a repeated or half-finished migration safe.
    return make(name, migration_action::blocked, "already present at the destination; keeping it", essential);
  }
  if (dry_run) return make(name, migration_action::moved, "", essential);

  fs::create_directories(target.parent_path(), ec);
  fs::rename(source, target, ec);
  if (ec) {
    // Across volumes rename fails; fall back to copy + remove. Copy first, and
    // only unlink the source once the copy is on disk.
    boost::system::error_code copy_ec;
    fs::copy_file(source, target, copy_ec);
    if (copy_ec) return make(name, migration_action::failed, copy_ec.message(), essential);
    boost::system::error_code remove_ec;
    fs::remove(source, remove_ec);
    if (remove_ec) {
      return make(name, migration_action::moved, "copied, but the original could not be removed: " + remove_ec.message(), essential);
    }
    return make(name, migration_action::moved, "", essential);
  }
  return make(name, migration_action::moved, reset_acl_after_rename(target), essential);
}

// Move a directory the hard way, for when rename() cannot: copy every file
// across and unlink the originals behind us.
//
// Entries already at the destination are left alone rather than overwritten,
// which is what makes a retry after a half-finished copy resume instead of
// clobbering the newer side - the same rule move_file() follows for a single
// file. `error` names the first entry that could not be copied.
bool move_tree_by_copy(const fs::path &from, const fs::path &to, std::string &error) {
  boost::system::error_code ec;
  fs::create_directories(to, ec);
  if (ec) {
    error = "could not create " + to.string() + ": " + ec.message();
    return false;
  }

  std::vector<fs::path> entries;
  for (fs::directory_iterator it(from, ec), end; it != end && !ec; it.increment(ec)) entries.push_back(it->path());
  if (ec) {
    error = "could not list " + from.string() + ": " + ec.message();
    return false;
  }
  std::sort(entries.begin(), entries.end());

  for (const fs::path &entry : entries) {
    const fs::path target = to / entry.filename();
    boost::system::error_code entry_ec;
    const fs::file_status entry_status = fs::status(entry, entry_ec);
    if (entry_status.type() == fs::status_error) {
      error = "could not read " + entry.string() + ": " + entry_ec.message();
      return false;
    }
    if (fs::is_directory(entry_status)) {
      if (!move_tree_by_copy(entry, target, error)) return false;
      continue;
    }
    if (fs::exists(target, entry_ec)) continue;
    fs::copy_file(entry, target, entry_ec);
    if (entry_ec) {
      error = "could not copy " + entry.string() + ": " + entry_ec.message();
      return false;
    }
    // Best effort: a file we copied but could not unlink is a duplicate, not a
    // loss, and the directory removal below will simply not happen.
    boost::system::error_code remove_ec;
    fs::remove(entry, remove_ec);
  }

  // Succeeds only once everything above is gone, which is exactly the condition
  // we want: a leftover means something did not move.
  fs::remove(from, ec);
  return true;
}

void migrate_security(const fs::path &from, const fs::path &to, const bool dry_run, migration_report &report) {
  const fs::path source_dir = from / "security";
  boost::system::error_code ec;
  const fs::file_status source_status = fs::status(source_dir, ec);
  if (unreadable(source_status)) {
    report.steps.push_back(make("security/", migration_action::failed, "cannot read " + source_dir.string() + ": " + ec.message()));
    return;
  }
  if (!fs::is_directory(source_status)) {
    report.steps.push_back(make("security/", migration_action::absent));
    return;
  }

  // Everything in here moves unless it is known not to. An allow-list would
  // strand whatever an admin put here themselves - a custom CA bundle, an extra
  // certificate - and stranding a trust anchor breaks TLS quietly.
  std::vector<fs::path> entries;
  for (fs::directory_iterator it(source_dir, ec), end; it != end && !ec; it.increment(ec)) entries.push_back(it->path());
  if (ec) {
    // Half a listing is worse than none: the entries we did not reach would be
    // reported as nothing at all, and this folder holds the agent's identity.
    report.steps.push_back(make("security/", migration_action::failed, "could not list " + source_dir.string() + ": " + ec.message()));
    return;
  }
  std::sort(entries.begin(), entries.end());

  for (const fs::path &entry : entries) {
    const std::string file = entry.filename().string();
    const std::string name = "security/" + file;
    boost::system::error_code entry_ec;
    const fs::file_status entry_status = fs::status(entry, entry_ec);
    if (unreadable(entry_status)) {
      report.steps.push_back(make(name, migration_action::failed, "cannot read: " + entry_ec.message()));
      continue;
    }
    if (fs::is_directory(entry_status)) {
      report.steps.push_back(make(name, migration_action::kept, "unexpected subdirectory; left alone"));
      continue;
    }
    if (is_shipped_security_file(file)) {
      report.steps.push_back(make(name, migration_action::kept, "shipped with the package, not per-machine state"));
      continue;
    }
    if (is_regenerated_security_file(file)) {
      if (!dry_run) {
        boost::system::error_code remove_ec;
        fs::remove(entry, remove_ec);
      }
      report.steps.push_back(make(name, migration_action::dropped, "re-exported from the Windows ROOT store at every start"));
      continue;
    }
    report.steps.push_back(move_file(entry, to / "security" / file, name, dry_run));
  }
}

void migrate_tree(const fs::path &from, const fs::path &to, const std::string &name, const bool dry_run, migration_report &report) {
  const fs::path source_dir = from / name;
  const bool essential = is_essential_directory(name);
  boost::system::error_code ec;
  const fs::file_status source_status = fs::status(source_dir, ec);
  if (unreadable(source_status)) {
    return report.steps.push_back(make(name + "/", migration_action::failed, "cannot read " + source_dir.string() + ": " + ec.message(), essential));
  }
  if (!fs::is_directory(source_status)) {
    report.steps.push_back(make(name + "/", migration_action::absent, "", essential));
    return;
  }
  const fs::path target_dir = to / name;
  boost::system::error_code target_ec;
  const fs::file_status target_status = fs::status(target_dir, target_ec);
  if (unreadable(target_status)) {
    return report.steps.push_back(make(name + "/", migration_action::failed, "cannot read " + target_dir.string() + ": " + target_ec.message(), essential));
  }
  if (fs::exists(target_status)) {
    return report.steps.push_back(make(name + "/", migration_action::blocked, "already present at the destination; keeping it", essential));
  }
  if (dry_run) {
    return report.steps.push_back(make(name + "/", migration_action::moved, "", essential));
  }
  fs::rename(source_dir, target_dir, ec);
  if (ec) {
    // rename() cannot cross a volume boundary, and this one routinely does: the
    // product installed on D: with %ProgramData% on C: is an ordinary setup, and
    // `fleet` is essential, so failing here used to abandon the migration with
    // nsclient.ini and security\ already moved. move_file() has had this
    // fallback all along; a tree needs it just as much.
    std::string copy_error;
    if (!move_tree_by_copy(source_dir, target_dir, copy_error)) {
      return report.steps.push_back(
          make(name + "/", migration_action::failed, ec.message() + "; copying instead: " + copy_error + " (the folder is now split between the two locations)",
               essential));
    }
    return report.steps.push_back(make(name + "/", migration_action::moved, "copied across volumes", essential));
  }
  report.steps.push_back(make(name + "/", migration_action::moved, reset_acl_after_rename(target_dir), essential));
}

// The destination must be empty for a first switch. List what is in the way, so
// the operator can see whether it is their own leftover or something else -
// capped, because the message goes into an installer log and a CLI line.
std::string describe_destination_contents(const fs::path &to) {
  boost::system::error_code ec;
  std::vector<std::string> names;
  for (fs::directory_iterator it(to, ec), end; it != end && !ec; it.increment(ec)) {
    names.push_back(it->path().filename().string());
    if (names.size() >= 5) break;
  }
  std::string joined;
  for (std::size_t i = 0; i < names.size(); ++i) joined += (i == 0 ? "" : ", ") + names[i];
  if (!ec && names.size() >= 5) joined += ", ...";
  return joined;
}

migration_report run(const std::string &from_path, const std::string &to_path, const bool dry_run, const destination_policy policy) {
  migration_report report;
  const fs::path from(from_path);
  const fs::path to(to_path);

  boost::system::error_code ec;
  if (from.empty() || to.empty()) {
    report.steps.push_back(make("", migration_action::failed, "both the source and the destination folder are required"));
    return report;
  }
  // Compare as text first: equivalent() needs both paths to exist, and during a
  // dry run the destination usually does not.
  if (from == to || (fs::exists(to, ec) && fs::equivalent(from, to, ec))) {
    report.steps.push_back(make("", migration_action::absent, "the source and destination are the same folder"));
    return report;
  }
  // Prove we can actually read the source before reporting on it. Every check
  // below treats an unreadable entry as "not there", so without this a source
  // folder we have no access to produces a clean, empty, entirely wrong report
  // - and a caller that trusts it would switch the layout having moved nothing.
  boost::system::error_code list_ec;
  fs::directory_iterator probe(from, list_ec);
  if (list_ec) {
    report.steps.push_back(make("", migration_action::failed, "cannot read the source folder " + from_path + ": " + list_ec.message()));
    return report;
  }

  if (!dry_run && !fs::is_directory(to, ec)) {
    // See the header: creating it here would mean secrets briefly living in a
    // folder nobody had locked down yet. A dry run is exempt - it changes
    // nothing, and refusing to describe a migration because its destination has
    // not been created yet would make the preview useless.
    report.steps.push_back(make("", migration_action::failed, "the destination " + to_path + " does not exist; create and secure it first"));
    return report;
  }

  // First switch into a folder the caller just created and adopted: it must be
  // empty. %ProgramData% lets any user pre-create the folder and drop files in
  // it, and the per-item logic below would either keep a planted file as the
  // "live" copy (adopt_existing) or never even look at one that has no source
  // counterpart (a planted agent-state.json on a fresh install). So refuse the
  // whole migration up front rather than move anything into a populated folder.
  if (policy == destination_policy::require_pristine && fs::is_directory(to, ec)) {
    const std::string contents = describe_destination_contents(to);
    if (!contents.empty()) {
      report.steps.push_back(make("", migration_action::failed,
                                  "the destination " + to_path + " already contains files (" + contents +
                                      "); refusing to migrate into it. A file here before the first switch is either a previous partial migration or content "
                                      "placed by another user - move it aside and retry so it is not adopted as the agent's configuration or identity."));
      return report;
    }
  }

  report.steps.push_back(move_file(from / "nsclient.ini", to / "nsclient.ini", "nsclient.ini", dry_run));
  migrate_security(from, to, dry_run, report);
  for (const char *directory : movable_directories) migrate_tree(from, to, directory, dry_run, report);
  report.steps.push_back(make("boot.ini", migration_action::kept, "stays beside the executable: it is what points at this folder"));
  return report;
}

const char *action_word(const migration_action action) {
  switch (action) {
    case migration_action::moved:
      return "move";
    case migration_action::kept:
      return "keep";
    case migration_action::dropped:
      return "drop";
    case migration_action::blocked:
      return "keep destination";
    case migration_action::failed:
      return "FAILED";
    default:
      return "none";
  }
}

}  // namespace

bool migration_report::ok() const {
  for (const migration_step &step : steps) {
    if (step.action == migration_action::failed && step.essential) return false;
  }
  return true;
}

bool migration_report::has(const migration_action action) const {
  for (const migration_step &step : steps) {
    if (step.action == action) return true;
  }
  return false;
}

std::vector<std::string> migration_report::describe() const {
  std::vector<std::string> lines;
  for (const migration_step &step : steps) {
    if (step.action == migration_action::absent && step.detail.empty()) continue;  // nothing there; nothing to say
    std::string line = std::string(action_word(step.action)) + "  " + (step.name.empty() ? std::string("(migration)") : step.name);
    if (!step.detail.empty()) line += "  - " + step.detail;
    lines.push_back(line);
  }
  return lines;
}

migration_report plan_migration(const std::string &from, const std::string &to, const destination_policy policy) { return run(from, to, true, policy); }
migration_report apply_migration(const std::string &from, const std::string &to, const destination_policy policy) { return run(from, to, false, policy); }

}  // namespace paths
}  // namespace nscp
