#pragma once

#include <string>

#include <boost/shared_ptr.hpp>

#include <NSCAPI.h>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/command_proxy.hpp>

namespace nscapi {
	namespace impl {
		struct simple_plugin {
			int id_;
			nscapi::core_wrapper* get_core() const;
			inline unsigned int get_id() const {
				return id_;
			}
			inline void set_id(const unsigned int id) {
				id_ = id;
			}
			inline boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() {
				return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(id_, get_core()));
			}
			inline boost::shared_ptr<nscapi::command_proxy> get_command_proxy() {
				return boost::shared_ptr<nscapi::command_proxy>(new nscapi::command_proxy(id_, get_core()));
			}
// 			void register_command(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
// 			void settings_register_key(std::string path, std::string key, NSCAPI::settings_type type, std::string title, std::string description, std::string defaultValue, bool advanced);
// 			void settings_register_path(std::string path, std::string title, std::string description, bool advanced);

			std::string get_base_path() const;
		};
	}
}
