// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <memory>
#include <string>

#include "python_script.hpp"
#include <nscp/script_roots.hpp>

#include "script_interface.hpp"

struct script_provider : public script_provider_interface {
 private:
  nscapi::core_wrapper *core_;
  int id_;
  boost::filesystem::path root_;
  boost::shared_mutex mutex_;

  typedef std::list<std::shared_ptr<python_script>> instance_list_type;
  instance_list_type instances_;
  // Folders a configured script may be loaded from. Set before any script
  // is added; an empty list refuses everything, which is what a caller that
  // forgot to set it should get.
  nscp::scripts::allowed_roots allowed_roots_;

 public:
  script_provider(int id, nscapi::core_wrapper *core, boost::filesystem::path root);

  unsigned int get_id();
  nscapi::core_wrapper *get_core();
  std::shared_ptr<nscapi::settings_proxy> get_settings_proxy();

  void set_allowed_roots(nscp::scripts::allowed_roots roots) override { allowed_roots_ = std::move(roots); }

  boost::filesystem::path get_root();
  boost::optional<boost::filesystem::path> find_file(std::string file);

  void add_command(std::string script_alias, std::string script, std::string plugin_alias);
  void remove_command(std::string alias);
  void clear();
};
