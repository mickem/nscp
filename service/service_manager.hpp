#pragma once

#ifdef _WIN32
#include <ServiceCmd.h>
#endif

namespace nsclient {
	namespace client {
#ifdef _WIN32
		class service_manager {
		private:
			std::wstring service_name_;

		public:
			service_manager(std::wstring service_name) : service_name_(service_name) {}

			static std::wstring get_default_service_name() {
				return DEFAULT_SERVICE_NAME;
			}
			static std::wstring get_default_service_desc() {
				return DEFAULT_SERVICE_DESC;
			}
			static std::wstring get_default_service_deps() {
				return DEFAULT_SERVICE_DEPS;
			}
			static std::wstring get_default_arguments() {
				return _T("service --run");
			}
			inline void print_msg(std::wstring str) {
				std::wcout << str << std::endl;
			}
			inline void print_error(std::wstring str) {
				std::wcerr << _T("ERROR: ") << str << std::endl;
			}
		public:
			int install(std::wstring service_description) {
				try {
					std::wstring args = get_default_arguments();
					if (service_name_ != get_default_service_name())
						args += _T(" --name ") + service_name_;
					serviceControll::Install(service_name_, service_description, get_default_service_deps(), SERVICE_WIN32_OWN_PROCESS, args);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service installation failed of '") + service_name_ + _T("' failed: ") + e.error_);
					return -1;
				}
				try {
					serviceControll::SetDescription(service_name_, service_description);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Couldn't set service description: ") + e.error_);
				}
				print_msg(_T("Service installed successfully!"));
				return 0;
			}
			int uninstall() {
				try {
					serviceControll::Uninstall(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service de-installation (") + service_name_ + _T(") failed; ") + e.error_ + _T("\nMaybe the service was not previously installed properly?"));
					return 0;
				}
				print_msg(_T("Service uninstalled successfully!"));
				return 0;
			}
			int start() {
				try {
					serviceControll::Start(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service failed to start: ") + e.error_);
					return -1;
				}
				return 0;
			}
			int stop() {
				try {
					serviceControll::Stop(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service failed to stop: ") + e.error_);
					return -1;
				}
				return 0;
			}
			int info() {
				try {
					std::wstring exe = serviceControll::get_exe_path(service_name_);
					print_error(_T("The Service uses: ") + exe);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Failed to find service: ") + e.error_);
					return -1;
				}
				return 0;
			}
		};
#else
		class service_manager {
		public:
			service_manager(std::wstring service_name) {}
			int unsupported() {
				std::wcout << _T("Service management is not supported on non Windows operating systems...") << std::endl;
				return -1;
			}
			int install(std::wstring service_description) {
				return unsupported();
			}
			int uninstall() {
				return unsupported();
			}
			int start() {
				return unsupported();
			}
			int stop() {
				return unsupported();
			}
			int info() {
				return unsupported();
			}
		};
#endif
	}
}
