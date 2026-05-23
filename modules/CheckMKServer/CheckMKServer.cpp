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

#include "CheckMKServer.h"

#include <ctime>
#include <net/check_mk/lua/lua_check_mk.hpp>
#include <net/socket/socket_settings_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>

#include "handler_impl.hpp"

namespace sh = nscapi::settings_helper;

CheckMKServer::CheckMKServer() {}
CheckMKServer::~CheckMKServer() {}

bool CheckMKServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  // Resolve to <base>/scripts so find_script finds <base>/scripts/lua/<file>.
  // Using get_base_path() here was a long-standing bug: the auto-add of
  // default_check_mk.lua silently failed because the .lua lives under
  // scripts/, not directly under base/. LuaScript module uses the same
  // expansion.
  root_ = get_core()->expand_path("${scripts}");
  nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
  lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
  lua_runtime_->register_plugin(std::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
  scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));
  handler_.reset(new handler_impl(scripts_));

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("check_mk", alias, "server");

  // clang-format off
  settings.alias().add_path_to_settings()
    ("CHECK MK SERVER SECTION", "Section for check_mk (CheckMKServer.dll) protocol options.")
    ("scripts", sh::fun_values_path([this] (auto key, auto value) { this->add_script(key, value); }),
	    "REMOTE TARGET DEFINITIONS", "",
	    "TARGET", "For more configuration options add a dedicated section")
    // The /local and /mrpe sub-paths are read on demand by default_check_mk.lua
    // (no C++-side callback needed). Registering them here gives them help text
    // and makes them appear in `nscp settings --generate` and the web UI.
    ("local", sh::fun_values_path([](auto, auto) {}),
        "LOCAL CHECK ENTRIES",
        "Each key under this path becomes a Checkmk service in <<<local>>> with the value `command=<nscp-command> [args...]`.\n"
        "Example:\n  CPU Load = command=check_cpu warn=load>80",
        "SERVICE NAME", "command=<nscp-command> [args...]")
    ("mrpe", sh::fun_values_path([](auto, auto) {}),
        "MRPE ENTRIES",
        "Each key under this path becomes a Checkmk service in <<<mrpe>>> with the value `command=<nscp-command> [args...]`.\n"
        "Example:\n  Uptime = command=check_uptime warn=uptime<2d",
        "SERVICE NAME", "command=<nscp-command> [args...]")
    ;
  // clang-format on

  // clang-format off
  settings.alias().add_key_to_settings()
    .add_string("port", sh::string_key(&info_.port_, "6556"), "PORT NUMBER", "Port to use for check_mk.")
    .add_string("mrpe channel", sh::string_key(&channel_mrpe_, "check_mk-mrpe"),
                "MRPE SUBMISSION CHANNEL",
                "Channel name passive check results land on to be relayed as cached <<<mrpe>>> entries.")
    .add_string("local channel", sh::string_key(&channel_local_, "check_mk-local"),
                "LOCAL SUBMISSION CHANNEL",
                "Channel name passive check results land on to be relayed as cached <<<local>>> entries.")
    .add_int("submission ttl", sh::int_key(&submission_ttl_, 60),
             "SUBMITTED RESULT TTL",
             "How long (seconds) a submitted check result is advertised as fresh in the cached(...) header. "
             "Should be at least the scheduler interval that submits it.")
    ;
  // clang-format on

  socket_helpers::settings_helper::add_core_server_opts(settings, info_);
  socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false, "", "${certificate-path}/certificate.pem", "",
                                                       "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

  settings.register_all();
  settings.notify();

  if (scripts_->empty()) {
    add_script("default", "default_check_mk.lua");
  }

  // Register the two passive submission channels. Other modules (e.g. the
  // Scheduler) submit Nagios-format check results to these; CheckMKServer
  // caches the latest result per service name and emits them as cached
  // <<<mrpe>>> / <<<local>>> entries on the next agent fetch.
  {
    nscapi::core_helper core_h(get_core(), get_id());
    if (!channel_mrpe_.empty()) core_h.register_channel(channel_mrpe_);
    if (!channel_local_.empty()) core_h.register_channel(channel_local_);
  }

#ifndef USE_SSL
  if (info_.use_ssl) {
    NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
    return false;
  }
