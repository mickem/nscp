// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/settings/helper.hpp>

namespace nsca_ng_handler {

// Settings/config object for one NSCA-NG target. Reads PSK and TLS keys from
// `[/settings/NSCA-NG/client/targets/<name>]` plus the standard SSL block
// inherited from `target_object`.
struct nsca_ng_target_object : public nscapi::targets::target_object {
  using parent = nscapi::targets::target_object;

  nsca_ng_target_object(const std::string &alias, const std::string &path);
  nsca_ng_target_object(const nscapi::settings_objects::object_instance &other, const std::string &alias, const std::string &path);

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, const bool is_sample) override;
};

// CLI options bridge: maps `--password`, `--identity`, `--no-psk`, etc. into
// the destination_container that the client handler reads.
struct options_reader_impl : public client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override;
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent_obj, const std::string alias,
                                                  const std::string path) override;
  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) override;
};

}  // namespace nsca_ng_handler
