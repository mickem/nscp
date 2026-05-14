/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <list>
#include <nscapi/command_alias.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

class CheckHelpers final : public nscapi::impl::simple_plugin {
 private:
  // Aliases registered via [/settings/check helpers/alias]. Lets admins
  // create predefined commands (like `my_check_cpu = check_cpu warn=load>80`)
  // without enabling CheckExternalScripts, which carries a larger attack
  // surface. See docs/setup/securing.md.
  //
  // Uses simple_command_map (flat key=value section) rather than the heavier
  // object_handler-based command_handler that CheckExternalScripts uses -
  // aliases here are leaf definitions, so the per-alias subdirectory and
  // template/parent machinery just added configuration noise.
  alias::simple_command_map aliases_;

 public:
  CheckHelpers() : aliases_(alias::make_simple_command_map()) {}
  ~CheckHelpers() {}

  // Module lifecycle - load registers the alias settings section.
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Fallback for any command name we don't ourselves implement - used to
  // dispatch alias lookups.
  void query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const PB::Commands::QueryRequestMessage &request_message);

  // Check commands.
  //
  // Wrap-and-dispatch commands take the full request_message as a third
  // argument so they can read the inbound caller identity (the
  // `nscp.caller_plugin_id` / `nscp.principal` metadata stamped by
  // upstream modules like WEBServer/NRPEServer) and forward it to the
  // wrapped command via simple_query_on_behalf_of. This keeps the
  // permission check on the downstream call attributed to the ORIGINAL
  // caller rather than to CheckHelpers itself. Trivial commands that
  // don't dispatch (check_ok, check_warning, check_critical,
  // check_version) keep the simpler 2-arg signature.
  void check_critical(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_warning(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_multi(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                   const PB::Commands::QueryRequestMessage &request_message);
  void check_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_always_warning(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                            const PB::Commands::QueryRequestMessage &request_message);
  void check_always_critical(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                             const PB::Commands::QueryRequestMessage &request_message);
  void check_ok(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_always_ok(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                       const PB::Commands::QueryRequestMessage &request_message);
  void check_negate(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const PB::Commands::QueryRequestMessage &request_message);
  void check_timeout(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const PB::Commands::QueryRequestMessage &request_message);
  void check_and_forward(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                         const PB::Commands::QueryRequestMessage &request_message);

  void filter_perf(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                   const PB::Commands::QueryRequestMessage &request_message);
  void render_perf(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                   const PB::Commands::QueryRequestMessage &request_message);
  void xform_perf(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                  const PB::Commands::QueryRequestMessage &request_message);

  // The forwarded identity threaded through the wrap-and-dispatch path
  // so the wrapped command's permission check sees the original caller.
  // Empty when the inbound request has no identity metadata (e.g. a
  // legacy call site that hasn't been updated yet) - downstream calls
  // then fall back to stamping CheckHelpers' own plugin_id.
  struct forwarded_identity {
    std::string caller_plugin_id;  // verbatim from nscp.caller_plugin_id
    std::string principal;         // verbatim from nscp.principal
  };
  static forwarded_identity extract_identity(const PB::Commands::QueryRequestMessage &request_message);

  // Helpers
  void check_change_status(PB::Common::ResultCode status, const PB::Commands::QueryRequestMessage::Request &request,
                           PB::Commands::QueryResponseMessage::Response *response, const forwarded_identity &id);
  bool simple_query(const std::string &command, const std::vector<std::string> &arguments, PB::Commands::QueryResponseMessage::Response *response,
                    const forwarded_identity &id);
  bool simple_query(const std::string &command, const std::list<std::string> &arguments, PB::Commands::QueryResponseMessage::Response *response,
                    const forwarded_identity &id);

 private:
  void add_alias(const std::string &key, const std::string &command);
  void handle_alias(const alias::simple_command &cd, const std::list<std::string> &args, PB::Commands::QueryResponseMessage::Response *response,
                    const forwarded_identity &id) const;
};