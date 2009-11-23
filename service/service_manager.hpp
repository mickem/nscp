#pragma once

#include <boost/program_options.hpp>
#ifdef _WIN32
#include <ServiceCmd.h>
#endif

namespace nsclient {
	namespace client {
#ifdef _WIN32
		namespace po = boost::program_options;
		class service_manager {
		private:
			bool output_gui_;
			int argc_;
			wchar_t** argv_;
			std::wstring service_name_;
			po::options_description desc_;
			po::variables_map vm_;

		public:
			service_manager(int argc, wchar_t* argv[]) : argc_(argc), argv_(argv), output_gui_(false), desc_("Allowed options") {}
		private:

			void add_global_options() {
				desc_.add_options()
					("help", "produce help message")
					("gui", po::value<bool>(&output_gui_)->zero_tokens()->default_value(false), "show message in the GUI")
					("name", po::value<std::wstring>(&service_name_)->default_value(SZSERVICENAME), "service name to use")
					;
			}

			void print_error(std::wstring message) {
				if (output_gui_)
					::MessageBox(NULL, message.c_str(), _T("NSClient++ Error:"), MB_OK|MB_ICONERROR);
				else
					std::wcerr << message << std::endl;
			}
			void print_msg(std::wstring title, std::wstring message) {
				if (output_gui_)
					::MessageBox(NULL, message.c_str(), title.c_str(), MB_OK|MB_ICONINFORMATION);
				else
					std::wcerr << title << _T(": ") << message << std::endl;
			}

			boolean parse() {
				try {
					po::store(po::parse_command_line(argc_, argv_, desc_), vm_);
					po::notify(vm_);
					return true;
				} catch (po::unknown_option &e) {
					print_error(_T("Failed to parse: ") + to_wstring(e.what()));
					return false;
				} catch (std::exception &e) {
					print_error(_T("Failed to parse: ") + to_wstring(e.what()));
					return false;
				} catch (...) {
					print_error(_T("Failed to parse: <UNKNOWN EXCEPTION>"));
					return false;
				}
			}

		public:
			int install() {
				bool bStart = false;
				std::wstring service_description;

				add_global_options();
				desc_.add_options() 
					("start",  po::value<bool>(&bStart)->zero_tokens()->default_value(false), "start the service")
					("description", po::value<std::wstring>(&service_description)->default_value(SZSERVICEDISPLAYNAME), "service description to use")
					;
				if (!parse())
					return -1;

				try {
					serviceControll::Install(service_name_, service_description, SZDEPENDENCIES);
					if (bStart)
						serviceControll::Start(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service installation failed: ") + e.error_);
					return -1;
				}
				try {
					serviceControll::SetDescription(service_name_, service_description);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Couldn't set service description: ") + e.error_);
				}
				print_msg(_T("Service installed"), _T("Service installed successfully!"));
				return 0;
			}
			int uninstall() {
				bool bStop = false;
				add_global_options();
				desc_.add_options() ("stop", po::value<bool>(&bStop)->zero_tokens()->default_value(false), "stop the service") ;
				if (!parse())
					return -1;

				try {
					if (bStop)
						serviceControll::Stop(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Failed to stop service (") + service_name_ + _T(") failed; ") + e.error_);
				}
				try {
					serviceControll::Uninstall(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service de-installation (") + service_name_ + _T(") failed; ") + e.error_ + _T("\nMaybe the service was not previously installed properly?"));
					return 0;
				}
				print_msg(_T("Service uninstalled"), _T("Service uninstalled successfully!"));
				return 0;
			}
			int start() {
				add_global_options();
				if (!parse())
					return -1;

				try {
					serviceControll::Start(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service failed to start: ") + e.error_);
					return -1;
				}
				return 0;
			}
			int stop() {
				add_global_options();
				if (!parse())
					return -1;

				try {
					serviceControll::Stop(service_name_);
				} catch (const serviceControll::SCException& e) {
					print_error(_T("Service failed to stop: ") + e.error_);
					return -1;
				}
				return 0;
			}
			int print_command() {
				add_global_options();
				if (!parse())
					return -1;

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
			service_manager(int argc, wchar_t* argv[]) {}
			int unsupported() {
				std::wcout << _T("Service management is not supported on non Windows operating systems...") << std::endl;
				return -1;
			}
			int install() {
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
			int print_command() {
				return unsupported();
			}
		};
#endif
	}
}
