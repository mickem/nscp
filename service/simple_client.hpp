#pragma once

class NSClientT;
namespace nsclient {
	class simple_client {
		NSClient *core_;
	public:
		simple_client(NSClient *core) : core_(core) {}
		void start() {
			core_->enableDebug(true);
			if (!core_->initCore(true)) {
				std::wcout << _T("Service *NOT* started!") << std::endl;
				return;
			}
			std::wcout << _T("Using settings from: ") << settings_manager::get_core()->get_settings_type_desc() << std::endl;
			std::wcout << _T("Enter command to inject or exit to terminate...") << std::endl;
/*
			Settings::get_settings()->clear_cache();
			LOG_MESSAGE_STD( _T("test 001: ") + SETTINGS_GET_STRING(NSCLIENT_TEST1) );
			LOG_MESSAGE_STD( _T("test 002: ") + SETTINGS_GET_STRING(NSCLIENT_TEST2) );
			LOG_MESSAGE_STD( _T("test 003: ") + SETTINGS_GET_STRING(NSCLIENT_TEST3) );
			LOG_MESSAGE_STD( _T("test 004: ") + SETTINGS_GET_STRING(NSCLIENT_TEST4) );

			Settings::get_settings()->save_to(_T("test.ini"));
*/
			std::wstring s = _T("");
			std::wstring buff = _T("");
			while (true) {
				std::wcin >> s;
				if (s == _T("exit")) {
					std::wcout << _T("Exiting...") << std::endl;
					break;
				} else if (s == _T("plugins")) {
					std::wcout << _T("Listing plugins...") << std::endl;
					core_->listPlugins();
				} else if (s == _T("list")) {
					std::wcout << _T("Listing commands...") << std::endl;
					std::list<std::wstring> lst = core_->list_commands();
					for (std::list<std::wstring>::const_iterator cit = lst.begin(); cit!=lst.end();++cit)
						std::wcout << *cit << std::endl;
					std::wcout << _T("Listing commands...Done") << std::endl;
				} else if (s == _T("off") && buff == _T("debug ")) {
					std::wcout << _T("Setting debug log off...") << std::endl;
					core_->enableDebug(false);
				} else if (s == _T("on") && buff == _T("debug ")) {
					std::wcout << _T("Setting debug log on...") << std::endl;
					core_->enableDebug(true);
				} else if (s == _T("reattach")) {
					std::wcout << _T("Reattaching to session 0") << std::endl;
					core_->startTrayIcon(0);
				} else if (s == _T("assert")) {
					throw "test";
				} else if (std::cin.peek() < 15) {
					buff += s;
					strEx::token t = strEx::getToken(buff, ' ');
					std::wstring msg, perf;
					NSCAPI::nagiosReturn ret = core_->inject(t.first, t.second, ' ', true, msg, perf);
					if (ret == NSCAPI::returnIgnored) {
						std::wcout << _T("No handler for command: ") << t.first << std::endl;
					} else {
						std::wcout << NSCHelper::translateReturn(ret) << _T(":");
						std::cout << strEx::wstring_to_string(msg);
						if (!perf.empty())
							std::cout << "|" << strEx::wstring_to_string(perf);
						std::wcout << std::endl;
					}
					buff = _T("");
				} else {
					buff += s + _T(" ");
				}
			}
			core_->exitCore(true);
		}
	};
}