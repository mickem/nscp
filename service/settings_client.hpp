#pragma once
#include <settings/settings_core.hpp>
#include <nsclient/logger.hpp>

class NSClientT;
namespace nsclient {
	class settings_client {
		bool started_;
		NSClient* core_;
		std::wstring log_;
		bool default_;
		bool remove_default_;
		bool load_all_;

	public:
		settings_client(NSClient* core, std::wstring log, bool update_defaults, bool remove_defaults, bool load_all) 
			: started_(false), core_(core), log_(log), default_(update_defaults), remove_default_(remove_defaults), load_all_(load_all) 
		{
			startup();
		}


		~settings_client() {
			terminate();
		}

		void startup() {
			if (started_)
				return;
			if (!core_->boot_init(log_)) {
				std::wcout << _T("boot::init failed") << std::endl;
				return;
			}
			if (load_all_)
				core_->preboot_load_all_plugin_files();

			if (!core_->boot_load_all_plugins()) {
				std::wcout << _T("boot::load_all_plugins failed!") << std::endl;
				return;
			}
			if (!core_->boot_start_plugins(false)) {
				std::wcout << _T("boot::start_plugins failed!") << std::endl;
				return;
			}
			if (default_) {
				std::wcout << _T("Adding default values") << std::endl;
				settings_manager::get_core()->update_defaults();
			}
			if (remove_default_) {
				std::wcout << _T("Removing default values") << std::endl;
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

		int migrate_from(std::wstring src) {
			try {
				debug_msg(_T("Migrating from: ") + src);
				settings_manager::get_core()->migrate_from(src);
				return 1;
			} catch (settings::settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
			return -1;
		}
		int migrate_to(std::wstring target) {
			try {
				debug_msg(_T("Migrating to: ") + target);
				settings_manager::get_core()->migrate_to(target);
				return 1;
			} catch (settings::settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
			return -1;
		}

		void dump_path(std::wstring root) {
			BOOST_FOREACH(std::wstring path, settings_manager::get_core()->get()->get_sections(root)) {
				if (!root.empty())
					dump_path(root + _T("/") + path);
				else
					dump_path(path);
			}
			BOOST_FOREACH(std::wstring key, settings_manager::get_core()->get()->get_keys(root)) {
				std::wcout << root << _T(".") << key << _T("=") << settings_manager::get_core()->get()->get_string(root, key) << std::endl;
			}
		}


		int generate(std::wstring target) {
			try {
				if (target == _T("settings") || target.empty()) {
					settings_manager::get_core()->get()->save();
				} else if (target == _T("trac")) {
					settings::string_list s = settings_manager::get_core()->get_reg_sections();
					BOOST_FOREACH(std::wstring path, s) {
						std::wcout << _T("== ") << path << _T(" ==") << std::endl;
						settings::settings_core::path_description desc = settings_manager::get_core()->get_registred_path(path);
						if (!desc.description.empty())
							std::wcout << desc.description << std::endl;
						std::wcout << std::endl;
						settings::string_list k = settings_manager::get_core()->get_reg_keys(path);
						bool first = true;
						BOOST_FOREACH(std::wstring key, k) {
							settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
							if (!desc.advanced) {
								if (first)
									std::wcout << _T("'''Normal settings'''") << std::endl;
								first = false;
								strEx::replace(desc.description, _T("\n"), _T("\n|| || ||"));
								if (desc.defValue.empty())
									desc.defValue = _T(" ");
								std::wcout << _T("||") << key << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description << std::endl;
							}
						}
						first = true;
						BOOST_FOREACH(std::wstring key, k) {
							settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
							if (desc.advanced) {
								if (first)
									std::wcout << _T("'''Advanced settings'''") << std::endl;
								first = false;
								std::wcout << _T("||") << key << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description << std::endl;
							}
						}
					}
				} else if (target.empty()) {
					settings_manager::get_core()->get()->save();
				} else {
					settings_manager::get_core()->get()->save_to(target);
				}
				return 1;
			} catch (settings::settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (NSPluginException &e) {
				error_msg(_T("Failed to load plugins: ") + to_wstring(e.what()));
			} catch (std::exception &e) {
				error_msg(_T("Failed to initialize settings: ") + to_wstring(e.what()));
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
			return -1;
		}


		void switch_context(std::wstring contect) {
			settings_manager::get_core()->set_primary(contect);
		}

		int set(std::wstring path, std::wstring key, std::wstring val) {
			settings::settings_core::key_type type = settings_manager::get_core()->get()->get_key_type(path, key);
			if (type == settings::settings_core::key_string) {
				settings_manager::get_core()->get()->set_string(path, key, val);
			} else if (type == settings::settings_core::key_integer) {
				settings_manager::get_core()->get()->set_int(path, key, strEx::stoi(val));
			} else if (type == settings::settings_core::key_bool) {
				settings_manager::get_core()->get()->set_bool(path, key, settings::settings_interface::string_to_bool(val));
			} else {
				error_msg(_T("Failed to set key (not found)"));
				return -1;
			}
			settings_manager::get_core()->get()->save();
			return 0;
		}
		int show(std::wstring path, std::wstring key) {
			std::wcout << settings_manager::get_core()->get()->get_string(path, key);
			return 0;
		}
		int list(std::wstring path) {
			try {
				dump_path(path);
			} catch (settings::settings_exception e) {
				error_msg(_T("Settings error: ") + e.getError());
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}

			return 0;
		}
		int validate() {
			settings::error_list errors = settings_manager::get_core()->validate();
			BOOST_FOREACH(const std::wstring &e, errors) {
				std::wcerr << e << std::endl;
			}
			return 0;
		}

		void error_msg(std::wstring msg) {
			nsclient::logging::logger::get_logger()->error(__FILE__, __LINE__, msg.c_str());
		}
		void debug_msg(std::wstring msg) {
			nsclient::logging::logger::get_logger()->debug(__FILE__, __LINE__, msg.c_str());
		}

		void list_settings_context_info(int padding, settings::instance_ptr instance) {
			std::wstring pad = std::wstring(padding, L' ');
			std::wcout << pad << instance->get_info() << std::endl;
			BOOST_FOREACH(settings::instance_ptr child, instance->get_children()) {
				list_settings_context_info(padding+2, child);
			}
		}
		void list_settings_info() {
			std::wcout << _T("Current settings instance loaded: ") << std::endl;
			list_settings_context_info(2, settings_manager::get_settings());
		}
		void activate(const std::wstring &module) 
		{
			if (!core_->boot_load_plugin(module)) {
				std::wcerr << _T("Failed to load module (Wont activate): ") << module << std::endl;
			}
			core_->boot_start_plugins(false);
			settings_manager::get_core()->get()->set_string(_T("/modules"), module, _T("enabled"));
			if (default_) {
				settings_manager::get_core()->update_defaults();
			}
			settings_manager::get_core()->get()->save();
		}
	};
}