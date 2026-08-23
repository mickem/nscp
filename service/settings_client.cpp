// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "settings_client.hpp"

#include <config.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscp/layout_migration.hpp>
#include <nscp/path_defaults.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"
#ifdef WIN32
#include <win/acl.hpp>
#endif
#include <list>

settings::settings_core *nsclient_core::settings_client::get_core() const { return settings_manager::get_core(); }

nsclient_core::settings_client::settings_client(std::shared_ptr<NSClient> core, bool update_defaults, bool remove_defaults, bool load_all, bool use_samples)
    : started_(false), core_(core), default_(update_defaults), remove_default_(remove_defaults), load_all_(load_all), use_samples_(use_samples) {
  startup();
}

nsclient_core::settings_client::~settings_client() { terminate(); }

void nsclient_core::settings_client::startup() {
  if (started_) return;
  if (!core_->load_configuration_1()) {
    std::cout << "boot::init::1 failed" << std::endl;
    return;
  }
  if (!core_->load_configuration_2(true)) {
    std::cout << "boot::init::2 failed" << std::endl;
    return;
  }
  if (load_all_) core_->boot_load_all_plugin_files();

  if (!core_->boot_load_active_plugins()) {
    std::cout << "boot::load_all_plugins failed!" << std::endl;
    return;
  }
  if (!core_->boot_start_plugins(false)) {
    std::cout << "boot::start_plugins failed!" << std::endl;
    return;
  }
  if (default_) {
    get_core()->update_defaults();
  }
  if (remove_default_) {
    std::cout << "Removing default values" << std::endl;
    get_core()->remove_defaults();
  }
  started_ = true;
}

std::string nsclient_core::settings_client::expand_context(const std::string &key) const { return get_core()->expand_context(key); }
void nsclient_core::settings_client::terminate() {
  if (!started_) return;
  core_->stop_nsclient();
  started_ = false;
}

