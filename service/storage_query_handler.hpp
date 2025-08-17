#pragma once

#include <nscapi/nscapi_protobuf_storage.hpp>
#include <nsclient/logger/logger.hpp>

#include "plugin_manager.hpp"
#include "storage_manager.hpp"

namespace nsclient {

namespace core {

class storage_query_handler {
 private:
  nsclient::core::storage_manager_instance storage_;
  nsclient::core::plugin_mgr_instance plugins_;
  nsclient::logging::logger_instance logger_;
  const PB::Storage::StorageRequestMessage &request_;

 public:
  storage_query_handler(nsclient::core::storage_manager_instance storage_, nsclient::core::plugin_mgr_instance plugins_,
                        nsclient::logging::logger_instance logger_, const PB::Storage::StorageRequestMessage &request);

  void parse(PB::Storage::StorageResponseMessage &response);

  void parse_get(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Get &q, PB::Storage::StorageResponseMessage &response);
  void parse_put(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Put &q, PB::Storage::StorageResponseMessage &response);

  // void find_plugins_on_disk(boost::unordered_set<std::string> &unique_instances, const PB::Storage::StorageRequestMessage::Request::Inventory &q,
  // PB::Storage::StorageResponseMessage::Response* rp);

 private:
  nsclient::logging::logger_instance get_logger() const { return logger_; }
  void add_module(PB::Storage::StorageResponseMessage::Response *rp, const plugin_cache_item &plugin);
  plugin_cache_item inventory_plugin_on_disk(nsclient::core::plugin_cache::plugin_cache_list_type &list, std::string plugin);
};

}  // namespace core
}  // namespace nsclient
