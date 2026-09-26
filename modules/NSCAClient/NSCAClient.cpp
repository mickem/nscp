// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NSCAClient.h"

#include <memory>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscpcrypt/nscpcrypt.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <utility>

#include "nsca_client.hpp"
#include "nsca_handler.hpp"

/**
 * Default c-tor
 * @return
 */
NSCAClient::NSCAClient()
    : simple_plugin(), client_("nsca", std::make_shared<nsca_client::nsca_client_handler>(), std::make_shared<nsca_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
NSCAClient::~NSCAClient() = default;

bool NSCAClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("NSCA", std::move(alias), "client");
    client_.set_path(settings.alias().get_settings_path("targets"));

    // clang-format off
    settings.alias().add_path_to_settings()
      ("NSCA CLIENT SECTION", "Section for NSCA passive check module.")

      ("handlers", sh::fun_values_path([this] (auto key, auto value) { this->add_command(key, value); }),
	      "CLIENT HANDLER SECTION", "",
	      "CLIENT HANDLER", "For more configuration options add a dedicated section")

      ("targets", sh::fun_values_path([this] (auto key, auto value) { this->add_target(key, value); }),
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
                    "${host_lc}\tHostname in lowercase\n"
                    "${host_uc}\tHostname in uppercase\n"
                    "${domain}\tDomainname\n"
                    "${domain_lc}\tDomainname in lowercase\n"
                    "${domain_uc}\tDomainname in uppercase\n"
                    "${address_ipv4}\tIPv4 address of the computer\n"
                    "${address_ipv6}\tIPv6 address of the computer (lowercase, compressed)\n"
                    "${address_ipv6_lc}\tIPv6 address in lowercase (compressed)\n"
                    "${address_ipv6_uc}\tIPv6 address in uppercase (compressed)\n"
                    "${address_ipv6_lc_comp}\tIPv6 address in lowercase, compressed (2001:db8::7)\n"
                    "${address_ipv6_lc_uncomp}\tIPv6 address in lowercase, uncompressed (2001:0db8:0000:0000:0000:0000:0000:0007)\n"
                    "${address_ipv6_uc_comp}\tIPv6 address in uppercase, compressed\n"
                    "${address_ipv6_uc_uncomp}\tIPv6 address in uppercase, uncompressed\n")

        .add_string("encoding", sh::string_key(&encoding_, ""), "NSCA DATA ENCODING", "", true)

        .add_string("channel", sh::string_key(&channel_, "NSCA"), "CHANNEL", "The channel to listen to.");

    settings.register_all();
    settings.notify();

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel_);

    hostname_ = socket_helpers::expand_hostname(hostname_);
    client_.set_sender(hostname_);
  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
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

void NSCAClient::add_target(const std::string &key, const std::string &arg) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

