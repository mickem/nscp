#pragma once

#include <boost/unordered_set.hpp>
#include <nscapi/nscapi_protobuf_registry.hpp>
#include <nsclient/logger/logger.hpp>

#include "path_manager.hpp"
#include "plugin_manager.hpp"

namespace nsclient {

namespace core {

class registry_query_handler {
 private:
  nsclient::core::path_instance path_;
  nsclient::core::plugin_mgr_instance plugins_;
  nsclient::logging::logger_instance logger_;
  const PB::Registry::RegistryRequestMessage &request_;

 public:
  registry_query_handler(nsclient::core::path_instance path_, nsclient::core::plugin_mgr_instance plugins_, nsclient::logging::logger_instance logger_,
                         const PB::Registry::RegistryRequestMessage &request);

  void parse(PB::Registry::RegistryResponseMessage &response);

  void parse_inventory(const PB::Registry::RegistryRequestMessage::Request::Inventory &q, PB::Registry::RegistryResponseMessage &response);
  void parse_registration(const PB::Registry::RegistryRequestMessage::Request::Registration &q, PB::Registry::RegistryResponseMessage &response);
  void parse_control(const PB::Registry::RegistryRequestMessage::Request::Control &q, PB::Registry::RegistryResponseMessage &response);

  void inventory_queries(const PB::Registry::RegistryRequestMessage::Request::Inventory &q, PB::Registry::RegistryResponseMessage::Response *rp);
  void inventory_modules(const PB::Registry::RegistryRequestMessage::Request::Inventory &q, PB::Registry::RegistryResponseMessage::Response *rp);
  void find_plugins_on_disk(boost::unordered_set<std::string> &unique_instances, const PB::Registry::RegistryRequestMessage::Request::Inventory &q,
                            PB::Registry::RegistryResponseMessage::Response *rp);

 private:
  nsclient::logging::logger_instance get_logger() const { return logger_; }
  void add_module(PB::Registry::RegistryResponseMessage::Response *rp, const plugin_cache_item &plugin, bool is_enabled);
  plugin_cache_item inventory_plugin_on_disk(nsclient::core::plugin_cache::plugin_cache_list_type &list, std::string plugin);
};

}  // namespace core
}  // namespace nsclient
