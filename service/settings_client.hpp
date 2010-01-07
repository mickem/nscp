#pragma once


class NSClientT;
namespace nsclient {
	class settings_client {
		NSClient* core_;
	public:
		settings_client(NSClient* core) : core_(core) {

		}
		void parse(std::wstring sarg, int argc, wchar_t* argv[]) {
			core_->enableDebug(true);
			if (!core_->initCore(false)) {
				std::wcout << _T("Service *NOT* started!") << std::endl;
				return;
			}
			std::wcout << _T("Using settings from: ") << settings_manager::get_core()->get_settings_type_desc() << std::endl;

			try {
				if (sarg == _T("migrate")) {
					if (argc == 0) {
						error_msg(_T("In correct syntax: nsclient++ -settings migrate <to>"));
						return;
					}
					std::wstring to = argv[0];
					debug_msg(_T("Migrating to: ") + to);
					try {
						settings_manager::get_core()->migrate_to(Settings::SettingsCore::string_to_type(to));
					} catch (SettingsException e) {
						error_msg(_T("Failed to migrate settings: ") + e.getError());
					}
				} else if (sarg == _T("generate")) {
					if (argc == 0) {
						error_msg(_T("In correct syntax: nsclient++ -settings generate <what>"));
						error_msg(_T("     where <what> is one of ths following:"));
						error_msg(_T("      trac"));
						error_msg(_T("      default"));
						error_msg(_T("      <type>"));
						return;
					}
					std::wstring arg1 = argv[0];
					if (arg1 == _T("default")) {
						try {
							core_->load_all_plugins(NSCAPI::dontStart);
							settings_manager::get_core()->update_defaults();
							settings_manager::get_core()->get()->save();
						} catch (SettingsException e) {
							error_msg(_T("Failed to migrate settings: ") + e.getError());
						}
					} else if (arg1 == _T("trac")) {
						try {
							core_->load_all_plugins(NSCAPI::dontStart);

							Settings::string_list s = settings_manager::get_core()->get_reg_sections();
							for (Settings::string_list::const_iterator cit = s.begin(); cit != s.end(); ++cit) {
								std::wcout << _T("== ") << (*cit) << _T(" ==") << std::endl;
								Settings::string_list k = settings_manager::get_core()->get_reg_keys(*cit);
								bool first = true;
								for (Settings::string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
									Settings::SettingsCore::key_description desc = settings_manager::get_core()->get_registred_key(*cit, *citk);
									if (!desc.advanced) {
										if (first)
											std::wcout << _T("'''Normal settings'''") << std::endl;
										first = false;
										std::wcout << _T("||") << (*citk) << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description
											<< std::endl;
									}
								}
								first = true;
								for (Settings::string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
									Settings::SettingsCore::key_description desc = settings_manager::get_core()->get_registred_key(*cit, *citk);
									if (desc.advanced) {
										if (first)
											std::wcout << _T("'''Advanced settings'''") << std::endl;
										first = false;
										std::wcout << _T("||") << (*citk) << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description
											<< std::endl;
									}
								}
							}
						} catch (SettingsException e) {
							error_msg(_T("Failed to migrate settings: ") + e.getError());
						}
					} else {
						try {
							Settings::SettingsCore::settings_type type = settings_manager::get_core()->string_to_type(arg1);
							core_->load_all_plugins(NSCAPI::dontStart);
							settings_manager::get_core()->update_defaults();
							settings_manager::get_core()->get(type)->save();
						} catch (SettingsException e) {
							error_msg(_T("Failed to migrate settings: ") + e.getError());
						}
					}
				} else {
					std::wcout << _T("Unknown keyword: ") << sarg << std::endl;
					help();
				}


			} catch (SettingsException e) {
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

		void help() {
			std::wcout << _T("Usage: nsclient++ -settings <key>") << std::endl;
			std::wcout << _T("In correct syntax: nsclient++ -settings <keyword>") << std::endl;
			std::wcout << _T(" <keyword> : ") << std::endl;
			std::wcout << _T("   migrate - migrate to a new setings subsystem") << std::endl;
			std::wcout << _T("   copy    - copy settings from one subsystem to another") << std::endl;
			std::wcout << _T("   set     - Set a setting system as the default store") << std::endl;
		}

	};
}