#pragma once

#include <nscapi/nscapi_protobuf_storage.hpp>
#include <nsclient/logger/logger.hpp>

#include "plugins/plugin_manager.hpp"
#include "storage_manager.hpp"

namespace nsclient {

namespace core {

class storage_query_handler {
  storage_manager_instance storage_;
  plugin_mgr_instance plugins_;
  logging::logger_instance logger_;
  const PB::Storage::StorageRequestMessage &request_;

 public:
  storage_query_handler(storage_manager_instance storage_, plugin_mgr_instance plugins_, logging::logger_instance logger_,
                        const PB::Storage::StorageRequestMessage &request);

  void parse(PB::Storage::StorageResponseMessage &response);

  void parse_get(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Get &q, PB::Storage::StorageResponseMessage &response);
  void parse_put(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Put &q, PB::Storage::StorageResponseMessage &response);

 private:
  logging::logger_instance get_logger() const { return logger_; }
  void add_module(PB::Storage::StorageResponseMessage::Response *rp, const plugin_cache_item &plugin);
  plugin_cache_item inventory_plugin_on_disk(plugin_cache::plugin_cache_list_type &list, std::string plugin);
};

}  // namespace core
}  // namespace nsclient
