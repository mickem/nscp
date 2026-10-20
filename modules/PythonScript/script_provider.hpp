// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <memory>
#include <string>

#include "python_script.hpp"
#include "script_interface.hpp"

struct script_provider : public script_provider_interface {
 private:
  nscapi::core_wrapper *core_;
  int id_;
  // The scripts folder. Replaced whole on a reload that moves it (a changed
  // `script root`) and read without the instances' mutex: an atomic
  // shared_ptr, as LUAScript publishes its generations, so a lookup racing
  // the reload sees the old folder or the new one, never a torn path.
  std::shared_ptr<const boost::filesystem::path> root_;
  boost::shared_mutex mutex_;

  typedef std::list<std::shared_ptr<python_script>> instance_list_type;
  instance_list_type instances_;

 public:
  script_provider(int id, nscapi::core_wrapper *core, boost::filesystem::path root);

  unsigned int get_id();
  nscapi::core_wrapper *get_core();
  std::shared_ptr<nscapi::settings_proxy> get_settings_proxy();

  boost::filesystem::path get_root();
  void set_scripts_folder(const boost::filesystem::path &scripts);
  // The scripts folder as of this call: `script root`, ${scripts} by default.
  boost::filesystem::path scripts_folder() const;
  boost::optional<boost::filesystem::path> find_file(std::string file);

  void add_command(std::string script_alias, std::string script, std::string plugin_alias);
  void remove_command(std::string alias);
  void clear();
};
