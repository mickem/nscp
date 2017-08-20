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

#include <windows.h>
#include <WinSvc.h>
#include <error/error.hpp>
#include <nsclient/nsclient_exception.hpp>
#include "EnumNtSrv.h"

#include <buffer.hpp>
#include <handle.hpp>

#include <boost/unordered_map.hpp>

#include <win_sysinfo/win_sysinfo.hpp>

#include <utf8.hpp>
#include <str/utils.hpp>
#include <str/format.hpp>

typedef boost::unordered_map<std::string, std::string> hash_map;
hash_map smap;


std::string services_helper::get_service_classification(const std::string &name) {
	hash_map::const_iterator cit = smap.find(name);
	if (cit == smap.end())
		return "custom";
	return cit->second;
}

void services_helper::init() {
	smap["BITS"] = "essential";
	smap["COMSysApp"] = "essential";
	smap["Dnscache"] = "essential";
	smap["DPS"] = "essential";
	smap["EventLog"] = "essential";
	smap["Eventsystem"] = "essential";
	smap["iphlpsvc"] = "essential";
	smap["LanmanServer"] = "essential";
	smap["LanmanWorkstation"] = "essential";
	smap["MpsSvc"] = "essential";
	smap["Netlogon"] = "essential";
	smap["pla"] = "essential";
	smap["ProfSvc"] = "essential";
	smap["RpcEptMapper"] = "essential";
	smap["RpcSs"] = "essential";
	smap["UALSVC"] = "essential";
	smap["vmicheartbeat"] = "essential";
	smap["vmicshutdown"] = "essential";
	smap["vmictimesync"] = "essential";
	smap["VMTools"] = "essential";
	smap["Winmgmt"] = "essential";
	smap["WinRM"] = "essential";
	smap["ALG"] = "ignored";
	smap["AppReadiness"] = "ignored";
	smap["AppXSvc"] = "ignored";
	smap["dot3svc"] = "ignored";
	smap["DsmSvc"] = "ignored";
	smap["EFS"] = "ignored";
	smap["hidserv"] = "ignored";
	smap["lltdsvc"] = "ignored";
	smap["msiserver"] = "ignored";
	smap["SNMPTRAP"] = "ignored";
	smap["SysMain"] = "ignored";
	smap["TabletInputService"] = "ignored";
	smap["TrustedInstaller"] = "ignored";
	smap["upnphost"] = "ignored";
	smap["vmicvss"] = "ignored";
	smap["vmvss"] = "ignored";
	smap["VSS"] = "ignored";
	smap["Wecsvc"] = "ignored";
	smap["adfssrv"] = "role";
	smap["ADWS"] = "role";
	smap["BITSCompactServer"] = "role";
	smap["c2wts"] = "role";
	smap["CertSvc"] = "role";
	smap["ClusSvc"] = "role";
	smap["ddpsvc"] = "role";
	smap["ddpvssvc"] = "role";
	smap["Dfs"] = "role";
	smap["DFSR"] = "role";
	smap["DNS"] = "role";
	smap["drs"] = "role";
	smap["Eaphost"] = "role";
	smap["Fax"] = "role";
	smap["fdPHost"] = "role";
	smap["FDResPub"] = "role";
	smap["fssagent"] = "role";
	smap["hkmsvc"] = "role";
	smap["IAS"] = "role";
	smap["IISADMIN"] = "role";
	smap["IKEEXT"] = "role";
	smap["IsmServ"] = "role";
	smap["Kdc"] = "role";
	smap["KdsSvc"] = "role";
	smap["KeyIso"] = "role";
	smap["KPSSVC"] = "role";
	smap["KtmRm"] = "role";
	smap["LPDSVC"] = "role";
	smap["MMCSS"] = "role";
	smap["MSDTC"] = "role";
	smap["MSiSCSI"] = "role";
	smap["MSiSNS"] = "role";
	smap["MSMQ"] = "role";
	smap["MSMQTriggers"] = "role";
	smap["MSSQL$MICROSOFT##WID"] = "role";
	smap["MSStrgSvc"] = "role";
	smap["NetMsmqActivator"] = "role";
	smap["NetPipeActivator"] = "role";
	smap["NetTcpActivator"] = "role";
	smap["NetTcpPortSharing"] = "role";
	smap["NfsService"] = "role";
	smap["NtFrs"] = "role";
	smap["OcspSvc"] = "role";
	smap["PeerDistSvc"] = "role";
	smap["PNRPAutoReg"] = "role";
	smap["PNRPsvc"] = "role";
	smap["PrintNotify"] = "role";
	smap["RaMgmtSvc"] = "role";
	smap["RasAuto"] = "role";
	smap["RasMan"] = "role";
	smap["RDMS"] = "role";
	smap["RemoteAccess"] = "role";
	smap["RPCHTTPLBS"] = "role";
	smap["RpcLocator"] = "role";
	smap["rqs"] = "role";
	smap["SessionEnv"] = "role";
	smap["simptcp"] = "role";
	smap["SmbHash"] = "role";
	smap["SmbWitness"] = "role";
	smap["smphost"] = "role";
	smap["SMTPSVC"] = "role";
	smap["SNMP"] = "role";
	smap["SrmReports"] = "role";
	smap["SrmSvc"] = "role";
	smap["SstpSvc"] = "role";
	smap["stisvc"] = "role";
	smap["StorSvc"] = "role";
	smap["svsvc"] = "role";
	smap["SyncShareSvc"] = "role";
	smap["SyncShareTTSvc"] = "role";
	smap["TapiSrv"] = "role";
	smap["TermService"] = "role";
	smap["TermServLicensing"] = "role";
	smap["THREADORDER"] = "role";
	smap["TieringEngineService"] = "role";
	smap["TimeBroker"] = "role";
	smap["TlntSvr"] = "role";
	smap["TPAutoConnSvc"] = "role";
	smap["TPVCGateway"] = "role";
	smap["TrkWks"] = "role";
	smap["TScPubRPC"] = "role";
	smap["TSGateway"] = "role";
	smap["Tssdis"] = "role";
	smap["UI0Detect"] = "role";
	smap["UmRdpService"] = "role";
	smap["w3logsvc"] = "role";
	smap["W3SVC"] = "role";
	smap["WAS"] = "role";
	smap["wbengine"] = "role";
	smap["WbioSrvc"] = "role";
	smap["WDSServer"] = "role";
	smap["WEPHOSTSVC"] = "role";
	smap["wercplsupport"] = "role";
	smap["WerSvc"] = "role";
	smap["WFFSvc"] = "role";
	smap["WiaRpc"] = "role";
	smap["WIDWriter"] = "role";
	smap["WinTarget"] = "role";
	smap["WSusCertServer"] = "role";
	smap["WsusService"] = "role";
	smap["AeLookupSvc"] = "supporting";
	smap["AppHostSvc"] = "supporting";
	smap["AppIDSvc"] = "supporting";
	smap["Appinfo"] = "supporting";
	smap["AppMgmt"] = "supporting";
	smap["aspnet_state"] = "supporting";
	smap["AudioEndpointBuilder"] = "supporting";
	smap["Audiosrv"] = "supporting";
	smap["AxInstSV"] = "supporting";
	smap["BDESVC"] = "supporting";
	smap["Browser"] = "supporting";
	smap["CertPropSvc"] = "supporting";
	smap["CryptSvc"] = "supporting";
	smap["CscService"] = "supporting";
	smap["defragsvc"] = "supporting";
	smap["DeviceAssociationService"] = "supporting";
	smap["DeviceInstall"] = "supporting";
	smap["Dhcp"] = "supporting";
	smap["DiagTrack"] = "supporting";
	smap["FontCache"] = "supporting";
	smap["gpsvc"] = "supporting";
	smap["IEEtwCollectorService"] = "supporting";
	smap["lmhosts"] = "supporting";
	smap["napagent"] = "supporting";
	smap["NcaSvc"] = "supporting";
	smap["NcbService"] = "supporting";
	smap["Netman"] = "supporting";
	smap["netprofm"] = "supporting";
	smap["NlaSvc"] = "supporting";
	smap["nsi"] = "supporting";
	smap["p2pimsvc"] = "supporting";
	smap["PerfHost"] = "supporting";
	smap["PlugPlay"] = "supporting";
	smap["PolicyAgent"] = "supporting";
	smap["QWAVE"] = "supporting";
	smap["RemoteRegistry"] = "supporting";
	smap["RSoPProv"] = "supporting";
	smap["sacsvr"] = "supporting";
	smap["SCardSvr"] = "supporting";
	smap["ScDeviceEnum"] = "supporting";
	smap["SCPolicySvc"] = "supporting";
	smap["seclogon"] = "supporting";
	smap["SENS"] = "supporting";
	smap["SharedAccess"] = "supporting";
	smap["ShellHWDetection"] = "supporting";
	smap["Spooler"] = "supporting";
	smap["sppsvc"] = "supporting";
	smap["SSDPSRV"] = "supporting";
	smap["swprv"] = "supporting";
	smap["systemEventsBroker"] = "supporting";
	smap["Themes"] = "supporting";
	smap["VaultSvc"] = "supporting";
	smap["vds"] = "supporting";
	smap["vmicguestinterface"] = "supporting";
	smap["vmickvpexchange"] = "supporting";
	smap["vmicrdv"] = "supporting";
	smap["W32Time"] = "supporting";
	smap["Wcmsvc"] = "supporting";
	smap["WcsPlugInService"] = "supporting";
	smap["WdiServiceHost"] = "supporting";
	smap["WdisystemHost"] = "supporting";
	smap["WebClient"] = "supporting";
	smap["WinHttpAutoProxySvc"] = "supporting";
	smap["wlidsvc"] = "supporting";
	smap["wmiApSrv"] = "supporting";
	smap["WPDBusEnum"] = "supporting";
	smap["WSearch"] = "supporting";
	smap["WSService"] = "supporting";
	smap["wuauserv"] = "supporting";
	smap["wudfsvc"] = "supporting";
	smap["BFE"] = "system";
	smap["BrokerInfrastructure"] = "system";
	smap["DcomLaunch"] = "system";
	smap["LSM"] = "system";
	smap["Power"] = "system";
	smap["SamSs"] = "system";
	smap["Schedule"] = "system";
	smap["WinDefend"] = "system";
}


