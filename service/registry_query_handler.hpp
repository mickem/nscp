#pragma once

#include "plugin_manager.hpp"
#include "path_manager.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nsclient/logger/logger.hpp>

#include <boost/unordered_set.hpp>

namespace nsclient {

	namespace core {

		class registry_query_handler {

		private:
			nsclient::core::path_instance path_;
			nsclient::core::plugin_mgr_instance plugins_;
			nsclient::logging::logger_instance logger_;
			const Plugin::RegistryRequestMessage &request_;

		public:
			registry_query_handler(nsclient::core::path_instance path_, nsclient::core::plugin_mgr_instance plugins_, nsclient::logging::logger_instance logger_, const Plugin::RegistryRequestMessage &request);

			void parse(Plugin::RegistryResponseMessage &response);

			void parse_inventory(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage &response);
			void parse_registration(const Plugin::RegistryRequestMessage::Request::Registration &q, Plugin::RegistryResponseMessage &response);
			void parse_control(const Plugin::RegistryRequestMessage::Request::Control &q, Plugin::RegistryResponseMessage &response);


			void inventory_queries(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage::Response* rp);
			void inventory_modules(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage::Response* rp);
			void find_plugins_on_disk(boost::unordered_set<std::string> &unique_instances, const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage::Response* rp);

		private:
			nsclient::logging::logger_instance get_logger() const {
				return logger_;
			}
			void add_module(Plugin::RegistryResponseMessage::Response* rp, const plugin_cache_item &plugin, bool is_enabled);
			plugin_cache_item inventory_plugin_on_disk(nsclient::core::plugin_cache::plugin_cache_list_type &list, std::string plugin);

		};

	}
}
