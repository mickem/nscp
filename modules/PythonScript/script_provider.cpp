// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "script_provider.hpp"

#include <boost/thread.hpp>
#include <file_helpers.hpp>
#include <memory>
#include <str/utils.hpp>

script_provider::script_provider(int id, nscapi::core_wrapper* core, boost::filesystem::path root) : core_(core), id_(id), root_(root) {}

unsigned int script_provider::get_id() { return id_; }

nscapi::core_wrapper* script_provider::get_core() { return core_; }

std::shared_ptr<nscapi::settings_proxy> script_provider::get_settings_proxy() { return std::make_shared<nscapi::settings_proxy>(get_id(), get_core()); }

// root_ is already ${scripts}, so this must not prepend "scripts" again.
// It used to, which made the import destination and the sandbox root
// ${scripts}/scripts/python - a folder find_file never searches and that
// does not exist on a normal install.
boost::filesystem::path script_provider::get_root() { return root_ / "python"; }

boost::optional<boost::filesystem::path> script_provider::find_file(std::string file) {
  std::list<boost::filesystem::path> checks;
  checks.push_back(file);
  checks.push_back(file + ".py");
  checks.push_back(root_ / "python" / file);
  checks.push_back(root_ / "python" / (file + ".py"));
  checks.push_back(root_ / file);
  checks.push_back(root_ / (file + ".py"));
  for (boost::filesystem::path c : checks) {
    if (boost::filesystem::exists(c) && boost::filesystem::is_regular_file(c)) return boost::optional<boost::filesystem::path>(c);
  }
  get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Script not found: " + file + " looking in " + get_root().string());
  return boost::optional<boost::filesystem::path>();
}

void script_provider::add_command(std::string script_alias, std::string script, std::string plugin_alias) {
  try {
    if (script.empty()) {
      script = script_alias;
      script_alias = "";
    }
    boost::optional<boost::filesystem::path> ofile = find_file(script);
    if (!ofile) {
      get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to find script: " + script);
      return;
    }
    // The search ends with the value joined onto the script folder, which does
    // not stop it climbing back out: `../foo.py` resolves to ${scripts}/../foo.py
    // and would otherwise load from the installation directory. The ext-scr CLI
    // has always held show/delete inside the script root; this is the same check
    // on the path that actually runs code.
    if (!allowed_roots_.allows(ofile.value())) {
      get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__,
                      "Refusing to load script outside the allowed roots: " + ofile.value().string() + " (allowed: " + allowed_roots_.describe() +
                          "). Add its folder to 'additional script roots' under the python section if it belongs there.");
      return;
    }
    std::string script_file = ofile.value().string();
    get_core()->log(NSCAPI::log_level::debug, __FILE__, __LINE__, "Adding script: " + script_alias + " (" + script_file + ")");

    std::shared_ptr<python_script> instance = std::make_shared<python_script>(get_id(), root_.string(), plugin_alias, script_alias, script_file);
    {
      boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
      if (!writeLock.owns_lock()) {
        get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: add_command");
        return;
      }
      instances_.push_back(instance);
    }
  } catch (...) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to add script: " + script);
  }
}

void script_provider::remove_command(std::string alias) {
  boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
  if (!writeLock.owns_lock()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: remove_command");
  }
}

void script_provider::clear() {
  boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
  if (!writeLock.owns_lock()) {
    get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to get mutex: remove_command");
    return;
  }
  instances_.clear();
}