struct service_closer {
	static void close(SC_HANDLE handle) {
		CloseServiceHandle(handle);
	}
};
typedef hlp::handle<SC_HANDLE, service_closer> service_handle;
namespace services_helper {
	DWORD parse_service_type(const std::string str) {
		DWORD ret = 0;
		BOOST_FOREACH(const std::string key, str::utils::split_lst(str, std::string(","))) {
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
				throw nsclient::nsclient_exception("Invalid service type specified: " + key);
		}
		return ret;
	}
	DWORD parse_service_state(const std::string str) {
		DWORD ret = 0;
		BOOST_FOREACH(const std::string key, str::utils::split_lst(str, std::string(","))) {
			if (key == "active")
				ret |= SERVICE_ACTIVE;
			else if (key == "inactive")
				ret |= SERVICE_INACTIVE;
			else if (key == "all")
				ret |= SERVICE_STATE_ALL;
			else
				throw nsclient::nsclient_exception("Invalid service type specified: " + key);
		}
		return ret;
	}

	hlp::buffer<BYTE, QUERY_SERVICE_CONFIG*> queryServiceConfig(SC_HANDLE hService, std::string service) {
		DWORD bytesNeeded = 0;
		DWORD deErr = 0;

		if (QueryServiceConfig(hService, NULL, 0, &bytesNeeded) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			throw nsclient::nsclient_exception("Failed to query service: " + service + ": " + error::lookup::last_error(deErr));

		hlp::buffer<BYTE, QUERY_SERVICE_CONFIG*> buf(bytesNeeded + 10);
		if (!QueryServiceConfig(hService, buf.get(), bytesNeeded, &bytesNeeded))
			throw nsclient::nsclient_exception("Failed to query service: " + service + ": " + error::lookup::last_error());
		return buf;
	}

	hlp::buffer<BYTE, SERVICE_STATUS_PROCESS*> queryServiceStatusEx(SC_HANDLE hService, std::string service) {
		DWORD bytesNeeded = 0;
		DWORD deErr = 0;

		if (windows::winapi::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, NULL, 0, &bytesNeeded) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			throw nsclient::nsclient_exception("Failed to query service: " + service + ": " + error::lookup::last_error(deErr));

		hlp::buffer<BYTE, SERVICE_STATUS_PROCESS*> buf(bytesNeeded + 10);
		if (!windows::winapi::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, buf, bytesNeeded, &bytesNeeded))
			throw nsclient::nsclient_exception("Failed to query service: " + service + ": " + error::lookup::last_error());
		return buf;
	}

