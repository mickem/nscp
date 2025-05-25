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

#include "SyslogClient.h"

#include "syslog_client.hpp"
#include "syslog_handler.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <str/utils.hpp>
#include <nscapi/macros.hpp>

#include <boost/make_shared.hpp>
#include <boost/asio.hpp>

namespace ph = boost::placeholders;

/**
 * Default c-tor
 * @return
 */
SyslogClient::SyslogClient()
    : client_("syslog", boost::make_shared<syslog_client::syslog_client_handler>(), boost::make_shared<syslog_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
SyslogClient::~SyslogClient() {}

bool SyslogClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("syslog", alias, "client");
    client_.set_path(settings.alias().get_settings_path("targets"));

    // clang-format off
		settings.alias().add_path_to_settings()
			("SYSLOG CLIENT SECTION", "Section for SYSLOG passive check module.")
			("handlers", sh::fun_values_path(boost::bind(&SyslogClient::add_command, this, ph::_1, ph::_2)),
				"CLIENT HANDLER SECTION", "",
				"CLIENT", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&SyslogClient::add_target, this, ph::_1, ph::_2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")
			;

    // clang-format on
    settings.alias()
        .add_key_to_settings()
        .add_string("hostname", sh::string_key(&hostname_, "auto"), "HOSTNAME",
                    "The host name of the monitored computer.\nSet this to auto (default) to use the windows name of the computer.\n\n"
                    "auto\tHostname\n"
                    "${host}\tHostname\n"
                    "${host_lc}\nHostname in lowercase\n"
                    "${host_uc}\tHostname in uppercase\n"
                    "${domain}\tDomainname\n"
                    "${domain_lc}\tDomainname in lowercase\n"
                    "${domain_uc}\tDomainname in uppercase\n")

        .add_string("channel", sh::string_key(&channel_, "syslog"), "CHANNEL", "The channel to listen to.");

    settings.register_all();
    settings.notify();

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel_);

    if (hostname_ == "auto") {
      hostname_ = boost::asio::ip::host_name();
    } else if (hostname_ == "auto-lc") {
      hostname_ = boost::asio::ip::host_name();
      std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
    } else if (hostname_ == "auto-uc") {
      hostname_ = boost::asio::ip::host_name();
      std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::toupper);
    } else {
      str::utils::token dn = str::utils::getToken(boost::asio::ip::host_name(), '.');

      try {
        boost::asio::io_service svc;
        boost::asio::ip::tcp::resolver resolver(svc);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
        boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query), end;

        std::string s;
        while (iter != end) {
          s += iter->host_name();
          s += " - ";
          s += iter->endpoint().address().to_string();
          iter++;
        }
      } catch (const std::exception &e) {
        NSC_LOG_ERROR_EXR("Failed to resolve: ", e);
      }

      str::utils::replace(hostname_, "${host}", dn.first);
      str::utils::replace(hostname_, "${domain}", dn.second);
      std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
      std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
      str::utils::replace(hostname_, "${host_uc}", dn.first);
      str::utils::replace(hostname_, "${domain_uc}", dn.second);
      std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
      std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
      str::utils::replace(hostname_, "${host_lc}", dn.first);
      str::utils::replace(hostname_, "${domain_lc}", dn.second);
    }
  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("NSClient API exception: ");
    return false;
  }
  return true;
}

void SyslogClient::add_target(std::string key, std::string arg) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

void SyslogClient::add_command(std::string key, std::string arg) {
  try {
    nscapi::core_helper core(get_core(), get_id());
    std::string k = client_.add_command(key, arg);
    if (!k.empty()) core.register_command(k.c_str(), "Syslog relay for: " + key);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add command: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add command: " + key);
  }
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool SyslogClient::unloadModule() {
  client_.clear();
  return true;
}

void SyslogClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

bool SyslogClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  if (target_mode == NSCAPI::target_module) return client_.do_exec(request, response, "_submit");
  return false;
}

void SyslogClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                      PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}