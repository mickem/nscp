#include <windows.h>
#include <WinSvc.h>
#include <error.hpp>
#include "EnumNtSrv.h"

#include <buffer.hpp>
#include <handle.hpp>

#include <win_sysinfo/win_sysinfo.hpp>


struct service_closer  {
	static void close(SC_HANDLE handle) {
		CloseServiceHandle(handle);
	}
};
typedef hlp::handle<SC_HANDLE, service_closer> service_handle;
namespace services_helper {
	DWORD parse_service_type(const std::string str) {
		DWORD ret = 0;
		BOOST_FOREACH(const std::string key, strEx::s::splitEx(str, std::string(","))) {
			if (key == "driver" || key == "drv")
				ret |= SERVICE_DRIVER;
			else if (key == "file-system-driver" || key == "fs-drv")
				ret |= SERVICE_FILE_SYSTEM_DRIVER;
			else if (key == "kernel-driver" || key == "k-drv")
				ret |= SERVICE_KERNEL_DRIVER;
			else if (key == "service" || key == "svc")
				ret |= SERVICE_WIN32;
			else if (key == "service-own-process" || key == "svc-own")
				ret |= SERVICE_WIN32_OWN_PROCESS;
			else if (key == "service-share-process" || key == "svc-shr")
				ret |= SERVICE_WIN32_SHARE_PROCESS;
			else
				throw nscp_exception("Invalid service type specified: " + key);
		}
		return ret;
	}
	DWORD parse_service_state(const std::string str) {
		DWORD ret = 0;
		BOOST_FOREACH(const std::string key, strEx::s::splitEx(str, std::string(","))) {
			if (key == "active")
				ret |= SERVICE_ACTIVE;
			else if (key == "inactive")
				ret |= SERVICE_INACTIVE;
			else if (key == "all")
				ret |= SERVICE_STATE_ALL;
			else
				throw nscp_exception("Invalid service type specified: " + key);
		}
		return ret;
	}

	hlp::buffer<BYTE, QUERY_SERVICE_CONFIG*> queryServiceConfig(SC_HANDLE hService, std::string service) {
		DWORD bytesNeeded = 0;
		DWORD deErr = 0;

		if (QueryServiceConfig(hService, NULL, 0, &bytesNeeded) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			throw nscp_exception("Failed to query service: " + service + ": " + error::lookup::last_error(deErr));

		hlp::buffer<BYTE, QUERY_SERVICE_CONFIG*> buf(bytesNeeded+10);
		if (!QueryServiceConfig(hService, buf.get(), bytesNeeded, &bytesNeeded))
			throw nscp_exception("Failed to query service: " + service + ": " + error::lookup::last_error());
		return buf;
	}

	hlp::buffer<BYTE, SERVICE_STATUS_PROCESS*> queryServiceStatusEx(SC_HANDLE hService, std::string service) {
		DWORD bytesNeeded = 0;
		DWORD deErr = 0;

		if (windows::winapi::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, NULL, 0, &bytesNeeded) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			throw nscp_exception("Failed to query service: " + service + ": " + error::lookup::last_error(deErr));

		hlp::buffer<BYTE, SERVICE_STATUS_PROCESS*> buf(bytesNeeded+10);
		if (!windows::winapi::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, buf, bytesNeeded, &bytesNeeded))
			throw nscp_exception("Failed to query service: " + service + ": " + error::lookup::last_error());
		return buf;
	}

