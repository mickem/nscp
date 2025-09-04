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

#include "SMTPClient.h"

#include <boost/make_shared.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include "smtp_client.hpp"
#include "smtp_handler.hpp"

/**
 * Default c-tor
 * @return
 */
SMTPClient::SMTPClient()
    : client_("graphite", boost::make_shared<smtp_client::smtp_client_handler>(), boost::make_shared<smtp_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
SMTPClient::~SMTPClient() {}

bool SMTPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  std::wstring template_string, sender, recipient;
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("SMTP", alias, "client");
    client_.set_path(settings.alias().get_settings_path("targets"));

    // clang-format off
    settings.alias().add_path_to_settings()
      ("SMTP CLIENT SECTION", "Section for SMTP passive check module.")
      ("handlers", sh::fun_values_path([this] (auto key, auto value) { this->add_command(key, value); }),
	      "CLIENT HANDLER SECTION", "",
	      "CLIENT HANDLER", "For more configuration options add a dedicated section")

      ("targets", sh::fun_values_path([this] (auto key, auto value) { this->add_target(key, value); }),
	      "REMOTE TARGET DEFINITIONS", "",
	      "TARGET", "For more configuration options add a dedicated section")
      ;

    // clang-format on
    settings.alias().add_key_to_settings().add_string("channel", sh::string_key(&channel_, "SMTP"), "CHANNEL", "The channel to listen to.")

        ;

    settings.register_all();
    settings.notify();

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel_);
  } catch (const nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("load", e);
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

void SMTPClient::add_target(std::string key, std::string arg) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

void SMTPClient::add_command(std::string key, std::string arg) {
  try {
    nscapi::core_helper core(get_core(), get_id());
    std::string k = client_.add_command(key, arg);
    if (!k.empty()) core.register_command(k.c_str(), "SMTP relay for: " + key);
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
bool SMTPClient::unloadModule() {
  client_.clear();
  return true;
}

void SMTPClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

bool SMTPClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  if (target_mode == NSCAPI::target_module) return client_.do_exec(request, response, "_submit");
  return false;
}

void SMTPClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                    PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}