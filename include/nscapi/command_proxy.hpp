#pragma once

#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
	class command_proxy {
	private:
		unsigned int plugin_id_;
		nscapi::core_wrapper* core_;

	public:
		command_proxy(unsigned int plugin_id, nscapi::core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}

		virtual void register_command(std::wstring command, std::wstring description) {
			core_->registerCommand(plugin_id_, command, description);
		}

		virtual void err(const char* file, int line, std::wstring message) {
			core_->log(NSCAPI::log_level::error, file, line, message);
		}
		virtual void warn(const char* file, int line, std::wstring message) {
			core_->log(NSCAPI::log_level::warning, file, line, message);
		}
		virtual void info(const char* file, int line, std::wstring message)  {
			core_->log(NSCAPI::log_level::info, file, line, message);
		}
		virtual void debug(const char* file, int line, std::wstring message)  {
			core_->log(NSCAPI::log_level::debug, file, line, message);
		}
	};
}