	void fetch_triggers(service_handle &hService, service_info &info) {
		DWORD bytesNeeded = 0;
		DWORD deErr = 0;
		if (QueryServiceConfig2W(hService, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &bytesNeeded) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			return;
		hlp::buffer<BYTE> buffer(bytesNeeded + 10);

		if (!QueryServiceConfig2W(hService, SERVICE_CONFIG_TRIGGER_INFO, buffer.get(), bytesNeeded, &bytesNeeded))
			throw nsclient::nsclient_exception("Failed to open service: " + info.name);

		info.triggers = buffer.get_t<SERVICE_TRIGGER_INFO*>()->cTriggers;
	}

	void fetch_delayed(service_handle &hService, service_info &info) {
		SERVICE_DELAYED_AUTO_START_INFO delayed;
		DWORD size = sizeof(SERVICE_DELAYED_AUTO_START_INFO);
		if (windows::winapi::QueryServiceConfig2W(hService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, reinterpret_cast<LPBYTE>(&delayed), size, &size)) {
			info.delayed = delayed.fDelayedAutostart?true:false;
		}
	}

	std::list<service_info> enum_services(const std::string computer, DWORD dwServiceType, DWORD dwServiceState) {
		std::list<service_info> ret;
		std::wstring comp = utf8::cvt<std::wstring>(computer);

		service_handle sc = OpenSCManager(comp.empty() ? NULL : comp.c_str(), NULL, SC_MANAGER_ENUMERATE_SERVICE);
		if (!sc)
			throw nsclient::nsclient_exception("Failed to open service manager: " + error::lookup::last_error());

		DWORD bytesNeeded = 0;
		DWORD count = 0;
		DWORD handle = 0;
		BOOL bRet = windows::winapi::EnumServicesStatusEx(sc, SC_ENUM_PROCESS_INFO, dwServiceType, dwServiceState, NULL, 0, &bytesNeeded, &count, &handle, NULL);
		if (bRet != 0) {
			int err = GetLastError();
			if (err != ERROR_MORE_DATA) {
				throw nsclient::nsclient_exception("Failed to enumerate service status: " + error::format::from_system(err));
			}
		}

		hlp::buffer<BYTE, ENUM_SERVICE_STATUS_PROCESS*> buf(bytesNeeded + 10);
		bRet = windows::winapi::EnumServicesStatusEx(sc, SC_ENUM_PROCESS_INFO, dwServiceType, dwServiceState, buf, bytesNeeded, &bytesNeeded, &count, &handle, NULL);
		if (!bRet)
			throw nsclient::nsclient_exception("Failed to enumerate service: " + error::lookup::last_error());
		ENUM_SERVICE_STATUS_PROCESS *data = buf.get();
		for (DWORD i = 0; i < count; ++i) {
			service_info info(utf8::cvt<std::string>(data[i].lpServiceName), utf8::cvt<std::string>(data[i].lpDisplayName));
			info.pid = data[i].ServiceStatusProcess.dwProcessId;
			info.state = data[i].ServiceStatusProcess.dwCurrentState;
			info.type = data[i].ServiceStatusProcess.dwServiceType;

			service_handle hService = OpenService(sc, data[i].lpServiceName, SERVICE_QUERY_CONFIG);
			if (!hService)
				throw nsclient::nsclient_exception("Failed to open service: " + info.name);

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

		service_handle sc = OpenSCManager(comp.empty() ? NULL : comp.c_str(), NULL, SC_MANAGER_ENUMERATE_SERVICE);
		if (!sc)
			throw nsclient::nsclient_exception("Failed to open service manager: " + error::lookup::last_error());

		service_handle hService = OpenService(sc, utf8::cvt<std::wstring>(service).c_str(), SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);
		if (!hService) {
			DWORD error = GetLastError();
			if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
				hlp::buffer<wchar_t> buf(2048);
				DWORD size = buf.size();
				if (!GetServiceKeyName(sc, utf8::cvt<std::wstring>(service).c_str(), buf.get(), &size)) {
					throw nsclient::nsclient_exception("Failed to open service " + service + ": " + error::lookup::last_error(error));
				}
				hService = OpenService(sc, buf.get(), SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);
				if (!hService)
					throw nsclient::nsclient_exception("Failed to open service " + service + ": " + error::lookup::last_error(error));
			} else
				throw nsclient::nsclient_exception("Failed to open service " + service + ": " + error::lookup::last_error(error));
		}

		hlp::buffer<BYTE, SERVICE_STATUS_PROCESS*> ssp = queryServiceStatusEx(hService, service);

		service_info info(service, "TODO");
		info.pid = ssp.get()->dwProcessId;
		info.state = ssp.get()->dwCurrentState;
		info.type = ssp.get()->dwServiceType;

		DWORD bytesNeeded2 = 0;
		DWORD deErr = 0;
		if (QueryServiceConfig(hService, NULL, 0, &bytesNeeded2) || (deErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
			throw nsclient::nsclient_exception("Failed to open service " + info.name + ": " + error::lookup::last_error(deErr));
		hlp::buffer<BYTE> buf2(bytesNeeded2 + 10);

		if (!QueryServiceConfig(hService, reinterpret_cast<QUERY_SERVICE_CONFIG*>(buf2.get()), bytesNeeded2, &bytesNeeded2))
			throw nsclient::nsclient_exception("Failed to open service: " + info.name);
		QUERY_SERVICE_CONFIG *data2 = reinterpret_cast<QUERY_SERVICE_CONFIG*>(buf2.get());
		info.start_type = data2->dwStartType;
		info.binary_path = utf8::cvt<std::string>(data2->lpBinaryPathName);
		info.error_control = data2->dwErrorControl;
		info.displayname = utf8::cvt<std::string>(data2->lpDisplayName);

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
			str::format::append_list(str, "system-driver");
		if (type&SERVICE_KERNEL_DRIVER)
			str::format::append_list(str, "kernel-driver");
		if (type&SERVICE_WIN32_OWN_PROCESS)
			str::format::append_list(str, "service-own-process");
		if (type&SERVICE_WIN32_SHARE_PROCESS)
			str::format::append_list(str, "service-shared-process");
		if (type&SERVICE_WIN32)
			str::format::append_list(str, "service");
		if (type&SERVICE_INTERACTIVE_PROCESS)
			str::format::append_list(str, "interactive");
		return str;
	}
}