	void fetch_triggers(service_handle &hService, service_info &info) {
		DWORD bytesNeeded = 0;
		DWORD deErr = 0;
		if (QueryServiceConfig2W(hService, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &bytesNeeded) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			return;
		hlp::buffer<BYTE> buffer(bytesNeeded+10);

		if (!QueryServiceConfig2W(hService, SERVICE_CONFIG_TRIGGER_INFO, buffer.get(), bytesNeeded, &bytesNeeded))
			throw nscp_exception("Failed to open service: " + info.name);

		info.triggers = buffer.get_t<SERVICE_TRIGGER_INFO*>()->cTriggers;
	}

	void fetch_delayed(service_handle &hService, service_info &info) {
		SERVICE_DELAYED_AUTO_START_INFO delayed;
		DWORD size=sizeof(SERVICE_DELAYED_AUTO_START_INFO);
		if (windows::winapi::QueryServiceConfig2W(hService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, reinterpret_cast<LPBYTE>(&delayed), size, &size)) {
			info.delayed = delayed.fDelayedAutostart;
		}
	}

	std::list<service_info> enum_services(const std::string computer, DWORD dwServiceType, DWORD dwServiceState) {
		std::list<service_info> ret;
		std::wstring comp = utf8::cvt<std::wstring>(computer);

		service_handle sc = OpenSCManager(comp.empty()?NULL:comp.c_str(),NULL,SC_MANAGER_ENUMERATE_SERVICE);
		if (!sc) 
			throw nscp_exception("Failed to open service manager: " + error::lookup::last_error());

		DWORD bytesNeeded = 0;
		DWORD count = 0;
		DWORD handle = 0;
		BOOL bRet = windows::winapi::EnumServicesStatusEx(sc, SC_ENUM_PROCESS_INFO, dwServiceType, dwServiceState, NULL, 0, &bytesNeeded, &count, &handle, NULL);
		if (bRet || GetLastError() != ERROR_MORE_DATA)
			throw nscp_exception("Failed to enumerate services");

		hlp::buffer<BYTE, ENUM_SERVICE_STATUS_PROCESS*> buf(bytesNeeded+10);
		bRet = windows::winapi::EnumServicesStatusEx(sc, SC_ENUM_PROCESS_INFO, dwServiceType, dwServiceState, buf, bytesNeeded, &bytesNeeded, &count, &handle, NULL);
		if (!bRet) 
			throw nscp_exception("Failed to enumerate service: " + error::lookup::last_error());
		ENUM_SERVICE_STATUS_PROCESS *data = buf.get();
		for (DWORD i=0; i<count;++i) {
			service_info info(utf8::cvt<std::string>(data[i].lpServiceName), utf8::cvt<std::string>(data[i].lpDisplayName));
			info.pid = data[i].ServiceStatusProcess.dwProcessId;
			info.state = data[i].ServiceStatusProcess.dwCurrentState;
			info.type = data[i].ServiceStatusProcess.dwServiceType;

			service_handle hService = OpenService(sc, data[i].lpServiceName, SERVICE_QUERY_CONFIG);
			if (!hService)
				throw nscp_exception("Failed to open service: " + info.name);

			hlp::buffer<BYTE, QUERY_SERVICE_CONFIG*> qscData = queryServiceConfig(hService, info.name);
			info.start_type = qscData.get()->dwStartType;
			info.binary_path = utf8::cvt<std::string>(qscData.get()->lpBinaryPathName);
			info.error_control = qscData.get()->dwErrorControl;

			fetch_delayed(hService, info);
			fetch_triggers(hService, info);
			ret.push_back(info);
		}
		return ret;
	}

	service_info get_service_info(const std::string computer, const std::string service) {
		std::wstring comp = utf8::cvt<std::wstring>(computer);

		service_handle sc = OpenSCManager(comp.empty()?NULL:comp.c_str(),NULL,SC_MANAGER_ENUMERATE_SERVICE);
		if (!sc) 
			throw nscp_exception("Failed to open service manager: " + error::lookup::last_error());

		service_handle hService = OpenService(sc, utf8::cvt<std::wstring>(service).c_str(), SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS );
		if (!hService)
			throw nscp_exception("Failed to open service: " + service);

		hlp::buffer<BYTE, SERVICE_STATUS_PROCESS*> ssp = queryServiceStatusEx(hService, service);

		service_info info(service, "TODO");
		info.pid = ssp.get()->dwProcessId;
		info.state = ssp.get()->dwCurrentState;
		info.type = ssp.get()->dwServiceType;

		DWORD bytesNeeded2 = 0;
		DWORD deErr = 0;
		if (QueryServiceConfig(hService, NULL, 0, &bytesNeeded2) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			throw nscp_exception("Failed to open service " + info.name + ": " + error::lookup::last_error(deErr));
		hlp::buffer<BYTE> buf2(bytesNeeded2+10);

		if (!QueryServiceConfig(hService, reinterpret_cast<QUERY_SERVICE_CONFIG*>(buf2.get()), bytesNeeded2, &bytesNeeded2))
			throw nscp_exception("Failed to open service: " + info.name);
		QUERY_SERVICE_CONFIG *data2 = reinterpret_cast<QUERY_SERVICE_CONFIG*>(buf2.get());
		info.start_type = data2->dwStartType;
		info.binary_path = utf8::cvt<std::string>(data2->lpBinaryPathName);
		info.error_control = data2->dwErrorControl;

		fetch_delayed(hService, info);
		fetch_triggers(hService, info);
		return info;
	}


	long long service_info::parse_start_type(const std::string &s) {
		if (s == "auto")
			return SERVICE_AUTO_START;
		if (s == "boot")
			return SERVICE_BOOT_START;
		if (s == "demand")
			return SERVICE_DEMAND_START;
		if (s == "disabled")
			return SERVICE_DISABLED;
		if (s == "system")
			return SERVICE_SYSTEM_START;
		throw std::string("Invalid start type: " + s);
	}
	long long service_info::parse_state(const std::string &s) {
		if (s == "continuing")
			return SERVICE_CONTINUE_PENDING;
		if (s == "pausing")
			return SERVICE_PAUSE_PENDING;
		if (s == "paused")
			return SERVICE_PAUSED;
		if (s == "running")
			return SERVICE_RUNNING;
		if (s == "started")
			return SERVICE_RUNNING;
		if (s == "starting")
			return SERVICE_START_PENDING;
		if (s == "stopping")
			return SERVICE_STOP_PENDING;
		if (s == "stopped")
			return SERVICE_STOPPED;
		throw std::string("Invalid state: " + s);
	}

	std::string service_info::get_state_s() const {
		if (state == SERVICE_CONTINUE_PENDING)
			return "continuing";
		if (state == SERVICE_PAUSE_PENDING)
			return "pausing";
		if (state == SERVICE_PAUSED)
			return "paused";
		if (state == SERVICE_RUNNING)
			return "running";
		if (state == SERVICE_START_PENDING)
			return "starting";
		if (state == SERVICE_STOP_PENDING)
			return "stopping";
		if (state == SERVICE_STOPPED)
			return "stopped";
		return "unknown";
	}
	std::string service_info::get_legacy_state_s() const {
		if (state == SERVICE_CONTINUE_PENDING)
			return "Continuing";
		if (state == SERVICE_PAUSE_PENDING)
			return "Pausing";
		if (state == SERVICE_PAUSED)
			return "Paused";
		if (state == SERVICE_RUNNING)
			return "Started";
		if (state == SERVICE_START_PENDING)
			return "Starting";
		if (state == SERVICE_STOP_PENDING)
			return "Stopping";
		if (state == SERVICE_STOPPED)
			return "Stopped";
		return "unknown";
	}
	std::string service_info::get_start_type_s() const {
		if (delayed)
			return "delayed";
		if (start_type == SERVICE_AUTO_START)
			return "auto";
		if (start_type == SERVICE_BOOT_START)
			return "boot";
		if (start_type == SERVICE_DEMAND_START)
			return "demand";
		if (start_type == SERVICE_DISABLED)
			return "disabled";
		if (start_type == SERVICE_SYSTEM_START)
			return "system";
		return "unknown";
	}
	// Return a service type as a string
	std::string service_info::get_type() const {
		std::string str = "";
		if (type&SERVICE_FILE_SYSTEM_DRIVER)
			strEx::append_list(str, "system-driver");
		if (type&SERVICE_KERNEL_DRIVER)
			strEx::append_list(str, "kernel-driver");
		if (type&SERVICE_WIN32_OWN_PROCESS)
			strEx::append_list(str, "service-own-process");
		if (type&SERVICE_WIN32_SHARE_PROCESS)
			strEx::append_list(str, "service-shared-process");
		if (type&SERVICE_WIN32)
			strEx::append_list(str, "service");
		if (type&SERVICE_INTERACTIVE_PROCESS)
			strEx::append_list(str, "interactive");
		return str;
	}

}
