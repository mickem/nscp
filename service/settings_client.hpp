#pragma once


class NSClientT;
namespace nsclient {
	class settings_client {
		NSClient* core_;
	public:
		settings_client(NSClient* core) : core_(core) {}

		std::wstring get_source() {
			settings_manager::get_core()->get()->get_context();
		}

		void boot() {
			if (!core_->initCore(true)) {
				std::wcout << _T("Service *NOT* started!") << std::endl;
				return;
			}
		}

		void migrate_from(std::wstring src, std::wstring target, bool def) {
			try {
				if (def)
					settings_manager::get_core()->update_defaults();
				if (!src.empty() && !target.empty()) {
					debug_msg(_T("Migrating ") + src + _T(" to ") + target);
					settings_manager::get_core()->migrate(src, target);
				} else {
					debug_msg(_T("Migrating ") + src + _T(" to ") + target);
					settings_manager::get_core()->migrate_from(src);
				}
				core_->exitCore(true);
			} catch (settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
		}
		void migrate_to(std::wstring src, std::wstring target, bool def) {
			try {
				if (def)
					settings_manager::get_core()->update_defaults();
				if (!src.empty() && !target.empty()) {
					debug_msg(_T("Migrating ") + src + _T(" to ") + target);
					settings_manager::get_core()->migrate(src, target);
				} else {
					debug_msg(_T("Migrating ") + src + _T(" to ") + target);
					settings_manager::get_core()->migrate_to(target);
				}
				core_->exitCore(true);
			} catch (settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
		}

		void generate(std::wstring target) {
			try {
				core_->load_all_plugins(NSCAPI::dontStart);
				if (target == _T("default") || target.empty()) {
					settings_manager::get_core()->update_defaults();
					settings_manager::get_core()->get()->save();
				} else if (target == _T("trac")) {
					settings::string_list s = settings_manager::get_core()->get_reg_sections();
					for (settings::string_list::const_iterator cit = s.begin(); cit != s.end(); ++cit) {
						std::wcout << _T("== ") << (*cit) << _T(" ==") << std::endl;
						settings::string_list k = settings_manager::get_core()->get_reg_keys(*cit);
						bool first = true;
						for (settings::string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
							settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(*cit, *citk);
							if (!desc.advanced) {
								if (first)
									std::wcout << _T("'''Normal settings'''") << std::endl;
								first = false;
								std::wcout << _T("||") << (*citk) << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description << std::endl;
							}
						}
						first = true;
						for (settings::string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
							settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(*cit, *citk);
							if (desc.advanced) {
								if (first)
									std::wcout << _T("'''Advanced settings'''") << std::endl;
								first = false;
								std::wcout << _T("||") << (*citk) << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description << std::endl;
							}
						}
					}
				} else {
					settings_manager::get_core()->update_defaults();
					settings_manager::get_core()->get()->save_to(target);
				}
			} catch (settings_exception e) {
				error_msg(_T("Failed to initialize settings: ") + e.getError());
			} catch (...) {
				error_msg(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			}
			core_->exitCore(true);
		}
		void error_msg(std::wstring msg) {
			core_->reportMessage(NSCAPI::error, __FILEW__, __LINE__, msg.c_str());
		}
		void debug_msg(std::wstring msg) {
			core_->reportMessage(NSCAPI::debug, __FILEW__, __LINE__, msg.c_str());
		}
	};
}