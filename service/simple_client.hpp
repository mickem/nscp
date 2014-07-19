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
		void start() {
			if (!core_->boot_init(true)) {
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

			info(__LINE__, "Enter command to inject or exit to terminate...");

			while (true) {
				std::string s;
				std::getline(std::cin, s);
				if (s == "exit") {
					info(__LINE__, "Exiting...");
					break;
				}
				handle_command(s);

			}
			core_->stop_unload_plugins_pre();
			core_->stop_exit_pre();
			core_->stop_exit_post();
		}

		void output_message(const std::string &msg) {
			get_logger()->info("client", __FILE__, __LINE__, msg);
		}
		void handle_command(const std::string &command) {
			if (command == "plugins") {
				output_message("Plugins: ");
				core_->listPlugins();
			} else if (command.size() > 4 && command.substr(0,4) == "load") {
				core_->boot_load_plugin(command.substr(5), true);
			} else if (command == "list" || command == "commands") {
				std::list<std::string> lst = core_->list_commands();
				if (lst.size() == 0)
					output_message("NO commands found");
				else {
					output_message("Commands:");
					BOOST_FOREACH(const std::string s, lst)
						output_message("| " + s + ": " + core_->describeCommand(s));
				}
			} else if (command.size() > 4 && command.substr(0,3) == "log") {
				output_message("Setting log to: " + command.substr(4));
				nsclient::logging::logger::set_log_level(command.substr(4));
			} else if (command == "assert") {
				int *foo = 0;
				*foo = 0;
				throw "test";
			} else if (!command.empty()) {
				try {
					strEx::s::token t = strEx::s::getToken(command, ' ');
					std::string msg, perf;
					NSCAPI::nagiosReturn ret = core_->inject(t.first, t.second, msg, perf);
					if (ret == NSCAPI::returnIgnored) {
						output_message("No handler for command: " + t.first);
					} else {
						output_message(nscapi::plugin_helper::translateReturn(ret) + ": " + msg);
						if (!perf.empty())
							output_message(" Performance data: " + perf);
					}
				} catch (const std::exception &e) {
					output_message("Exception: " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					output_message("Unknown exception");
				}
			}
		}
	};
}