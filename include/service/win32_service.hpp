/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>

#include <boost/thread/mutex.hpp>

#include <sysinfo.h>
#include <error/error.hpp>

#ifndef _T
#define _T(x) L ## x
#endif

namespace service_helper_impl {
	class service_exception {
		std::string what_;
	public:
		service_exception(std::string what) : what_(what) {
			OutputDebugString(utf8::cvt<std::wstring>(what).c_str());
		}
		std::string reason() {
			return what_;
		}
	};

	class w32_api_impl {
	public:

		// Win32 API defines
		typedef DWORD(WINAPI *LPHANDLER_FUNCTION_EX) (
			DWORD dwControl,     // requested control code
			DWORD dwEventType,   // event type
			LPVOID lpEventData,  // event data
			LPVOID lpContext     // user-defined context data
			);
		typedef SERVICE_STATUS_HANDLE(WINAPI *REGISTER_SERVICE_CTRL_HANDLER_EX) (
			LPCTSTR lpServiceName,                // name of service
			LPHANDLER_FUNCTION_EX lpHandlerProc,  // handler function
			LPVOID lpContext                      // user data
			);

		static SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerEx_(LPCTSTR lpServiceName, LPHANDLER_FUNCTION_EX lpHandlerProc, LPVOID lpContext) {
			HMODULE hAdvapi32 = NULL;
			REGISTER_SERVICE_CTRL_HANDLER_EX fRegisterServiceCtrlHandlerEx = NULL;

			if ((hAdvapi32 = GetModuleHandle(TEXT("Advapi32.dll"))) == NULL)
				return 0;
#ifdef _UNICODE
			fRegisterServiceCtrlHandlerEx = (REGISTER_SERVICE_CTRL_HANDLER_EX)GetProcAddress(hAdvapi32, "RegisterServiceCtrlHandlerExW");
#else
			fRegisterServiceCtrlHandlerEx = (REGISTER_SERVICE_CTRL_HANDLER_EX)GetProcAddress(hAdvapi32, "RegisterServiceCtrlHandlerExA");
#endif // _UNICODE
			if (!fRegisterServiceCtrlHandlerEx)
				return 0;
			return fRegisterServiceCtrlHandlerEx(lpServiceName, lpHandlerProc, lpContext);
		}
		typedef struct tagWTSSESSION_NOTIFICATION {
			DWORD cbSize;
			DWORD dwSessionId;
		} WTSSESSION_NOTIFICATION, *PWTSSESSION_NOTIFICATION;
#define WTS_SESSION_LOGON                  0x5
#define WTS_SESSION_LOGOFF                 0x6
	};

	/**
	 * @ingroup NSClient++
	 * Helper class to implement a NT service
	 *
	 * @version 1.0
	 * first version
	 *
	 * @date 02-13-2005
	 *
	 * @author mickem
	 *
	 * @par license
	 * This code is absolutely free to use and modify. The code is provided "as is" with
	 * no expressed or implied warranty. The author accepts no liability if it causes
	 * any damage to your computer, causes your pet to fall ill, increases baldness
	 * or makes your car start emitting strange noises when you start it up.
	 * This code has no bugs, just undocumented features!
	 *
	 * @todo
	 *
	 * @bug
	 *
	 */
	template <class TBase>
	class win32_service : public TBase {
	public:
	private:
		boost::mutex			stop_mutex_;
		SERVICE_STATUS          ssStatus;
		SERVICE_STATUS_HANDLE   sshStatusHandle;
		SERVICE_TABLE_ENTRY		*dispatchTable;
		DWORD					dwControlsAccepted;
		std::wstring			name_;
		wchar_t					*serviceName_;
	public:
		win32_service() : dispatchTable(NULL), serviceName_(NULL), name_(), dwControlsAccepted(SERVICE_ACCEPT_STOP) {}
		virtual ~win32_service() {
			delete[] dispatchTable;
			delete[] serviceName_;
		}

