/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent_obj, const std::string alias, const std::string path) override;
  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) override;
};

}  // namespace nsca_ng_handler
