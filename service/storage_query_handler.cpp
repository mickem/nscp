#include "storage_query_handler.hpp"

#include "dll_plugin.h"

#include <nscapi/nscapi_protobuf_functions.hpp>

namespace nsclient {

	namespace core {


		storage_query_handler::storage_query_handler(nsclient::core::storage_manager_instance storage_, nsclient::core::plugin_mgr_instance plugins_, nsclient::logging::logger_instance logger_, const PB::Storage::StorageRequestMessage &request)
			: storage_(storage_)
			, plugins_(plugins_)
			, logger_(logger_)
			, request_(request)
		{}



		void storage_query_handler::parse(PB::Storage::StorageResponseMessage &response) {

			for(const PB::Storage::StorageRequestMessage::Request &r: request_.payload()) {
				if (r.has_get()) {
					parse_get(r.id(), r.get(), response);
				} else if (r.has_put()) {
					parse_put(r.id(), r.put(), response);
				} else {
					LOG_ERROR_CORE("Storage query: Unsupported action");
				}
			}
		}

		void storage_query_handler::parse_get(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Get &q, PB::Storage::StorageResponseMessage &response) {
			PB::Storage::StorageResponseMessage::Response *payload = response.add_payload();
			std::string plugin_name = "";
			nsclient::core::plugin_manager::plugin_type plugin = plugins_->find_plugin(plugin_id);
			if (plugin) {
				plugin_name = plugin->get_alias_or_name();
			}
			for(const ::PB::Storage::Storage_Entry &e: storage_->get(plugin_name, q.context())) {
				payload->mutable_get()->add_entry()->CopyFrom(e);
			}
		}
		void storage_query_handler::parse_put(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Put &q, PB::Storage::StorageResponseMessage &response) {
			std::string plugin_name = "";
			nsclient::core::plugin_manager::plugin_type plugin = plugins_->find_plugin(plugin_id);
			if (plugin) {
				plugin_name = plugin->get_alias_or_name();
			}
			storage_->put(plugin_name, q.entry());

		}

		plugin_cache_item storage_query_handler::inventory_plugin_on_disk(nsclient::core::plugin_cache::plugin_cache_list_type &list, std::string plugin) {
			plugin_cache_item itm;
			try {
				bool loaded = false;
				plugin_type instance = plugins_->only_load_module(plugin, "", loaded);
				if (!loaded) {
					return itm;
				}
				itm.dll = instance->getModule();
				itm.alias = "";
				itm.desc = instance->getDescription();
				itm.id = instance->get_id();
				itm.is_loaded = false;
				list.push_back(itm);
			} catch (const std::exception &e) {
				LOG_ERROR_CORE("Failed to load " + plugin + ": " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				LOG_ERROR_CORE("Failed to load " + plugin + ": UNKNOWN EXCEPTION");
			}
			return itm;
		}

	}
}
