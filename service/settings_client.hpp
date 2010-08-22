#pragma once
#include <settings/settings_core.hpp>

class NSClientT;
namespace nsclient {
	class settings_client {
		NSClient* core_;
		std::wstring current_;
		bool default_;


	public:
		settings_client(NSClient* core) : core_(core) {}

		std::wstring get_source() {
			settings_manager::get_core()->get()->get_context();
		}

		void boot() {
			if (!current_.empty())
				core_->set_settings_context(current_);
			if (!core_->initCore(false)) {
				std::wcout << _T("Service *NOT* started!") << std::endl;
				return;
			}
			if (default_)
				settings_manager::get_core()->update_defaults();
		}

		void exit() {
			core_->exitCore(true);
		}

		void set_current(std::wstring current) { current_ = current; }
		void set_update_defaults(bool def) { default_ = def; }

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
				core_->load_all_plugins(NSCAPI::dontStart);
				settings_manager::get_core()->update_defaults();
				if (target == _T("settings") || target.empty()) {
					settings_manager::get_core()->get()->save();
				} else if (target == _T("trac")) {
					settings::string_list s = settings_manager::get_core()->get_reg_sections();
					BOOST_FOREACH(std::wstring path, s) {
						std::wcout << _T("== ") << path << _T(" ==") << std::endl;
						settings::string_list k = settings_manager::get_core()->get_reg_keys(path);
						bool first = true;
						BOOST_FOREACH(std::wstring key, k) {
							settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
							if (!desc.advanced) {
								if (first)
									std::wcout << _T("'''Normal settings'''") << std::endl;
								first = false;
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
				} else {
					settings_manager::get_core()->update_defaults();
					settings_manager::get_core()->get()->save_to(target);
				}
				return 1;
			} catch (settings::settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (std::exception &e) {
				error_msg(_T("Failed to initialize settings: ") + to_wstring(e.what()));
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
			return -1;
		}


		int set(std::wstring path, std::wstring key, std::wstring val) {
			core_->load_all_plugins(NSCAPI::dontStart);
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
			//core_->load_all_plugins(NSCAPI::dontStart);
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

		void error_msg(std::wstring msg) {
			core_->reportMessage(NSCAPI::error, __FILEW__, __LINE__, msg.c_str());
		}
		void debug_msg(std::wstring msg) {
			core_->reportMessage(NSCAPI::debug, __FILEW__, __LINE__, msg.c_str());
		}
	};
}