#endif
  NSC_LOG_ERROR_LISTS(info_.validate());

  std::list<std::string> errors;
  info_.allowed_hosts.refresh(errors);
  NSC_LOG_ERROR_LISTS(errors);
  NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

  boost::asio::io_context io_service_;

  scripts_->load_all();

  if (mode == NSCAPI::normalStart) {
    server_.reset(new check_mk::server::server(info_, handler_));
    if (!server_) {
      NSC_LOG_ERROR_STD("Failed to create server instance!");
      return false;
    }
    server_->start();
  }
  return true;
}

void CheckMKServer::prepareShutdown() {
  // Stop accepting new connections and join the I/O threads while every peer
  // plugin is still loaded, so any in-flight check_mk query can complete
  // cleanly before unloadModule tears state down.
  try {
    if (server_) {
      server_->stop();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("prepare_shutdown");
  }
}

bool CheckMKServer::unloadModule() {
  try {
    if (server_) {
      server_->stop();
      server_.reset();
    }
    scripts_.reset();
    lua_runtime_.reset();
    nscp_runtime_.reset();
    handler_.reset();
  } catch (...) {
    NSC_LOG_ERROR_EX("unload");
    return false;
  }
  return true;
}

void CheckMKServer::submitMetrics(const PB::Metrics::MetricsMessage &metrics) { check_mk::shared_metrics_store().set(metrics); }

namespace {
int result_to_status_code(PB::Common::ResultCode r) {
  using namespace PB::Common;
  switch (r) {
    case Result_StatusCodeType_STATUS_OK:
      return 0;
    case Result_StatusCodeType_STATUS_WARNING:
      return 1;
    case Result_StatusCodeType_STATUS_ERROR:
      return 2;
    default:
      return 3;
  }
}
}  // namespace

void CheckMKServer::handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                                       PB::Commands::SubmitResponseMessage *response_message) {
  check_mk::submission_store::kind kind;
  if (channel == channel_mrpe_) {
    kind = check_mk::submission_store::kind_mrpe;
  } else if (channel == channel_local_) {
    kind = check_mk::submission_store::kind_local;
  } else {
    nscapi::protobuf::functions::set_response_bad(*response_message->add_payload(), "Unknown channel: " + channel);
    return;
  }

  const long long now = static_cast<long long>(std::time(nullptr));
  for (const PB::Commands::QueryResponseMessage::Response &payload : request_message.payload()) {
    check_mk::cached_result r;
    r.generated = now;
    r.ttl = submission_ttl_;
    r.code = result_to_status_code(payload.result());

    // Concatenate all response lines into a single message + the perf data
    // re-rendered as a Nagios-style string. Most submitters only emit one line
    // anyway; multi-line is collapsed with a single space between fragments.
    std::string message, perf;
    for (const PB::Commands::QueryResponseMessage::Response::Line &line : payload.lines()) {
      if (!message.empty()) message += " ";
      message += line.message();
      const std::string line_perf = nscapi::protobuf::functions::build_performance_data(line, 0);
      if (!line_perf.empty()) {
        if (!perf.empty()) perf += " ";
        perf += line_perf;
      }
    }
    r.message = std::move(message);
    r.perf = std::move(perf);

    // The scheduler puts the schedule's alias (e.g. "Scheduled_OK") in
    // payload.alias() and leaves payload.command() as the underlying check
    // command (e.g. "check_ok"). Prefer the alias as the Checkmk service
    // name; fall back to command if no alias was set.
    const std::string &name = payload.alias().empty() ? payload.command() : payload.alias();
    check_mk::shared_submission_store().put(kind, name, std::move(r));

    PB::Commands::SubmitResponseMessage::Response *resp = response_message->add_payload();
    resp->set_command(name);
    resp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  }
}

bool CheckMKServer::add_script(std::string alias, std::string file) {
  try {
    if (file.empty()) {
      file = alias;
      alias = "";
    }

    boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
    if (!ofile) {
      NSC_LOG_ERROR("Failed to find script: " + file);
      return false;
    }
    NSC_DEBUG_MSG_STD("Adding script: " + ofile->string());
    scripts_->add(alias, ofile->string());
    return true;
  } catch (...) {
    NSC_LOG_ERROR_EX("add script");
  }
  return false;
}