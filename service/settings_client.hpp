#pragma once
#include <settings/settings_core.hpp>
#include <nsclient/logger.hpp>
#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif

class NSClientT;
namespace nsclient {
	class settings_client {
		bool started_;
		NSClient* core_;
		bool default_;
		bool remove_default_;
		bool load_all_;
		bool use_samples_;

	public:
		settings_client(NSClient* core, bool update_defaults, bool remove_defaults, bool load_all, bool use_samples, std::string filter) 
			: started_(false), core_(core), default_(update_defaults), remove_default_(remove_defaults), load_all_(load_all), use_samples_(use_samples) 
		{
			startup();
		}


		~settings_client() {
			terminate();
		}

		void startup() {
			if (started_)
				return;
			if (!core_->boot_init(true)) {
				std::cout << "boot::init failed" << std::endl;
				return;
			}
			if (load_all_)
				core_->preboot_load_all_plugin_files();

			if (!core_->boot_load_all_plugins()) {
				std::cout << "boot::load_all_plugins failed!" << std::endl;
				return;
			}
			if (!core_->boot_start_plugins(false)) {
				std::cout << "boot::start_plugins failed!" << std::endl;
				return;
			}
			if (default_) {
				settings_manager::get_core()->update_defaults();
			}
			if (remove_default_) {
				std::cout << "Removing default values" << std::endl;
				settings_manager::get_core()->remove_defaults();
			}
			started_ = true;
		}

		void terminate() {
			if (!started_)
				return;
			core_->stop_unload_plugins_pre();
			core_->stop_exit_pre();
			core_->stop_exit_post();
			started_ = false;
		}

		int migrate_from(std::string src) {
			try {
				debug_msg("Migrating from: " + src);
				settings_manager::get_core()->migrate_from(src);
				return 1;
			} catch (settings::settings_exception e) {
				error_msg("Failed to initialize settings: " + e.reason());
			} catch (...) {
				error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
			}
			return -1;
		}
		int migrate_to(std::string target) {
			try {
				debug_msg("Migrating to: " + target);
				settings_manager::get_core()->migrate_to(target);
				return 1;
			} catch (settings::settings_exception e) {
				error_msg("Failed to initialize settings: " + e.reason());
			} catch (...) {
				error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
			}
			return -1;
		}

		void dump_path(std::string root) {
			BOOST_FOREACH(const std::string &path, settings_manager::get_core()->get()->get_sections(root)) {
				if (!root.empty())
					dump_path(root + "/" + path);
				else
					dump_path(path);
			}
			BOOST_FOREACH(std::string key, settings_manager::get_core()->get()->get_keys(root)) {
				std::cout << root << "." << key << "=" << settings_manager::get_core()->get()->get_string(root, key) << std::endl;
			}
		}

		int generate(std::string target) {
			try {
				if (target == "settings" || target.empty()) {
					settings_manager::get_core()->get()->save();
				} else if (target.empty()) {
					settings_manager::get_core()->get()->save();
				} else {
					settings_manager::get_core()->get()->save_to(target);
				}
				return 0;
			} catch (settings::settings_exception e) {
				error_msg("Failed to initialize settings: " + e.reason());
				return 1;
			} catch (NSPluginException &e) {
				error_msg("Failed to load plugins: " + utf8::utf8_from_native(e.what()));
				return 1;
			} catch (std::exception &e) {
				error_msg("Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
				return 1;
			} catch (...) {
				error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
				return 1;
			}
		}


		void switch_context(std::string contect) {
			settings_manager::get_core()->set_primary(contect);
		}

		int set(std::string path, std::string key, std::string val) {
			settings::settings_core::key_type type = settings_manager::get_core()->get()->get_key_type(path, key);
			if (type == settings::settings_core::key_string) {
				settings_manager::get_core()->get()->set_string(path, key, val);
			} else if (type == settings::settings_core::key_integer) {
				settings_manager::get_core()->get()->set_int(path, key, strEx::s::stox<int>(val));
			} else if (type == settings::settings_core::key_bool) {
				settings_manager::get_core()->get()->set_bool(path, key, settings::settings_interface::string_to_bool(val));
			} else {
				error_msg("Failed to set key (not found)");
				return -1;
			}
			settings_manager::get_core()->get()->save();
			return 0;
		}
		int show(std::string path, std::string key) {
			std::cout << settings_manager::get_core()->get()->get_string(path, key);
			return 0;
		}
		int list(std::string path) {
			try {
				dump_path(path);
			} catch (settings::settings_exception e) {
				error_msg("Settings error: " + e.reason());
			} catch (...) {
				error_msg("FATAL ERROR IN SETTINGS SUBSYTEM");
			}

			return 0;
		}
		int validate() {
			settings::error_list errors = settings_manager::get_core()->validate();
			BOOST_FOREACH(const std::string &e, errors) {
				std::cerr << e << std::endl;
			}
			return 0;
		}

		void error_msg(std::string msg) {
			nsclient::logging::logger::get_logger()->error("client", __FILE__, __LINE__, msg.c_str());
		}
		void debug_msg(std::string msg) {
			nsclient::logging::logger::get_logger()->debug("client", __FILE__, __LINE__, msg.c_str());
		}

		void list_settings_context_info(int padding, settings::instance_ptr instance) {
			std::string pad = std::string(padding, ' ');
			std::cout << pad << instance->get_info() << std::endl;
			BOOST_FOREACH(settings::instance_ptr child, instance->get_children()) {
				list_settings_context_info(padding+2, child);
			}
		}
		void list_settings_info() {
			std::cout << "Current settings instance loaded: " << std::endl;
			list_settings_context_info(2, settings_manager::get_settings());
		}
		void activate(const std::string &module) 
		{
			if (!core_->boot_load_plugin(module)) {
				std::cerr << "Failed to load module (Wont activate): " << module << std::endl;
			}
			core_->boot_start_plugins(false);
			settings_manager::get_core()->get()->set_string("/modules", module, "enabled");
			if (default_) {
				settings_manager::get_core()->update_defaults();
			}
			settings_manager::get_core()->get()->save();
		}
	};
}