		void create_dispatch_table(std::wstring name) {
			serviceName_ = new wchar_t[name.length() + 2];
			wcsncpy(serviceName_, name.c_str(), name.length());
			dispatchTable = new SERVICE_TABLE_ENTRY[2];
			dispatchTable[0].lpServiceName = serviceName_;
			dispatchTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)service_helper_impl::win32_service<TBase>::service_main_dispatch;
			dispatchTable[1].lpServiceName = NULL;
			dispatchTable[1].lpServiceProc = NULL;
		}

		/** start */
		void start_and_wait(std::string name) {
			name_ = utf8::cvt<std::wstring>(name);
			print_debug(_T("Starting: ") + name_);
			create_dispatch_table(name_);
			StartServiceCtrlDispatcher();
		}

		// Callbacks into the client (implementer)
		/*
		virtual void handle_session_change(unsigned long dwSessionId, bool logon) = 0;
		virtual void handle_error(unsigned int line, wchar_t file, std::wstring message) = 0;
		virtual void handle_startup() = 0;
		virtual void handle_shutdown() = 0;
		*/

		// Callbacks from the W32 API
		static void WINAPI service_ctrl_dispatch(DWORD dwCtrlCode) {
			try {
				service_ctrl_dispatch_ex(dwCtrlCode, 0, NULL, NULL);
			} catch (service_helper::service_exception e) {
				handle_error(__LINE__, __FILE__, "Unknown service error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				handle_error(__LINE__, __FILE__, "Unknown service error!");
			}
		}
		static DWORD WINAPI service_ctrl_dispatch_ex(DWORD dwCtrlCode, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
			try {
				switch (dwCtrlCode) {
				case SERVICE_CONTROL_STOP:
					TBase::get_global_instance()->_report_status_to_SCMgr(SERVICE_STOP_PENDING, 0);
					TBase::get_global_instance()->stop_service();
					//ServiceStop();
					return 0;

				case SERVICE_CONTROL_INTERROGATE:
					break;

				case SERVICE_CONTROL_SESSIONCHANGE:
					if (lpEventData != NULL) {
						DWORD dwSessionId = reinterpret_cast<service_helper_impl::w32_api_impl::WTSSESSION_NOTIFICATION*>(lpEventData)->dwSessionId;
						if (dwEventType == WTS_SESSION_LOGON)
							TBase::get_global_instance()->handle_session_change(dwSessionId, true);
						else if (dwEventType == WTS_SESSION_LOGOFF)
							TBase::get_global_instance()->handle_session_change(dwSessionId, false);
					} else {
						TBase::get_global_instance()->handle_session_change(0, true);
					}
					break;

				default:
					break;
				}
				TBase::get_global_instance()->_report_status_to_SCMgr();
				return 0;
			} catch (service_helper::service_exception e) {
				handle_error(__LINE__, __FILE__, "Unknown service error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				handle_error(__LINE__, __FILE__, "Unknown service error!");
			}
			return 0;
		}
		static void WINAPI service_main_dispatch(DWORD dwArgc, LPTSTR *lpszArgv) {
			try {
				TBase::get_global_instance()->_service_main(dwArgc, lpszArgv);
			} catch (service_helper::service_exception e) {
				handle_error(__LINE__, __FILE__, "Unknown service error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				handle_error(__LINE__, __FILE__, "Unknown service error!");
			}
		}
	private:
		// Internal helper functions
		inline void print_debug(std::wstring s) {
			OutputDebugString(s.c_str());
		}
		inline void print_debug(wchar_t *s) {
			OutputDebugString(s);
		}
		static void handle_error(const int line, const char* file, std::string message) {
			OutputDebugString(utf8::cvt<std::wstring>(message).c_str());
		}

		bool StartServiceCtrlDispatcher() {
			BOOL ret = ::StartServiceCtrlDispatcher(dispatchTable);
			if (ret == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
				OutputDebugString(_T("We are running in console mode, terminating..."));
				return false;
			}
			return ret != 0;
		}

		void _service_main(DWORD dwArgc, LPTSTR *lpszArgv) {
			print_debug(_T("service_main launcing..."));
			sshStatusHandle = service_helper_impl::w32_api_impl::RegisterServiceCtrlHandlerEx_(name_.c_str(), service_helper_impl::win32_service<TBase>::service_ctrl_dispatch_ex, NULL);
			if (sshStatusHandle == 0) {
				print_debug(_T("Failed to register RegisterServiceCtrlHandlerEx_ (attempting to use normal one)..."));
				sshStatusHandle = RegisterServiceCtrlHandler(name_.c_str(), service_helper_impl::win32_service<TBase>::service_ctrl_dispatch);
			} else {
				if (systemInfo::isAboveXP(systemInfo::getOSVersion())) {
					dwControlsAccepted |= SERVICE_ACCEPT_SESSIONCHANGE;
					print_debug(_T("Windows XP or above detected so enabling session messages..."));
				} else
					print_debug(_T("Windows 2000 or older detected (disabling session messages)"));
			}
			if (sshStatusHandle == 0)
				throw service_exception("Failed to register service: " + error::lookup::last_error());

			ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
			ssStatus.dwServiceSpecificExitCode = 0;
			ssStatus.dwControlsAccepted = dwControlsAccepted;

			// report the status to the service control manager.
			if (!_report_status_to_SCMgr(SERVICE_START_PENDING, 3000)) {
				_report_status_to_SCMgr(SERVICE_STOPPED, 0);
				throw service_exception("Failed to report service status: " + error::lookup::last_error());
			}
			try {
				_handle_start(dwArgc, lpszArgv);
			} catch (...) {
				throw service_exception("Uncaught exception in service... terminating: " + error::lookup::last_error());
			}
			_report_status_to_SCMgr(SERVICE_STOPPED, 0);
		}

		BOOL _report_status_to_SCMgr() {
			return _report_status_to_SCMgr(ssStatus.dwCurrentState, 0);
		}

		/**
		* Sets the current status of the service and reports it to the Service Control Manager
		*
		* @param dwCurrentState
		* @param dwWin32ExitCode
		* @param dwWaitHint
		* @return
		*
		* @author mickem
		*
		* @date 03-13-2004
		*
		*/
		BOOL _report_status_to_SCMgr(DWORD dwCurrentState, DWORD dwWaitHint) {
			static DWORD dwCheckPoint = 1;
			BOOL fResult = TRUE;

			if (dwCurrentState == SERVICE_START_PENDING)
				ssStatus.dwControlsAccepted = 0;
			else
				ssStatus.dwControlsAccepted = dwControlsAccepted;

			ssStatus.dwCurrentState = dwCurrentState;
			ssStatus.dwWin32ExitCode = 0;
			ssStatus.dwWaitHint = dwWaitHint;

			if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
				ssStatus.dwCheckPoint = 0;
			else
				ssStatus.dwCheckPoint = dwCheckPoint++;

			// Report the status of the service to the service control manager.
			fResult = SetServiceStatus(sshStatusHandle, &ssStatus);

			return fResult;
		}

		/**
		* Actual code of the service that does the work.
		*
		* @param dwArgc
		* @param *lpszArgv
		*
		* @author mickem
		*
		* @date 03-13-2004
		*
		*/
		void _handle_start(DWORD dwArgc, LPTSTR *lpszArgv) {
			stop_mutex_.lock();
			if (!_report_status_to_SCMgr(SERVICE_RUNNING, 0)) {
				stop_service();
				return;
			}

			TBase::handle_startup(utf8::cvt<std::string>(name_));

			stop_mutex_.lock();

			print_debug(_T("Shutting down: ") + name_);
			TBase::handle_shutdown(utf8::cvt<std::string>(name_));
		}

		/**
		* Stops the service
		*
		*
		* @author mickem
		*
		* @date 03-13-2004
		*
		*/
		void stop_service() {
			stop_mutex_.unlock();
		}
	};
}