#pragma once


#include <settings/macros.h>

namespace setting_keys {
	namespace check_system {

		DEFINE_PATH(SECTION, CHECK_SYSTEM_SECTION);
		DESCRIBE_SETTING(SECTION, "SYSTEM", "Section for system checks and system settings.");

		DEFINE_SETTING_S(PROC_ENUM, CHECK_SYSTEM_SECTION, "process enumeration method", "auto");
		DESCRIBE_SETTING_ADVANCED(PROC_ENUM, "COMMAND TIMEOUT", "Set the PROCESS enumeration method (auto, TOOLHELP or PSAPI)");
		const std::wstring PROC_ENUM_PSAPI = _T("PSAPI");
		const std::wstring PROC_ENUM_TH = _T("TOOLHELP");

		DEFINE_SETTING_S(CPU_METHOD, CHECK_SYSTEM_SECTION, "method", "PDH_AUTO");
		DESCRIBE_SETTING_ADVANCED(CPU_METHOD, "CPU METHOD", "Set this to false to disable auto detect (counters.defs) PDH language and OS version.");
		const std::wstring CPU_METHOD_PDH_MANUAL = _T("PDH_MANUAL");
		const std::wstring CPU_METHOD_PDH_AUTO = _T("PDH_AUTO");
		const std::wstring CPU_METHOD_PDH_NO_INDEX = _T("PDH_NO_INDEX");

		DEFINE_SETTING_S(BUFFER_SIZE, CHECK_SYSTEM_SECTION, "buffer size", "1h");
		DESCRIBE_SETTING(BUFFER_SIZE, "BUFFER SIZE", "	The time to store CPU load data.");

		DEFINE_SETTING_I(INTERVALL, CHECK_SYSTEM_SECTION, "interval", 10);
		DESCRIBE_SETTING_ADVANCED(INTERVALL, "CHECK INTERVAL", "Time between checks in 1/10 of seconds.");

		DEFINE_SETTING_S(FORCE_LANGUAGE, CHECK_SYSTEM_SECTION, "locale", "auto");
		DESCRIBE_SETTING_ADVANCED(FORCE_LANGUAGE, "FORCE LOCALE", "Set this to a locale ID if you want to force auto-detection of counters from that locale.");

		DEFINE_PATH(COUNTERS_SECTION, CHECK_SYSTEM_COUNTERS_SECTION);
		DESCRIBE_SETTING_ADVANCED(COUNTERS_SECTION, "PDH COUNTERS", "Section to define PDH counters to use");

		DEFINE_SETTING_S(PDH_MEM_CMT_LIM, CHECK_SYSTEM_COUNTERS_SECTION, "memory commit limit", "\\Memory\\Commit Limit");
		DESCRIBE_SETTING_ADVANCED(PDH_MEM_CMT_LIM, "PDH COUNTER", "Counter to use to check upper memory limit.");

		DEFINE_SETTING_S(PDH_MEM_CMT_BYT, CHECK_SYSTEM_COUNTERS_SECTION, "memory commit byte", "\\Memory\\Committed Bytes");
		DESCRIBE_SETTING_ADVANCED(PDH_MEM_CMT_BYT, "PDH COUNTER", "Counter to use to check current memory usage.");

		DEFINE_SETTING_S(PDH_SYSUP, CHECK_SYSTEM_COUNTERS_SECTION, "system uptime", "\\System\\System Up Time");
		DESCRIBE_SETTING_ADVANCED(PDH_SYSUP, "PDH COUNTER", "Counter to use to check the uptime of the system.");

		DEFINE_SETTING_S(PDH_CPU, CHECK_SYSTEM_COUNTERS_SECTION, "processor time", "\\Processor(_total)\\% Processor Time");
		DESCRIBE_SETTING_ADVANCED(PDH_CPU, "PDH COUNTER", "Counter to use for CPU load.");

		//DEFINE_PATH(SERVICES_SECTION, CHECK_SYSTEM_SERVICES_SECTION);
		//DESCRIBE_SETTING_ADVANCED(SERVICES_SECTION, "SERVICE CHECKS", "Section to define service checks to use");

		//DEFINE_SETTING_S(SVC_BOOT_START, CHECK_SYSTEM_SERVICES_SECTION, "SERVICE_BOOT_START", "ignored");
		//DESCRIBE_SETTING_ADVANCED(SVC_BOOT_START, "SERVICE_BOOT_START SERVICE CHECK", "Set how to handle services set to SERVICE_BOOT_START state when checking all services");

		//DEFINE_SETTING_S(SVC_SYSTEM_START, CHECK_SYSTEM_SERVICES_SECTION, "SERVICE_SYSTEM_START", "ignored");
		//DESCRIBE_SETTING_ADVANCED(SVC_SYSTEM_START, "SERVICE_BOOT_START SERVICE CHECK", "Set how to handle services set to SERVICE_BOOT_START state when checking all services");

		//DEFINE_SETTING_S(SVC_AUTO_START, CHECK_SYSTEM_SERVICES_SECTION, "SERVICE_AUTO_START", "started");
		//DESCRIBE_SETTING_ADVANCED(SVC_AUTO_START, "SERVICE_BOOT_START SERVICE CHECK", "Set how to handle services set to SERVICE_BOOT_START state when checking all services");

		//DEFINE_SETTING_S(SVC_DEMAND_START, CHECK_SYSTEM_SERVICES_SECTION, "SERVICE_DEMAND_START", "ignored");
		//DESCRIBE_SETTING_ADVANCED(SVC_DEMAND_START, "SERVICE_BOOT_START SERVICE CHECK", "Set how to handle services set to SERVICE_BOOT_START state when checking all services");

		//DEFINE_SETTING_S(SVC_DISABLED, CHECK_SYSTEM_SERVICES_SECTION, "SERVICE_DISABLED", "stopped");
		//DESCRIBE_SETTING_ADVANCED(SVC_DISABLED, "SERVICE_BOOT_START SERVICE CHECK", "Set how to handle services set to SERVICE_BOOT_START state when checking all services");

		DEFINE_SETTING_S(PDH_SUBSYSTEM, CHECK_SYSTEM_SECTION, "pdh_subsystem", "fast");
		DESCRIBE_SETTING_ADVANCED(PDH_SUBSYSTEM, "PDH_SUBSYSTEM", "TODO");
		const std::wstring PDH_SUBSYSTEM_FAST = _T("fast");
		const std::wstring PDH_SUBSYSTEM_THREAD_SAFE = _T("thread-safe");

	}
}