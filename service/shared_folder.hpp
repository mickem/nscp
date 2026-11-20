// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32

#include <boost/filesystem/operations.hpp>
#include <exception>
#include <functional>
#include <list>
#include <nsclient/logger/logger.hpp>
#include <settings/settings_interface.hpp>
#include <string>
#include <vector>
#include <win/acl.hpp>

#include "path_manager.hpp"

namespace nsclient {
namespace core {

// Creating the shared folder and locking it down before anything writes into
// it - the decisions, apart from the ACL calls that carry them out.
//
// Only for the modern layout: the legacy one lives in the install directory,
// whose permissions are the installer's business. On the modern layout the
// folder sits under %ProgramData%, which grants Users: Read & Execute by
// inheritance - so creating it and walking away would publish the
// configuration (passwords), the fleet private key and the log to every
// account on the machine.
//
// It runs during the settings bootstrap of *every* process, elevated or not,
// which is what makes the order of the questions matter: what it refuses, what
// it leaves alone and what it complains about are three different answers, and
// a wrong one is either a hole or a warning nobody believes any more.
namespace shared_folder {

// The ACL operations, replaceable so a unit test can put the folder in any
// state - restricted, unreadable, refusing the lockdown - without the
// privileges it takes to get a real one there. The defaults are the real ones.
//
// `service_sids` is who, beyond SYSTEM and Administrators, is supposed to have
// access: the service's own account when it does not run as SYSTEM (see
// nscp/service_account.hpp). Every process has to be told, not just the
// service - an elevated `nscp` that did not expect the grant would take it for
// an intruder and remove it, locking the service out of its configuration.
struct acl_functions {
  std::function<bool(const std::string &, std::list<std::string> &)> is_reparse_point = &windows_acl::is_reparse_point;
  std::function<windows_acl::protection(const std::string &, std::list<std::string> &)> inspect_protection;
  std::function<bool(const std::string &, std::list<std::string> &)> protect_directory;
  std::function<bool(const std::string &, std::list<std::string> &)> is_protected;
  // Who the folder is restricted to, for the log.
  std::string audience = "SYSTEM and Administrators";

