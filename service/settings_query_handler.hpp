#pragma once

#include <nscapi/nscapi_protobuf_settings.hpp>
#include <nsclient/logger/logger.hpp>
#include <timer.hpp>

#include "nsclient_core_interface.hpp"

namespace nsclient {

namespace core {

class settings_query_handler {
  logging::logger_instance logger_;
  const PB::Settings::SettingsRequestMessage &request_;
  std::shared_ptr<core_interface> core_;
  timer t;

 public:
  settings_query_handler(std::shared_ptr<core_interface> core, const PB::Settings::SettingsRequestMessage &request);

  void parse(PB::Settings::SettingsResponseMessage &response);

  void parse_inventory(const PB::Settings::SettingsRequestMessage::Request::Inventory &q, PB::Settings::SettingsResponseMessage::Response *rp);
  void parse_query(const PB::Settings::SettingsRequestMessage::Request::Query &q, PB::Settings::SettingsResponseMessage::Response *rp);
  void parse_registration(const PB::Settings::SettingsRequestMessage::Request::Registration &q, int plugin_id,
                          PB::Settings::SettingsResponseMessage::Response *rp);
  void parse_update(const PB::Settings::SettingsRequestMessage::Request::Update &q, PB::Settings::SettingsResponseMessage::Response *rp);
  void parse_control(const PB::Settings::SettingsRequestMessage::Request::Control &q, PB::Settings::SettingsResponseMessage::Response *rp);

 private:
  void recurse_find(PB::Settings::SettingsResponseMessage::Response::Query *rpp, const std::string base, bool recurse, bool fetch_keys);
  void settings_add_plugin_data(const std::set<unsigned int> &plugins, PB::Settings::Information *info);
  logging::logger_instance get_logger() const { return logger_; }
};

}  // namespace core
}  // namespace nsclient
