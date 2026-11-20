// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <functional>
#include <list>
#include <nscp/path_defaults.hpp>
#include <nscp/service_account.hpp>
#include <string>
#include <vector>

namespace nsclient {
namespace core {

// Changing the account an installed service runs as: the steps, and above all
// their order. The Windows calls that carry each step out are handed in, so
// the sequence can be tested without a service to reconfigure.
//
// The order is the point. Three places have to agree - the service's
// registration, the grant on the shared folder, and the `[service]` record in
// boot.ini that tells every other process to expect that grant - and they
// cannot be changed atomically. So each direction is arranged for the states
// in between to be harmless:
//
//  - Towards the virtual account the folder is opened to it first, then the
//    record is written, and the service is re-registered last. Stopping
//    anywhere leaves a service that still runs as SYSTEM, beside a grant to an
//    account nothing runs as.
//  - Back to LocalSystem the service is re-registered first, and the grant and
//    the record go last. Stopping anywhere leaves a service that runs as
//    SYSTEM, which can read the folder whatever else is true.
//
// The state that must never be left behind is the other one: a service
// registered as the virtual account while boot.ini still says LocalSystem. The
// next elevated `nscp` would find the account's grant on the folder, take it
// for an intruder, and remove it.
namespace service_account_switch {

struct operations {
  // Whether a service of that name is registered at all.
  std::function<bool(const std::string &service)> is_installed;
  // Restrict `folder` to SYSTEM, Administrators and `service_sids`.
  std::function<bool(const std::string &folder, const std::vector<std::string> &service_sids, std::list<std::string> &errors)> protect;
  // Write one key of boot.ini's [service] section. Throws when it cannot.
  std::function<void(const std::string &key, const std::string &value)> record;
  // The three service registration changes. Each throws when it fails.
  std::function<void(const std::string &service, const std::string &account)> set_account;
  std::function<void(const std::string &service)> set_unrestricted_sid;
  std::function<void(const std::string &service, const std::vector<std::string> &privileges)> set_required_privileges;
  // Add `member_sid` to, or remove it from, the local group `group_sid`.
  std::function<bool(const std::string &member_sid, const std::string &group_sid, bool add, std::string &error)> set_group_membership;
  // What the operator is told.
  std::function<void(const std::string &)> info;
  std::function<void(const std::string &)> error;
};

struct request {
  nscp::service_account::kind wanted = nscp::service_account::kind::system;
  std::string service_name;
  nscp::paths::layout layout = nscp::paths::layout::legacy;
  // ${shared-path}, resolved by the caller.
  std::string shared_folder;
};

enum class outcome {
  // Not attempted, and nothing was touched: the request cannot be honoured on
  // this installation. The operator has been told what to do instead.
  refused,
  // Something failed part-way. The service still runs as LocalSystem, or does
  // again; the message says what was and was not changed.
  failed,
  // Done. A group membership that could not be changed is reported and does
  // not change this answer: the service runs, with less to read.
  switched,
};

namespace detail {

inline outcome to_virtual(const request &what, const operations &ops) {
  const std::string sid = nscp::service_account::service_sid(what.service_name);
  nscp::service_account::identity who;
  who.account = nscp::service_account::kind::virtual_account;
  who.service_name = what.service_name;
  const std::string account = nscp::service_account::account_name(who);

  std::list<std::string> acl_errors;
  if (!ops.protect(what.shared_folder, {sid}, acl_errors)) {
    for (const std::string &e : acl_errors) ops.error("acl: " + e);
    ops.error("Could not give " + account + " access to " + what.shared_folder + ". Nothing else was changed; the service still runs as LocalSystem.");
    return outcome::failed;
  }

  try {
    ops.record("name", what.service_name);
    ops.record("account", nscp::service_account::name(nscp::service_account::kind::virtual_account));
  } catch (const std::exception &e) {
    ops.error(std::string("Could not record the service account in boot.ini: ") + e.what());
    ops.error("The service still runs as LocalSystem. " + account + " was given access to " + what.shared_folder +
              ", which is harmless while nothing runs as that account; run `nscp service --account system` to take it away again.");
    return outcome::failed;
  }

  try {
    ops.set_unrestricted_sid(what.service_name);
    ops.set_required_privileges(what.service_name, nscp::service_account::required_privileges());
    // Last: until this one succeeds the service is still LocalSystem, which
    // works with everything done so far.
    ops.set_account(what.service_name, account);
  } catch (const std::exception &e) {
    ops.error(std::string("Could not change the service registration: ") + e.what());
    ops.error("The service still runs as LocalSystem, but boot.ini and " + what.shared_folder +
              " are already prepared for " + account + ". Run this again once the cause is fixed, or `nscp service --account system` to undo it.");
    return outcome::failed;
  }

  for (const std::string &group : nscp::service_account::group_sids()) {
    std::string group_error;
    if (!ops.set_group_membership(sid, group, true, group_error)) {
      ops.error("Could not add the service account to a local group: " + group_error +
                ". Checks that read performance counters or event logs may report less.");
    }
  }

  ops.info("The service '" + what.service_name + "' now runs as " + account + " (" + sid + ").");
  ops.info("Restart the service for it to take effect.");
  return outcome::switched;
}

inline outcome to_system(const request &what, const operations &ops) {
  try {
    // First: once this succeeds the service can read its folder no matter
    // what state the rest is in.
    ops.set_account(what.service_name, "LocalSystem");
    ops.set_required_privileges(what.service_name, std::vector<std::string>());
  } catch (const std::exception &e) {
    ops.error(std::string("Could not change the service registration: ") + e.what());
    ops.error("Nothing else was changed.");
    return outcome::failed;
  }

  // An empty SID means the name is one the virtual account was never available
  // for, so there is no membership and no grant to take away.
  const std::string sid = nscp::service_account::service_sid(what.service_name);
  if (!sid.empty()) {
    for (const std::string &group : nscp::service_account::group_sids()) {
      std::string group_error;
      if (!ops.set_group_membership(sid, group, false, group_error)) {
        ops.error("Could not remove the service account from a local group: " + group_error);
      }
    }
  }

  try {
    ops.record("account", nscp::service_account::name(nscp::service_account::kind::system));
  } catch (const std::exception &e) {
    ops.error(std::string("Could not record the service account in boot.ini: ") + e.what());
    ops.error("The service runs as LocalSystem again, but boot.ini still records the virtual account, so its access to " + what.shared_folder +
              " was left in place. Run this again once the cause is fixed.");
    return outcome::failed;
  }

  // Only the modern layout's folder ever carried the grant.
  if (what.layout == nscp::paths::layout::modern && !what.shared_folder.empty()) {
    std::list<std::string> acl_errors;
    if (!ops.protect(what.shared_folder, std::vector<std::string>(), acl_errors)) {
      for (const std::string &e : acl_errors) ops.error("acl: " + e);
      ops.error("The service runs as LocalSystem again, but the old service account's access to " + what.shared_folder + " could not be removed.");
      return outcome::failed;
    }
  }

  ops.info("The service '" + what.service_name + "' now runs as LocalSystem.");
  ops.info("Restart the service for it to take effect.");
  return outcome::switched;
}

}  // namespace detail

// Whether `what` can be honoured at all, without touching anything. Split out
// so `nscp service --install --account virtual` can ask before it installs a
// service it would then have to leave half-configured.
inline std::string refusal(const request &what) {
  return nscp::service_account::refusal(what.wanted, what.service_name, what.layout);
}

inline outcome apply(const request &what, const operations &ops) {
  const std::string why_not = refusal(what);
  if (!why_not.empty()) {
    ops.error(why_not);
    return outcome::refused;
  }
  if (!ops.is_installed(what.service_name)) {
    ops.error("There is no service named '" + what.service_name + "'. Install it and choose the account in one step with `nscp service --install --account " +
              nscp::service_account::name(what.wanted) + "`.");
    return outcome::refused;
  }
  if (what.wanted == nscp::service_account::kind::virtual_account) return detail::to_virtual(what, ops);
  return detail::to_system(what, ops);
}

}  // namespace service_account_switch
}  // namespace core
}  // namespace nsclient
