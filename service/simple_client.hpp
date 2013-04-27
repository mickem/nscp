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
		inline void error(unsigned int line, const std::string &message) {
			get_logger()->error("client", __FILE__, line, message);
		}
		inline void info(unsigned int line, const std::string &message) {
			get_logger()->info("client", __FILE__, line, message);
		}
		void start(std::string log) {
			if (!core_->boot_init(log)) {
				error(__LINE__, "Service failed to init");
				return;
			}
			if (!core_->boot_load_all_plugins()) {
				error(__LINE__, "Service failed to load plugins");
				return;
			}
			if (!core_->boot_start_plugins(true)) {
				error(__LINE__, "Service failed to start plugins");
				return;
			}

			if (core_->get_service_control().is_started())
				info(__LINE__, "Service seems to be started (Sockets and such will probably not work)...");

			//std::wcout << _T("Using settings from: ") << settings_manager::get_core()->get_settings_type_desc() << std::endl;
			info(__LINE__, "Enter command to inject or exit to terminate...");
			std::wstring s = _T("");

			while (true) {
				std::string s;
				std::getline(std::cin, s);
				if (s == "exit") {
					info(__LINE__, "Exiting...");
					break;
				} else if (s == "plugins") {
					info(__LINE__, "Plugins: ");
					core_->listPlugins();
				} else if (s.size() > 4 && s.substr(0,4) == "load") {
					core_->boot_load_plugin(s.substr(5));
				} else if (s == "list" || s == "commands") {
					std::list<std::string> lst = core_->list_commands();
					if (lst.size() == 0)
						info(__LINE__, "NO commands found");
					else {
						info(__LINE__, "Commands:");
						BOOST_FOREACH(const std::string s, lst)
							info(__LINE__, "| " + s + ": " + core_->describeCommand(s));
					}
				} else if (s.size() > 4 && s.substr(0,3) == "log") {
					info(__LINE__, "Setting log to: " + s.substr(4));
					nsclient::logging::logger::set_log_level(s.substr(4));
				} else if (s == "assert") {
					int *foo = 0;
					*foo = 0;
					throw "test";
				} else {
					try {
						strEx::s::token t = strEx::s::getToken(s, ' ');
						std::string msg, perf;
						NSCAPI::nagiosReturn ret = core_->inject(t.first, t.second, msg, perf);
						if (ret == NSCAPI::returnIgnored) {
							info(__LINE__, "No handler for command: " + t.first);
						} else {
							if (msg.size() > 4096) {
								info(__LINE__, "Command returned too much data (result truncated)");
								msg = msg.substr(0, 4096);
							}
							info(__LINE__, nscapi::plugin_helper::translateReturn(ret) + ":" + msg);
							if (!perf.empty())
								info(__LINE__, " Performance data: " + perf);
						}
					} catch (const std::exception &e) {
						error(__LINE__, "Exception: " + utf8::utf8_from_native(e.what()));
					} catch (...) {
						error(__LINE__, "Unknown exception");
					}
				}
			}
			core_->stop_unload_plugins_pre();
			core_->stop_exit_pre();
			core_->stop_exit_post();
		}
	};
}