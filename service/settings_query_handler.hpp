#pragma once

#include "nsclient_core_interface.hpp"

#include <timer.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nsclient/logger/logger.hpp>


namespace nsclient {
	
	namespace core {
		
		class settings_query_handler {
			
		private:
			nsclient::logging::logger_instance logger_;
			const Plugin::SettingsRequestMessage &request_;
			nsclient::core::core_interface *core_;
			timer t;

		public:
			settings_query_handler(nsclient::core::core_interface *core, const Plugin::SettingsRequestMessage &request);
			
			void parse(Plugin::SettingsResponseMessage &response);
			
			void parse_inventory(const Plugin::SettingsRequestMessage::Request::Inventory &q, Plugin::SettingsResponseMessage::Response* rp);
			void parse_query(const Plugin::SettingsRequestMessage::Request::Query &q, Plugin::SettingsResponseMessage::Response* rp);
			void parse_registration(const Plugin::SettingsRequestMessage::Request::Registration &q, int plugin_id, Plugin::SettingsResponseMessage::Response* rp);
			void parse_update(const Plugin::SettingsRequestMessage::Request::Update &q, Plugin::SettingsResponseMessage::Response* rp);
			void parse_control(const Plugin::SettingsRequestMessage::Request::Control &q, Plugin::SettingsResponseMessage::Response* rp);
			
		private:
			void recurse_find(Plugin::SettingsResponseMessage::Response::Query *rpp, const std::string base, bool recurse, bool fetch_keys);
			void settings_add_plugin_data(const std::set<unsigned int> &plugins, ::Plugin::Settings_Information* info);
			nsclient::logging::logger_instance get_logger() const {
				return logger_;
			}

			
		};
		
	}
}
