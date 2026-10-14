// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <scripts/script_root.hpp>
#include <string>

#include "commands.hpp"

struct script_provider_interface {
  virtual ~script_provider_interface() = default;
  virtual unsigned int get_id() = 0;
  virtual nscapi::core_wrapper* get_core() = 0;
  virtual std::shared_ptr<nscapi::settings_proxy> get_settings_proxy() = 0;

  // The scripts folder: what `show`, `delete` and `add --import` are confined
  // to (${scripts}, or `<script root>/scripts`).
  virtual boost::filesystem::path get_root() = 0;
  // The whole `script root` layout, for the working directory a script runs
  // in and the base an imported script is recorded relative to.
  virtual scripts::root::layout get_layout() = 0;
  virtual std::string generate_wrapped_command(std::string command) = 0;

  virtual void add_command(std::string alias, std::string script) = 0;
  virtual commands::command_object_instance find_command(std::string alias) = 0;
  virtual void remove_command(std::string alias) = 0;
  virtual std::list<std::string> get_commands() = 0;
};
