// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NSCPClient.h"

#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>

#include "nscp_client.hpp"
#include "nscp_handler.hpp"

namespace sh = nscapi::settings_helper;

/**
 * Default c-tor
 * @return
 */
NSCPClient::NSCPClient() : client_("nscp", std::make_shared<nscp_client::nscp_client_handler<> >(), std::make_shared<nscp_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
NSCPClient::~NSCPClient() {}

bool NSCPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    client_.clear();
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("NSCP", alias, "client");

    client_.set_path(settings.alias().get_settings_path("targets"));

    // clang-format off
    settings.alias().add_path_to_settings()
      ("NSCP CLIENT SECTION", "Section for NSCP active/passive check module.")

      ("handlers", sh::fun_values_path([this] (auto key, auto value) { this->add_command(key, value); }),
	      "CLIENT HANDLER SECTION", "",
	      "TARGET", "For more configuration options add a dedicated section")

      ("targets", sh::fun_values_path([this] (auto key, auto value) { this->add_target(key, value); }),
	      "REMOTE TARGET DEFINITIONS", "",
	      "TARGET", "For more configuration options add a dedicated section")
      ;
    // clang-format on

    settings.alias().add_key_to_settings().add_string("channel", sh::string_key(&channel_, "NSCP"), "CHANNEL", "The channel to listen to.")

        ;

    settings.register_all();
    settings.notify();

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel_);
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("loading", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading");
    return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NSCPClient::add_target(std::string key, std::string arg) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

void NSCPClient::add_command(std::string name, std::string args) {
  try {
    nscapi::core_helper core(get_core(), get_id());
    std::string key = client_.add_command(name, args);
    if (!key.empty()) core.register_command(key.c_str(), "NSCP relay for: " + name);
  } catch (boost::program_options::validation_error &e) {
    NSC_LOG_ERROR_EXR("Failed to add command: " + name, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add command: " + name);
  }
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NSCPClient::unloadModule() { return true; }

void NSCPClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

bool NSCPClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  for (const PB::Commands::ExecuteRequestMessage::Request &payload : request.payload()) {
    if (payload.arguments_size() == 0 || payload.arguments(0) == "help") {
      PB::Commands::ExecuteResponseMessage::Response *rp = response.add_payload();
      nscapi::protobuf::functions::set_response_bad(*rp, "Usage: nscp nscp --help");
      return true;
    }
  }
  if (target_mode == NSCAPI::target_module) return client_.do_exec(request, response, "check_nscp");
  return false;
}

void NSCPClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                    PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//