void NSCAClient::add_command(const std::string &key, const std::string &arg) {
  try {
    nscapi::core_helper core(get_core(), get_id());
    std::string k = client_.add_command(key, arg);
    if (!k.empty()) core.register_command(k.c_str(), "NSCA relay for: " + key);
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
bool NSCAClient::unloadModule() {
  client_.clear();
  return true;
}

void NSCAClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

bool NSCAClient::commandLineExec(int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  for (const PB::Commands::ExecuteRequestMessage::Request &payload : request.payload()) {
    if (payload.arguments_size() > 0 && payload.arguments(0) == "install") {
      PB::Commands::ExecuteResponseMessage::Response *rp = response.add_payload();
      return cli_install(payload, rp);
    }
  }
  if (target_mode == NSCAPI::target_module) {
    return client_.do_exec(request, response, "submit_");
  }
  return false;
}

bool NSCAClient::cli_install(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) const {
  namespace pf = nscapi::protobuf::functions;
  po::options_description desc;

  const std::string target_path = "/settings/NSCA/client/targets/default";
  const std::string client_path = "/settings/NSCA/client";
  const std::string server_path = "/settings/NSCA/server";

  std::string host, port, password, encryption, hostname;

  // clang-format off
  desc.add_options()("help", "Show help.")
      ("host", po::value<std::string>(&host), "Address of the NSCA server (the machine running the nsca daemon) to submit results to.")
      ("port", po::value<std::string>(&port), "Port the nsca daemon listens on (5667 unless it was changed).")
      ("password", po::value<std::string>(&password),
       "The shared key. NSCA encrypts with it rather than checking it, so it has to be the same string as `password` in the daemon's nsca.cfg, "
       "and it is stored in clear text because a hash is not a key. It is not the /settings/default password the web UI and check_nt verify "
       "callers against.")
      ("encryption", po::value<std::string>(&encryption),
       std::string("Cipher, which has to match `decryption_method` in the daemon's nsca.cfg. Available:\n") + nscp::encryption::helpers::get_crypto_string("\n"))
      ("hostname", po::value<std::string>(&hostname),
       "The host name to submit results as. It has to match the host as Nagios/Icinga knows it, not necessarily this machine's name; `auto` uses "
       "the computer name.")
      ;
  // clang-format on

  try {
    po::variables_map vm;
    nscapi::program_options::basic_command_line_parser cmd(request);
    cmd.options(desc);
    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (vm.count("help")) {
      pf::set_response_good(*response, nscapi::program_options::help(desc));
      return true;
    }

    // What is on disk already, so an option left out keeps its value. Also
    // whether NSCAServer is running, because it falls back to this target's key.
    std::string current_address, current_port, current_password, current_encryption, current_hostname;
    std::string nsca_server_module, nsca_server_password;
    pf::settings_query q(get_id());
    q.get(target_path, "address", "");
    q.get(target_path, "port", "");
    q.get(target_path, "password", "");
    q.get(target_path, "encryption", "");
    q.get(client_path, "hostname", "");
    q.get("/modules", "NSCAServer", "");
    q.get(server_path, "password", "");
    get_core()->settings_query(q.request(), q.response());
    if (!q.validate_response()) {
      pf::set_response_bad(*response, q.get_response_error());
      return true;
    }
    for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
      if (val.matches(target_path, "address"))
        current_address = val.get_string();
      else if (val.matches(target_path, "port"))
        current_port = val.get_string();
      else if (val.matches(target_path, "password"))
        current_password = val.get_string();
      else if (val.matches(target_path, "encryption"))
        current_encryption = val.get_string();
      else if (val.matches(client_path, "hostname"))
        current_hostname = val.get_string();
      else if (val.matches("/modules", "NSCAServer"))
        nsca_server_module = val.get_string();
      else if (val.matches(server_path, "password"))
        nsca_server_password = val.get_string();
    }

    if (host.empty()) host = current_address;
    if (port.empty()) port = current_port;
    if (password.empty()) password = current_password;
    if (encryption.empty()) encryption = current_encryption;
    if (hostname.empty()) hostname = current_hostname;

    // A submission with nowhere to go is the one thing worth refusing over: the
    // module would load, register its channel and drop every result.
    if (host.empty()) {
      pf::set_response_bad(*response,
                           "No NSCA server to submit to. Pass --host <address of the machine running the nsca daemon>; --port, --password, "
                           "--encryption and --hostname are optional and keep their current values when left out.");
      return true;
    }
    // Only default the cipher on a fresh target, so a re-run never silently
    // changes a cipher the daemon is configured for.
    if (encryption.empty()) encryption = "aes256";

    std::stringstream result;
    pf::settings_query s(get_id());
    s.set("/modules", "NSCAClient", "enabled");
    s.set(target_path, "address", host);
    if (!port.empty()) s.set(target_path, "port", port);
    s.set(target_path, "encryption", encryption);
    s.set(target_path, "password", password);
    if (!hostname.empty()) s.set(client_path, "hostname", hostname);
    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      pf::set_response_bad(*response, s.get_response_error());
      return true;
    }

    result << "Submitting NSCA results to " << host << (port.empty() ? "" : ":" + port) << " with encryption " << encryption << "." << std::endl;
    if (!hostname.empty()) {
      result << "Submitting as host name " << hostname << "." << std::endl;
    }
    if (password.empty()) {
      result << "WARNING: no password set. NSCA derives its key from the password, so an empty one is a well-known key: anyone who can reach the" << std::endl;
      result << "         daemon can forge submissions. Pass --password <the key from the daemon's nsca.cfg>." << std::endl;
    } else {
      result << "The key must match `password` in the daemon's nsca.cfg, and the cipher its `decryption_method`." << std::endl;
    }
    // The server side reads this target when it has no key of its own, so say
    // what that means rather than leaving the operator to infer it.
    const bool server_enabled =
        !nsca_server_module.empty() && nsca_server_module != "disabled" && nsca_server_module != "0" && nsca_server_module != "false";
    if (server_enabled && nsca_server_password.empty()) {
      result << "NSCAServer is enabled with no key of its own, so it will accept submissions with this same key." << std::endl;
      result << "Give it a different one under [" << server_path << "] if that is not what you want." << std::endl;
    }
    result << "Restart nsclient++ for the change to take effect." << std::endl;
    pf::set_response_good(*response, result.str());
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
    return true;
  }
}

void NSCAClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                    PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}
