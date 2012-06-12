#pragma once

#include <string>

#include <boost/shared_ptr.hpp>

#include <NSCAPI.h>

#include <nscapi/settings_proxy.hpp>

namespace nscapi {
	namespace impl {
		struct simple_plugin {
			int id_;
			nscapi::core_wrapper* get_core();
			inline unsigned int get_id() {
				return id_;
			}
			inline void set_id(unsigned int id) {
				id_ = id;
			}
			inline boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() {
				return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(get_core()));
			}
			void register_command(std::wstring command, std::wstring description) {
				get_core()->registerCommand(get_id(), command, description);
			}

		};

		class simple_submission_handler {
		public:
			NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
			virtual NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) = 0;

		};

		class simple_command_handler {
		public:
			NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
			virtual NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf) = 0;
		};
		class simple_command_line_exec {
		public:
			NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
			virtual NSCAPI::nagiosReturn commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) = 0;
		};


		class simple_log_handler {
		public:
			void handleMessageRAW(std::string data);
			virtual void handleMessage(int msgType, const std::string file, int line, std::string message) = 0;
		};
	}
}