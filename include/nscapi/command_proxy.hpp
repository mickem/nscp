#pragma once


#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
	class command_proxy {
	private:
		unsigned int plugin_id_;
		nscapi::core_wrapper* core_;

	public:
		command_proxy(unsigned int plugin_id, nscapi::core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}
		virtual void registry_query(const std::string &request, std::string &response) {
			if (core_->registry_query(request, response) != NSCAPI::isSuccess) {
				throw "TODO: FIXME: DAMN!!!";
			}
		}

		unsigned int get_plugin_id() const { return plugin_id_; }

		virtual void err(const char* file, int line, std::string message) {
			core_->log(NSCAPI::log_level::error, file, line, message);
		}
		virtual void warn(const char* file, int line, std::string message) {
			core_->log(NSCAPI::log_level::warning, file, line, message);
		}
		virtual void info(const char* file, int line, std::string message)  {
			core_->log(NSCAPI::log_level::info, file, line, message);
		}
		virtual void debug(const char* file, int line, std::string message)  {
			core_->log(NSCAPI::log_level::debug, file, line, message);
		}
	};
}