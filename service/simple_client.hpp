#pragma once
#include <nscapi/nscapi_helper.hpp>
#include <nsclient/logger.hpp>
class NSClientT;
namespace nsclient {
	class simple_client {
		NSClient *core_;
	public:
		simple_client(NSClient *core) : core_(core) {}

		inline nsclient::logging::logger_interface* get_logger() const {
			return nsclient::logging::logger::get_logger(); 
		}
		inline void error(unsigned int line, const std::wstring &message) {
			get_logger()->error(_T("client"), __FILE__, line, message);
		}
		inline void info(unsigned int line, const std::wstring &message) {
			get_logger()->info(_T("client"), __FILE__, line, message);
		}
		void start(std::wstring log) {
			if (!core_->boot_init(log)) {
				error(__LINE__, _T("Service failed to init"));
				return;
			}
			if (!core_->boot_load_all_plugins()) {
				error(__LINE__, _T("Service failed to load plugins"));
				return;
			}
			if (!core_->boot_start_plugins(true)) {
				error(__LINE__, _T("Service failed to start plugins"));
				return;
			}

			if (core_->get_service_control().is_started())
				info(__LINE__, _T("Service seems to be started (Sockets and such will probably not work)..."));

			//std::wcout << _T("Using settings from: ") << settings_manager::get_core()->get_settings_type_desc() << std::endl;
			info(__LINE__, _T("Enter command to inject or exit to terminate..."));
			std::wstring s = _T("");

			while (true) {
				std::wstring s;
				std::getline(std::wcin, s);
				if (s == _T("exit")) {
					info(__LINE__, _T("Exiting..."));
					break;
				} else if (s == _T("plugins")) {
					info(__LINE__, _T("Plugins: "));
					core_->listPlugins();
				} else if (s == _T("list") || s == _T("commands")) {
					info(__LINE__, _T("Commands:"));
					std::list<std::wstring> lst = core_->list_commands();
					for (std::list<std::wstring>::const_iterator cit = lst.begin(); cit!=lst.end();++cit)
						info(__LINE__, _T("| ") + *cit + _T(": ") + core_->describeCommand(*cit));
				} else if (s.size() > 4 && s.substr(0,3) == _T("log")) {
					info(__LINE__, _T("Setting log to: ") + s.substr(4));
					nsclient::logging::logger::set_log_level(s.substr(4));
				} else if (s == _T("reattach")) {
					info(__LINE__, _T("Reattaching to session 0"));
					core_->startTrayIcon(0);
				} else if (s == _T("assert")) {
					int *foo = 0;
					*foo = 0;
					throw "test";
				} else {
					try {
						strEx::token t = strEx::getToken(s, L' ');
						std::wstring msg, perf;
						NSCAPI::nagiosReturn ret = core_->inject(t.first, t.second, msg, perf);
						if (ret == NSCAPI::returnIgnored) {
							info(__LINE__, _T("No handler for command: ") + t.first);
						} else {
							if (msg.size() > 4096) {
								info(__LINE__, _T("Command returned: ") + strEx::itos(msg.size()) + _T(" bytes of data will only display first 4k."));
								msg = msg.substr(0, 4096);
							}
							info(__LINE__, nscapi::plugin_helper::translateReturn(ret) + _T(":") + msg);
							if (!perf.empty())
								info(__LINE__, _T(" Performance data: ") + perf);
						}
					} catch (const nscapi::nscapi_exception &e) {
						error(__LINE__, _T("NSCAPI Exception: ") + utf8::cvt<std::wstring>(e.what()));
					} catch (const std::exception &e) {
						error(__LINE__, _T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
					} catch (...) {
						error(__LINE__, _T("Unknown exception"));
					}
				}
			}
			core_->stop_unload_plugins_pre();
			core_->stop_exit_pre();
			core_->stop_exit_post();
		}
	};
}