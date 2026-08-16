// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_nrpe.hpp"

#include <config.h>

#include <boost/filesystem.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_copy.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <nscp/boot_layout.hpp>
#include <nscp/path_defaults.hpp>

#include "../../modules/NRPEClient/nrpe_client.hpp"
#include "../../modules/NRPEClient/nrpe_handler.hpp"
#include "win/shellapi.hpp"

std::string gLog = "";

int main(int argc, char *argv[]) {
  PB::Commands::QueryResponseMessage response_message;
  std::vector<std::string> args;
  for (int i = 1; i < argc; i++) {
    args.push_back(argv[i]);
  }
  PB::Commands::QueryRequestMessage request_message;
  PB::Commands::QueryRequestMessage::Request *request = request_message.add_payload();
  request->set_command("check_nrpe");
  for (int i = 1; i < argc; i++) {
    request->add_arguments(argv[i]);
  }

  check_nrpe client;
  client.query(request_message, response_message);
  NSCAPI::nagiosReturn ret = NSCAPI::query_return_codes::returnOK;
  for (const ::PB::Commands::QueryResponseMessage_Response &response : response_message.payload()) {
    ret = nscapi::plugin_helper::maxState(ret, nscapi::protobuf::functions::gbp_to_nagios_status(response.result()));
    for (const ::PB::Commands::QueryResponseMessage_Response_Line &line : response.lines()) {
      std::cout << line.message();
      std::string tmp = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
      if (!tmp.empty()) std::cout << '|' << tmp;
    }
  }
  return ret;
}

#ifdef WIN32
boost::filesystem::path get_self_path() { return shellapi::get_module_file_name(); }
#else
boost::filesystem::path get_self_path() {
  char buff[1024];
  ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
  if (len != -1) {
    buff[len] = '\0';
    boost::filesystem::path p = std::string(buff);
    return p.parent_path();
  }
  return boost::filesystem::initial_path();
}
#endif
boost::filesystem::path getBasePath() { return get_self_path(); }

boost::filesystem::path getTempPath() {
  std::string tempPath;
#ifdef WIN32
  tempPath = shellapi::get_temp_path().string();
#else
  tempPath = "/tmp";
#endif
  return tempPath;
}

struct stdout_client_handler : public socket_helpers::client::client_handler {
  void log_debug(std::string, int, std::string msg) const {
    if (gLog == "debug") std::cout << msg << std::endl;
  }
  void log_error(std::string, int, std::string msg) const {
    if (gLog == "debug" || gLog == "error") std::cout << msg << std::endl;
  }

  // The on-disk layout, read from boot.ini next to the executable. A client
  // that disagrees with the service about ${shared-path} looks for
  // certificates in a folder the service never wrote to.
  // Read once, when the handler is constructed. A missing or silent boot.ini
  // means the legacy layout, which is what every installation that predates
  // the setting has.
  nscp::paths::layout layout_ = nscp::paths::layout_from_boot_ini_file((getBasePath() / "boot.ini").string());

  std::string getFolder(std::string key) {
    // Lookups only this binary can answer, about its own location.
    if (key == "base-path" || key == "exe-path") return getBasePath().string();
    if (key == "temp") return getTempPath().string();
#ifdef WIN32
    if (key == "common-appdata") return shellapi::get_special_folder_path(CSIDL_COMMON_APPDATA, getBasePath()).string();
#endif
    // Everything else comes from the table the service uses, so the two cannot
    // drift apart - including ${shared-path}, which the layout moves.
    const std::string shared = nscp::paths::default_for(key, layout_);
    if (!shared.empty()) return shared;
    return getBasePath().string();
  }

  std::string expand_path(std::string file) {
    return nscp::paths::expand_tokens(std::move(file), [this](const std::string &key) { return getFolder(key); });
  }
};

bool test(client::destination_container &source, client::destination_container &) {
  if (source.has_data("log"))
    gLog = source.get_string_data("log");
  else
    gLog = "error";
  return true;
}

boost::program_options::options_description add_client_options(client::destination_container &source, client::destination_container &) {
  namespace po = boost::program_options;

  po::options_description desc("Client options");
  desc.add_options()("log", po::value<std::string>()->notifier([&source](auto value) { source.set_string_data("log", value); }), "Set log level");
  return desc;
}

typedef nrpe_client::nrpe_client_handler<stdout_client_handler> nrpe_client_handler;
check_nrpe::check_nrpe() : client_("nrpe", std::make_shared<nrpe_client_handler>(), std::make_shared<nrpe_handler::options_reader_impl>()) {
  client_.client_desc = &add_client_options;
  client_.client_pre = &test;
}

void check_nrpe::query(const PB::Commands::QueryRequestMessage &request, PB::Commands::QueryResponseMessage &response) { client_.do_query(request, response); }