int nsclient_core::settings_client::migrate_from(std::string src) {
  try {
    debug_msg(__FILE__, __LINE__, "Migrating from: " + expand_context(src));
    // Handed over as written: create_instance expands what it opens itself.
    get_core()->migrate_from("master", src);
    return 1;
  } catch (settings::settings_exception &e) {
    error_msg(__FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
  }
  return -1;
}
int nsclient_core::settings_client::migrate_to(std::string target) {
  try {
    debug_msg(__FILE__, __LINE__, "Migrating to: " + expand_context(target));
    // Handed over as written, like --switch: migrate_to ends in set_primary,
    // which writes the context to boot.ini, and a host name placeholder the
    // operator typed is a template for the whole fleet - expanding it first
    // would store this host's name instead (issue #458). create_instance
    // expands what it opens itself.
    get_core()->migrate_to("master", target);
    return 1;
  } catch (const settings::settings_exception &e) {
    error_msg(e.file(), e.line(), "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
  }
  return -1;
}

int nsclient_core::settings_client::migrate_layout(const std::string &mode, const bool dry_run) {
  try {
    if (!nscp::paths::is_known_layout(mode)) {
      error_msg(__FILE__, __LINE__, "Unknown layout '" + mode + "'. Use 'modern' or 'legacy'.");
      return -1;
    }
    const nscp::paths::layout target_layout = nscp::paths::parse_layout(mode);
    const nscp::paths::layout current_layout = core_->get_path()->get_layout();

    // Resolve the source with the layout that is in force *now*, and the
    // destination by asking what the other layout would answer. Doing it in
    // that order is what makes this work while the agent is still configured
    // the old way.
    const std::string from = core_->get_path()->expand_path("${shared-path}");
    core_->get_path()->set_layout(target_layout);
    const std::string to = core_->get_path()->expand_path("${shared-path}");
    core_->get_path()->set_layout(current_layout);

    if (from == to) {
      std::cout << "Already using the " << nscp::paths::layout_name(target_layout) << " layout (" << from << "); nothing to do." << std::endl;
      return 1;
    }
    std::cout << (dry_run ? "Would migrate" : "Migrating") << " from " << from << std::endl << "                to " << to << std::endl << std::endl;

    // Switching into the modern layout means moving the configuration and the
    // fleet private key into a folder we are about to create and lock down; it
    // must be empty first, or a file already there is silently adopted as the
    // agent's own (round-2 #3). from == to above has already ruled out an
    // already-modern re-run, so reaching here with a modern target is a genuine
    // first switch.
    const nscp::paths::destination_policy policy =
        target_layout == nscp::paths::layout::modern ? nscp::paths::destination_policy::require_pristine : nscp::paths::destination_policy::adopt_existing;

    if (dry_run) {
      const nscp::paths::migration_report plan = nscp::paths::plan_migration(from, to, policy);
      for (const std::string &line : plan.describe()) std::cout << "  " << line << std::endl;
      std::cout << std::endl << "Nothing was changed. Re-run without --dry-run to apply, then restart the service." << std::endl;
      return plan.ok() ? 1 : -1;
    }

    // The destination has to exist *and* be locked down before any secret is
    // written into it - the configuration holds passwords and the fleet
    // identity is a private key. apply_migration refuses a destination that
    // does not exist for exactly this reason.
    boost::system::error_code ec;
    boost::filesystem::create_directories(to, ec);
    if (ec) {
      error_msg(__FILE__, __LINE__, "Failed to create " + to + ": " + ec.message() + " (try an elevated prompt)");
      return -1;
    }
#ifdef WIN32
    if (target_layout == nscp::paths::layout::modern) {
      std::list<std::string> acl_errors;
      if (!nsclient::windows_acl::protect_directory(to, acl_errors)) {
        for (const std::string &e : acl_errors) error_msg(__FILE__, __LINE__, "acl: " + e);
        error_msg(__FILE__, __LINE__, "Refusing to migrate into " + to + ": it could not be restricted to SYSTEM and Administrators.");
        return -1;
      }
      // Locking the destination down can lock *us* out: the DACL grants SYSTEM
      // and Administrators, and a process that is not elevated does not carry
      // the Administrators group in its token. Find that out here, with one
      // sentence, rather than through a wall of "Access is denied" once the
      // move is under way.
      const boost::filesystem::path probe = boost::filesystem::path(to) / ".nscp-migrate-probe";
      boost::system::error_code probe_ec;
      {
        std::ofstream(probe.string().c_str());
      }
      if (!boost::filesystem::exists(probe, probe_ec)) {
        error_msg(__FILE__, __LINE__, "Cannot write to " + to + " after restricting it to SYSTEM and Administrators.");
        error_msg(__FILE__, __LINE__, "Run this from an elevated prompt (\"Run as administrator\"); nothing has been moved.");
        return -1;
      }
      boost::filesystem::remove(probe, probe_ec);
    }
#endif

    const nscp::paths::migration_report report = nscp::paths::apply_migration(from, to, policy);
    for (const std::string &line : report.describe()) std::cout << "  " << line << std::endl;
    if (!report.ok()) {
      error_msg(__FILE__, __LINE__, "Migration failed; the configuration has NOT been switched to the new layout.");
      return -1;
    }

    // Only now: the switch is what makes the agent look in the new place, so
    // it must not be written until the files are actually there.
    settings_manager::write_boot_ini_key("layout", "mode", nscp::paths::layout_name(target_layout));
    std::cout << std::endl
              << "Switched to the " << nscp::paths::layout_name(target_layout) << " layout." << std::endl
              << "Restart the service for it to take effect." << std::endl;
    return 1;
  } catch (const settings::settings_exception &e) {
    error_msg(e.file(), e.line(), "Failed to record the layout in boot.ini: " + utf8::utf8_from_native(e.what()));
  } catch (const std::exception &e) {
    error_msg(__FILE__, __LINE__, std::string("Failed to migrate the layout: ") + e.what());
  } catch (...) {
    error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
  }
  return -1;
}
namespace {
// Mask values for keys registered sensitive (add_password / is_sensitive_key)
// so a shared `--list` dump or an accidental `--show` does not spill secrets.
// `--list`/`--show` are local tools, so this is defence-in-depth (the caller
// can still read nsclient.ini), and it keeps CLI output consistent with the
// masking the REST read paths and /diff already apply. Only keys whose module
// registered them sensitive are masked; an unknown backend returns false.
std::string redacted_for_display(settings::settings_core *core, const std::string &path, const std::string &key, const std::string &value) {
  try {
    if (core->is_sensitive_key(path, key)) return "***";
  } catch (...) {
  }
  return value;
}
}  // namespace

void nsclient_core::settings_client::dump_path(std::string root) {
  for (const std::string &path : get_core()->get()->get_sections(root)) {
    if (!root.empty()) {
      dump_path(root + "/" + path);
    } else if (!path.empty()) {
      dump_path(path);
    }
  }
  for (std::string key : get_core()->get()->get_keys(root)) {
    settings::settings_interface::op_string val = get_core()->get()->get_string(root, key);
    if (val) std::cout << root << "." << key << "=" << redacted_for_display(get_core(), root, key, *val) << std::endl;
  }
}

int nsclient_core::settings_client::generate(std::string target) {
  try {
    auto new_context = expand_context(target);
    if (target == "settings" || target.empty() || get_core()->get()->get_context() == new_context) {
      get_core()->get()->save(true);
    } else {
      get_core()->get()->save_to("master", new_context);
    }
    return 0;
  } catch (settings::settings_exception &e) {
    error_msg(__FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return 1;
  } catch (nsclient::core::plugin_exception &e) {
    error_msg(__FILE__, __LINE__, "Failed to load plugins: " + e.reason());
    return 1;
  } catch (std::exception &e) {
    error_msg(__FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return 1;
  } catch (...) {
    error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
    return 1;
  }
}

// The context is handed over unexpanded: set_primary writes it to boot.ini, and
// a host name placeholder the operator typed is a template for the whole fleet,
// not a request to store this host's name (issue #458). set_primary resolves
// the protocol aliases itself.
void nsclient_core::settings_client::switch_context(std::string context) { get_core()->set_primary(context); }

int nsclient_core::settings_client::set(std::string path, std::string key, std::string val) {
  get_core()->get()->set_string(path, key, val);
  get_core()->get()->save(false);
  return 0;
}
void list_settings_context_info(int padding, settings::instance_ptr instance) {
  std::string pad = std::string(padding, ' ');
  std::cout << pad << instance->get_info() << std::endl;
  for (settings::instance_ptr child : instance->get_children()) {
    list_settings_context_info(padding + 2, child);
  }
}

int nsclient_core::settings_client::show(std::string path, std::string key) {
  if (path.empty() && key.empty())
    list_settings_context_info(2, settings_manager::get_settings());
  else {
    settings::settings_interface::op_string val = get_core()->get()->get_string(path, key);
    if (val) std::cout << redacted_for_display(get_core(), path, key, *val);
  }
  return 0;
}
int nsclient_core::settings_client::list(std::string path) {
  try {
    dump_path(path);
  } catch (settings::settings_exception &e) {
    error_msg(__FILE__, __LINE__, "Settings error: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
  }

  return 0;
}
int nsclient_core::settings_client::validate() {
  settings::error_list errors = get_core()->validate();
  for (const std::string &e : errors) {
    std::cerr << e << std::endl;
  }
  return 0;
}

int nsclient_core::settings_client::sort() {
  try {
    debug_msg(__FILE__, __LINE__, "Sorting settings store: " + get_core()->get()->get_context());
    get_core()->get()->save_sorted();
    std::cout << "Sorted settings written to: " << get_core()->get()->get_context() << std::endl;
    return 0;
  } catch (settings::settings_exception &e) {
    error_msg(__FILE__, __LINE__, "Failed to sort settings: " + utf8::utf8_from_native(e.what()));
  } catch (std::exception &e) {
    error_msg(__FILE__, __LINE__, "Failed to sort settings: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    error_msg(__FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
  }
  return 1;
}

void nsclient_core::settings_client::error_msg(const char *file, const int line, std::string msg) {
  core_->get_logger()->error("client", file, line, msg.c_str());
}
void nsclient_core::settings_client::debug_msg(const char *file, const int line, std::string msg) {
  core_->get_logger()->debug("client", file, line, msg.c_str());
}

void nsclient_core::settings_client::list_settings_info() {
  std::cout << "Current settings instance loaded: " << std::endl;
  list_settings_context_info(2, settings_manager::get_settings());
}
int nsclient_core::settings_client::activate(const std::vector<std::string> &modules) {
  int ret = 0;
  for (const std::string &module : modules) {
    if (!core_->boot_load_single_plugin(module)) {
      std::cerr << "Failed to load module (Wont activate): " << module << std::endl;
      ret = -1;
      continue;
    }
    get_core()->get()->set_string(MAIN_MODULES_SECTION, module, "enabled");
  }
  core_->boot_start_plugins(false);
  if (default_) {
    get_core()->update_defaults();
  }
  get_core()->get()->save(false);
  return ret;
}