  explicit acl_functions(const std::vector<std::string> &service_sids = std::vector<std::string>()) {
    inspect_protection = [service_sids](const std::string &path, std::list<std::string> &errors) {
      return windows_acl::inspect_protection(path, errors, service_sids);
    };
    protect_directory = [service_sids](const std::string &path, std::list<std::string> &errors) {
      return windows_acl::protect_directory(path, errors, service_sids);
    };
    is_protected = [service_sids](const std::string &path, std::list<std::string> &errors) { return windows_acl::is_protected(path, errors, service_sids); };
    if (!service_sids.empty()) audience = "SYSTEM, Administrators and the service account";
  }
};

enum class outcome {
  // Not the modern layout, or no shared folder to speak of: nothing was looked
  // at and nothing was touched.
  not_applicable,
  // Already restricted to SYSTEM and Administrators. Left alone.
  already_restricted,
  // The permissions could not be read. Left to the service, quietly.
  cannot_inspect,
  // Created if it was missing, locked down, and verified.
  restricted,
  // The folder is not there and could not be created.
  create_failed,
  // The lockdown failed: the folder is readable by every user.
  protect_failed,
  // The lockdown reported success and the folder is still open.
  still_open,
  // Something threw on the way. Reported, like the three above; never fatal.
  failed,
};

// Throws settings::settings_exception - the one fatal answer - when the shared
// folder is a junction or a symbolic link. Everything else is reported through
// `log` and returned.
inline outcome prepare(path_manager &paths, const logging::log_client_accessor &log, const acl_functions &acl = acl_functions()) {
  const auto get_logger = [&log] { return log; };

  if (paths.get_layout() != nscp::paths::layout::modern) return outcome::not_applicable;
  const std::string shared = paths.expand_path("${shared-path}");
  if (shared.empty()) return outcome::not_applicable;

  // Outside the try below on purpose: this one is fatal. A junction or
  // symbolic link where the shared folder should be is not a permissions
  // problem an operator fixes in place - it is a redirect. %ProgramData%
  // lets any local account create one under our name before we first run,
  // and every path-based operation follows it: the lockdown would land on
  // the link's target while the link stays the creator's to remove and
  // replace with a real folder holding a configuration of their choosing,
  // which the next start would then load as SYSTEM. So the configuration is
  // not read from behind a link, ever. Relocating the folder is supported
  // through the [paths] section of boot.ini instead.
  {
    std::list<std::string> reparse_errors;
    if (acl.is_reparse_point(shared, reparse_errors)) {
      throw settings::settings_exception(__FILE__, __LINE__,
                                         "Refusing to use " + shared +
                                             ": it is a junction or symbolic link, not a directory. The shared folder must be a real directory; "
                                             "remove the link (and whatever created it), or relocate the folder with a [paths] override in boot.ini.");
    }
  }

  try {
    // Look before acting. This runs during the settings bootstrap of *every*
    // process, not just the service: an unelevated `nscp client ...` has no
    // WRITE_DAC, so it used to fail below and announce that a folder which is
    // in fact locked down "may be readable by every user on this machine".
    // Restricting it is the service's job, and the same reasoning as the
    // trust-store export applies - complaining about something working as
    // designed is noise that trains operators to ignore the message.
    boost::system::error_code ec;
    if (boost::filesystem::is_directory(shared, ec)) {
      std::list<std::string> state_errors;
      switch (acl.inspect_protection(shared, state_errors)) {
        case windows_acl::protection::restricted:
          LOG_DEBUG_CORE(shared + " is already restricted to " + acl.audience);
          return outcome::already_restricted;
        case windows_acl::protection::unknown:
          // Reading the security descriptor needs READ_CONTROL, which the
          // lockdown denies to everyone else - so being unable to look is
          // itself evidence that the folder is not open, and there is nothing
          // useful an unprivileged process could do about it either way.
          for (const std::string &e : state_errors) LOG_DEBUG_CORE("acl: " + e);
          LOG_DEBUG_CORE("Cannot inspect the permissions on " + shared + "; leaving that to the service");
          return outcome::cannot_inspect;
        case windows_acl::protection::incomplete:
          // Nobody unexpected has access, but the service account lost its
          // grant. Same remedy as an open folder: apply the lockdown again.
          for (const std::string &e : state_errors) LOG_DEBUG_CORE("acl: " + e);
          break;
        case windows_acl::protection::open:
          break;  // genuinely wide open - fix it, and complain if we cannot
      }
    }

    boost::filesystem::create_directories(shared, ec);
    if (ec) {
      LOG_ERROR_CORE("Failed to create " + shared + ": " + ec.message());
      return outcome::create_failed;
    }
    std::list<std::string> errors;
    if (!acl.protect_directory(shared, errors)) {
      for (const std::string &e : errors) LOG_ERROR_CORE("acl: " + e);
      // Loud, and deliberately not fatal: refusing to start would take a
      // working agent offline over a permissions problem an operator can fix.
      // Say plainly what is exposed instead - and it is exposed, not "may
      // be": we either just created the folder (which inherits
      // Users: Read & Execute from %ProgramData%) or looked and found it open.
      LOG_ERROR_CORE("Could not restrict access to " + shared +
                     " - it is readable by every user on this machine, including the configuration and the fleet identity.");
      return outcome::protect_failed;
    }
    errors.clear();
    if (!acl.is_protected(shared, errors)) {
      // Setting the DACL and the DACL actually excluding everyone else are
      // different claims; check the second one rather than assume it.
      for (const std::string &e : errors) LOG_ERROR_CORE("acl: " + e);
      LOG_ERROR_CORE("Access to " + shared + " is wider than " + acl.audience + " after locking it down.");
      return outcome::still_open;
    }
    LOG_DEBUG_CORE("Restricted " + shared + " to " + acl.audience);
    return outcome::restricted;
  } catch (const std::exception &e) {
    LOG_ERROR_CORE(std::string("Failed to prepare the shared folder: ") + e.what());
    return outcome::failed;
  }
}

}  // namespace shared_folder
}  // namespace core
}  // namespace nsclient

#endif
