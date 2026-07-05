// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <string>

struct script_provider {
 private:
  nscapi::core_wrapper *core_;
  int id_;
  boost::filesystem::path root_;

 public:
  script_provider(int id, nscapi::core_wrapper *core, boost::filesystem::path root);

  unsigned int get_id();
  nscapi::core_wrapper *get_core();
  std::shared_ptr<nscapi::settings_proxy> get_settings_proxy();

  boost::filesystem::path get_root();
  boost::optional<boost::filesystem::path> find_file(std::string file);
};
