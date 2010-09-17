#pragma once

#include <unicode_char.hpp>

#define DEFINE_SETTING_S(name, path, key, value) \
	const std::wstring name ## _PATH = _T(path); \
	const std::wstring name = _T(key); \
	const std::wstring name ## _DEFAULT = _T(value);
#define NSCLIENT_SETTINGS_SYSTRAY_EXE _T("systray_exe")
#define NSCLIENT_SETTINGS_SYSTRAY_EXE_DEFAULT _T("nstray.exe")

#define DEFINE_PATH(name, path) \
	const std::wstring name ## _PATH = _T(path);

#define DEFINE_SETTING_I(name, path, key, value) \
	const std::wstring name ## _PATH = _T(path); \
	const std::wstring name = _T(key); \
	const long long name ## _DEFAULT = value;

#define DEFINE_SETTING_B(name, path, key, value) \
	const std::wstring name ## _PATH = _T(path); \
	const std::wstring name = _T(key); \
	const bool name ## _DEFAULT = value;

#define DESCRIBE_SETTING(name, title, description) \
	const std::wstring name ## _TITLE = _T(title); \
	const std::wstring name ## _DESC = _T(description); \
	const bool name ## _ADVANCED = false;

#define DESCRIBE_SETTING_ADVANCED(name, title, description) \
	const std::wstring name ## _TITLE = _T(title); \
	const std::wstring name ## _DESC = _T(description); \
	const bool name ## _ADVANCED = true;

#define SETTINGS_KEY(key) \
	setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT

#define SETTINGS_REG_KEY_S_GEN(key, type) \
	setting_keys::key ## _PATH, setting_keys::key, type, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT, setting_keys::key ## _ADVANCED
#define SETTINGS_REG_KEY_I_GEN(key, type) \
	setting_keys::key ## _PATH, setting_keys::key, type, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, boost::lexical_cast<std::wstring>(setting_keys::key ## _DEFAULT), setting_keys::key ## _ADVANCED
#define SETTINGS_REG_KEY_B_GEN(key, type) \
	setting_keys::key ## _PATH, setting_keys::key, type, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT==1?_T("1"):_T("0"), setting_keys::key ## _ADVANCED
#define SETTINGS_REG_PATH_GEN(key) \
	setting_keys::key ## _PATH, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _ADVANCED




#define GENERIC_KEY_ALLOWED_HOSTS "allowed hosts"
#define GENERIC_KEY_BIND_TO "bind to"
#define GENERIC_KEY_SOCK_READ_TIMEOUT "socket read timeout"
#define GENERIC_KEY_SOCK_LISTENQUE "socket queue size"
#define GENERIC_KEY_SOCK_CACHE_ALLOWED "allowed hosts caching"
#define GENERIC_KEY_PWD_MASTER_KEY "master key"
#define GENERIC_KEY_PWD "password"
#define GENERIC_KEY_OBFUSCATED_PWD "obfuscated password"
#define GENERIC_KEY_USE_SSL "use ssl"


// Main Registry ROOT
#define NS_HKEY_ROOT HKEY_LOCAL_MACHINE
#define NS_REG_ROOT _T("SOFTWARE\\NSClient++")



namespace setting_keys {

#define DEFAULT_PROTOCOL_SECTION "/protocols/default"
#define NSCLIENT_SECTION "/protocols/NSClient"
#define NRPE_SECTION_PROTOCOL "/protocols/NRPE"
#define DEFAULT_SECTION "/settings"
#define NRPE_SECTION "/settings/NRPE"
#define NRPE_CLIENT_HANDLER_SECTION "/settings/NRPE/client/handlers"
#define NRPE_SECTION_HANDLERS "/settings/NRPE/Handlers"
#define MAIN_MODULES_SECTION _T("/modules")
#define EVENT_LOG_SECTION "/settings/eventlog"
#define EXTSCRIPT_SECTION "/settings/external scripts"
#define EXTSCRIPT_SCRIPT_SECTION "/settings/external scripts/scripts"
#define EXTSCRIPT_ALIAS_SECTION "/settings/external scripts/alias"
#define EXTSCRIPT_WRAPPINGS_SECTION "/settings/external scripts/wrappings"
#define EXTSCRIPT_WRAPPED_SCRIPT "/settings/external scripts/wrapped scripts"
#define CHECK_SYSTEM_SECTION "/settings/system"
#define CHECK_SYSTEM_COUNTERS_SECTION "/settings/system/PDH counters"
#define CHECK_SYSTEM_SERVICES_SECTION "/settings/system/services"
#define NSCA_SECTION "/settings/NSCA"
#define SCHEDULER_SECTION "/settings/scheduler"
#define SCHEDULER_SECTION_SCH "/settings/scheduler/schedules"
#define SCHEDULER_SECTION_FAKE "/settings/scheduler/schedules/<schedule name>"
#define SCHEDULER_SECTION_DEF "/settings/scheduler/default"
#define NSCA_SERVER_SECTION "/settings/NSCA/server"
#define NSCA_CMD_SECTION "/settings/NSCA/server/commands"
#define TASK_SCHED_SECTION "/settings/Task Scheduler"
#define LUA_SECTION "/settings/Lua/script"
#define LOG_SECTION "/settings/log"

#define CHECK_DISK_SECTION "/settings/check/disk"

	namespace settings_def {
		DEFINE_SETTING_I(PAYLOAD_LEN, DEFAULT_SECTION, "payload length", 4096);
		DESCRIBE_SETTING(PAYLOAD_LEN, "PAYLOAD LENGTH", "...");

		DEFINE_SETTING_B(SHARED_SESSION, DEFAULT_SECTION, "shared session", true);
		DESCRIBE_SETTING(SHARED_SESSION, "SHARED SESSION", "TODO");

		DEFINE_SETTING_S(SYSTRAY_EXE, DEFAULT_SECTION, "systray_exe", "nstray.exe");
		DESCRIBE_SETTING(SYSTRAY_EXE, "TODO", "TODO");
	}

	// NSClient Setting headlines
	namespace nsclient {
		DEFINE_PATH(SECTION, NSCLIENT_SECTION);
		DESCRIBE_SETTING(SECTION, "NSCLIENT SECTION", "Section for NSClient (NSClientListsner.dll) (check_nt) protocol options.");

		DEFINE_SETTING_S(ALLOWED_HOSTS, NSCLIENT_SECTION, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to NSClient deamon. If you leave this blank the global version will be used instead.");

		DEFINE_SETTING_I(PORT, NSCLIENT_SECTION, "port", 12489);
		DESCRIBE_SETTING(PORT, "NSCLIENT PORT NUMBER", "This is the port the NSClientListener.dll will listen to.");

		DEFINE_SETTING_S(BINDADDR, NSCLIENT_SECTION, GENERIC_KEY_BIND_TO, "");
		DESCRIBE_SETTING(BINDADDR, "BIND TO ADDRESS", "Allows you to bind server to a specific local address. This has to be a dotted ip adress not a hostname. Leaving this blank will bind to all avalible IP adresses.");

		DEFINE_SETTING_I(READ_TIMEOUT, NSCLIENT_SECTION, GENERIC_KEY_SOCK_READ_TIMEOUT, 30);
		DESCRIBE_SETTING(READ_TIMEOUT, "SOCKET TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out.");

		DEFINE_SETTING_I(LISTENQUE, NSCLIENT_SECTION, GENERIC_KEY_SOCK_LISTENQUE, 0);
		DESCRIBE_SETTING_ADVANCED(LISTENQUE, "LISTEN QUEUE", "Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.");

		DEFINE_SETTING_S(VERSION, NSCLIENT_SECTION, "version", "auto");
		DESCRIBE_SETTING(VERSION, "VERSION", "The version number to return for the CLIENTVERSION check (useful to \"simulate\" an old/different version of the client, auto will be generated from the compiled version string inside NSClient++");

		DEFINE_SETTING_B(CACHE_ALLOWED, NSCLIENT_SECTION, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		DEFINE_SETTING_S(MASTER_KEY, NSCLIENT_SECTION, GENERIC_KEY_PWD_MASTER_KEY, "This is a secret key that you should change");
		DESCRIBE_SETTING(MASTER_KEY, "MASTER KEY", "The secret \"key\" used when (de)obfuscating passwords.");

		DEFINE_SETTING_S(PWD, NSCLIENT_SECTION, GENERIC_KEY_PWD, "");
		DESCRIBE_SETTING(PWD, "PASSWORD", "This is the password (-s) that is required to access NSClient remotely. If you leave this blank everyone will be able to access the daemon remotly.");

		DEFINE_SETTING_S(OBFUSCATED_PWD, NSCLIENT_SECTION, GENERIC_KEY_OBFUSCATED_PWD, "");
		DESCRIBE_SETTING(OBFUSCATED_PWD, "OBFUSCATED PASSWORD", "This is the same as the password option but here you can store the password in an obfuscated manner. *NOTICE* obfuscation is *NOT* the same as encryption, someone with access to this file can still figure out the password. Its just a bit harder to do it at first glance.");

	}

	// NSClient Setting headlines
	namespace nrpe {
		DEFINE_PATH(SECTION, NRPE_SECTION_PROTOCOL);
		//DESCRIBE_SETTING(SECTION, "NRPE SECTION", "Section for NRPE (NRPEListener.dll) (check_nrpe) protocol options.");

		
		DEFINE_PATH(CH_SECTION, NRPE_CLIENT_HANDLER_SECTION);
		DESCRIBE_SETTING(CH_SECTION, "CLIENT HANDLER SECTION", "...");

		DEFINE_SETTING_S(ALLOWED_HOSTS, NRPE_SECTION_PROTOCOL, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to NSClient deamon. If you leave this blank the global version will be used instead.");

		DEFINE_SETTING_I(PORT, NRPE_SECTION_PROTOCOL, "port", 5666);
		//DESCRIBE_SETTING(PORT, "NSCLIENT PORT NUMBER", "This is the port the NSClientListener.dll will listen to.");

		DEFINE_SETTING_S(BINDADDR, NRPE_SECTION_PROTOCOL, GENERIC_KEY_BIND_TO, "");
		//DESCRIBE_SETTING(BINDADDR, "BIND TO ADDRESS", "Allows you to bind server to a specific local address. This has to be a dotted ip adress not a hostname. Leaving this blank will bind to all avalible IP adresses.");

		DEFINE_SETTING_I(READ_TIMEOUT, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_READ_TIMEOUT, 30);
		//DESCRIBE_SETTING(READ_TIMEOUT, "SOCKET TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out.");

		DEFINE_SETTING_I(LISTENQUE, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_LISTENQUE, 0);
		//DESCRIBE_SETTING_ADVANCED(LISTENQUE, "LISTEN QUEUE", "Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.");

		DEFINE_SETTING_I(THREAD_POOL, NRPE_SECTION_PROTOCOL, "thread pool", 10);
		//DESCRIBE_SETTING_ADVANCED(THREAD_POOL, "THREAD POOL", "");

		

		DEFINE_SETTING_B(CACHE_ALLOWED, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		DEFINE_SETTING_B(KEYUSE_SSL, NRPE_SECTION_PROTOCOL, GENERIC_KEY_USE_SSL, true);
		//DESCRIBE_SETTING(KEYUSE_SSL, "USE SSL SOCKET", "This option controls if SSL should be used on the socket.");

		DEFINE_SETTING_I(PAYLOAD_LENGTH, NRPE_SECTION_PROTOCOL, "payload length", 1024);
		//DESCRIBE_SETTING_ADVANCED(PAYLOAD_LENGTH, "PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.");

		DEFINE_SETTING_B(ALLOW_PERFDATA, NRPE_SECTION, "performance data", true);
		//DESCRIBE_SETTING_ADVANCED(ALLOW_PERFDATA, "PERFORMANCE DATA", "Send performance data back to nagios (set this to 0 to remove all performance data).");

		DEFINE_SETTING_I(CMD_TIMEOUT, NRPE_SECTION, "command timeout", 60);
		//DESCRIBE_SETTING(CMD_TIMEOUT, "COMMAND TIMEOUT", "This specifies the maximum number of seconds that the NRPE daemon will allow plug-ins to finish executing before killing them off.");

		DEFINE_SETTING_B(ALLOW_ARGS, NRPE_SECTION, "allow arguments", false);
		//DESCRIBE_SETTING(ALLOW_ARGS, "COMMAND ARGUMENT PROCESSING", "This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.");

		DEFINE_SETTING_B(ALLOW_NASTY, NRPE_SECTION, "allow nasy characters", false);
		//DESCRIBE_SETTING(ALLOW_NASTY, "COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the NRPE daemon will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.");

	}

	namespace protocol_def {
		DEFINE_SETTING_S(ALLOWED_HOSTS, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to the all daemons. If leave this blank anyone can access the deamon remotly (NSClient still requires a valid password). The syntax is host or ip/mask so 192.168.0.0/24 will allow anyone on that subnet access");

		DEFINE_SETTING_B(CACHE_ALLOWED, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		DEFINE_SETTING_S(MASTER_KEY, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_PWD_MASTER_KEY, "This is a secret key that you should change");
		DESCRIBE_SETTING(MASTER_KEY, "MASTER KEY", "The secret \"key\" used when (de)obfuscating passwords.");

		DEFINE_SETTING_S(PWD, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_PWD, "");
		DESCRIBE_SETTING(PWD, "PASSWORD", "This is the password (-s) that is required to access NSClient remotely. If you leave this blank everyone will be able to access the daemon remotly.");

		DEFINE_SETTING_S(OBFUSCATED_PWD, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_OBFUSCATED_PWD, "");
		DESCRIBE_SETTING(OBFUSCATED_PWD, "OBFUSCATED PASSWORD", "This is the same as the password option but here you can store the password in an obfuscated manner. *NOTICE* obfuscation is *NOT* the same as encryption, someone with access to this file can still figure out the password. Its just a bit harder to do it at first glance.");
	}
	namespace event_log {
		DEFINE_PATH(SECTION, EVENT_LOG_SECTION);
		//DESCRIBE_SETTING(SECTION, "EVENT LOG SECTION", "Section for the EventLog Checker (CHeckEventLog.dll).");

		DEFINE_SETTING_B(DEBUG_KEY, EVENT_LOG_SECTION, "debug", false);
		//DESCRIBE_SETTING_ADVANCED(DEBUG_KEY, "DEBUG", "Log all \"hits\" and \"misses\" on the eventlog filter chain, useful for debugging eventlog checks but very very very noisy so you don't want to accidentally set this on a real machine.");

		DEFINE_SETTING_B(LOOKUP_NAMES, EVENT_LOG_SECTION, "lookup_names", false);
		//DESCRIBE_SETTING_ADVANCED(LOOKUP_NAMES, "TODO", "TODO");

		DEFINE_SETTING_S(SYNTAX, EVENT_LOG_SECTION, "syntax", "");
		//DESCRIBE_SETTING(SYNTAX, "SYNTAX", "Set this to use a specific syntax string for all commands (that don't specify one).");

		DEFINE_SETTING_I(BUFFER_SIZE, EVENT_LOG_SECTION, "buffer_size", 65535);
		//DESCRIBE_SETTING(BUFFER_SIZE, "BUFFER SIZE", "The size of the bugfer to use when getting messages this affects the speed and maximum size of messages you can recieve.");
	}

	namespace external_scripts {
		DEFINE_PATH(SECTION, EXTSCRIPT_SECTION);
		//DESCRIBE_SETTING(SECTION, "EXTERNAL SCRIPT SECTION", "Section for external scripts (CheckExternalScripts.dll).");

		DEFINE_SETTING_I(TIMEOUT, EXTSCRIPT_SECTION, "timeout", 60);
		//DESCRIBE_SETTING(TIMEOUT, "COMMAND TIMEOUT", "The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones.");

		DEFINE_SETTING_B(ALLOW_ARGS, EXTSCRIPT_SECTION, "allow arguments", false);
		//DESCRIBE_SETTING(ALLOW_ARGS, "COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.");

		DEFINE_SETTING_B(ALLOW_NASTY, EXTSCRIPT_SECTION, "allow nasy characters", false);
		//DESCRIBE_SETTING(ALLOW_NASTY, "COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.");

		DEFINE_SETTING_S(SCRIPT_PATH, EXTSCRIPT_SECTION, "script path", "");
		//DESCRIBE_SETTING_ADVANCED(SCRIPT_PATH, "SCRIPT DIRECTORY", "Load all scripts in a directory and use them as commands. Probably dangerous but usefull if you have loads of scripts :)");

		DEFINE_PATH(SCRIPT_SECTION, EXTSCRIPT_SCRIPT_SECTION);
		//DESCRIBE_SETTING(SCRIPT_SECTION, "EXTERNAL SCRIPT SCRIPTS SECTION", "A list of scripts available to run from the CheckExternalScripts module. Syntax is: <command>=<script> <arguments> for instance:");

		DEFINE_PATH(ALIAS_SECTION, EXTSCRIPT_ALIAS_SECTION);
		//DESCRIBE_SETTING(ALIAS_SECTION, "EXTERNAL SCRIPT ALIAS SECTION", "Works like the \"inject\" concept of NRPE scripts module. But in short a list of aliases available. An alias is an internal command that has been \"wrapped\" (to add arguments). Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)");

		DEFINE_PATH(WRAPPINGS_SECTION, EXTSCRIPT_WRAPPINGS_SECTION);
		//DESCRIBE_SETTING(WRAPPINGS_SECTION, "EXTERNAL SCRIPT WRAPPINGS SECTION", "");

		DEFINE_PATH(WRAPPED_SCRIPT, EXTSCRIPT_WRAPPED_SCRIPT);
		//DESCRIBE_SETTING(WRAPPED_SCRIPT, "EXTERNAL SCRIPT WRAPPINGS SECTION", "");

	}

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

	namespace nsca {
		DEFINE_PATH(SECTION, NSCA_SECTION);
		//DESCRIBE_SETTING(SECTION, "NSCA SECTION", "Section for NSCA passive check module.");

		DEFINE_SETTING_I(INTERVAL, NSCA_SECTION, "interval", 60);
		//DESCRIBE_SETTING(INTERVAL, "COMMAND TIMEOUT", "Time in seconds between each report back to the server (cant as of yet be set individually so this is for all \"checks\")");

		DEFINE_SETTING_S(HOSTNAME, NSCA_SECTION, "hostname", "");
		//DESCRIBE_SETTING_ADVANCED(HOSTNAME, "LOCAL HOSTNAME", "The host name of this host if set to blank (default) the windows name of the computer will be used.");

		DEFINE_PATH(SERVER_SECTION, NSCA_SERVER_SECTION);
		//DESCRIBE_SETTING(SERVER_SECTION, "NSCA SERVER SECTION", "Configure the NSCA server to report to");

		DEFINE_SETTING_S(SERVER_HOST, NSCA_SERVER_SECTION, "host", "unknown-host");
		//DESCRIBE_SETTING(SERVER_HOST, "NSCA SERVER", "The NSCA/Nagios(?) server to report results to.");

		DEFINE_SETTING_I(SERVER_PORT, NSCA_SERVER_SECTION, "port", 5667);
		//DESCRIBE_SETTING(SERVER_PORT, "NSCA PORT", "The NSCA server port");

		DEFINE_SETTING_I(ENCRYPTION, NSCA_SERVER_SECTION, "encryption method", 1);
		//DESCRIBE_SETTING(ENCRYPTION, "NSCA ENCRYPTION", "Number corresponding to the various encryption algorithms (see the wiki). Has to be the same as the server or it wont work at all.");

		DEFINE_SETTING_S(PASSWORD, NSCA_SERVER_SECTION, "password", "");
		//DESCRIBE_SETTING(PASSWORD, "NSCA PASSWORD", "The password to use. Again has to be the same as the server or it wont work at all.");

		DEFINE_SETTING_I(THREADS, NSCA_SECTION, "debug threads", 1);
		//DESCRIBE_SETTING_ADVANCED(THREADS, "DEBUG OPTION (THREADS)", "Number of threads to run, no reason to change this really (unless you want to stress test something)");

		DEFINE_SETTING_B(CACHE_HOST, NSCA_SECTION, "cache hostname", false);
		//DESCRIBE_SETTING_ADVANCED(CACHE_HOST, "CACHE HOSTNAME", "???");

		DEFINE_PATH(CMD_SECTION, NSCA_CMD_SECTION);
		//DESCRIBE_SETTING(CMD_SECTION, "NSCA COMMAND SECTION", "Commands to check and report to the NSCA server");

		DEFINE_SETTING_S(REPORT_MODE, NSCA_SERVER_SECTION, "report", "all");
		//DESCRIBE_SETTING(REPORT_MODE, "REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)");

		DEFINE_SETTING_S(TIME_DELTA_DEFAULT, NSCA_SECTION, "delay", "0");
		//DESCRIBE_SETTING(TIME_DELTA_DEFAULT, "TODO", "TODO");

		DEFINE_SETTING_I(PAYLOAD_LENGTH, NSCA_SECTION, "payload length", 512);
		//DESCRIBE_SETTING_ADVANCED(PAYLOAD_LENGTH, "PAYLOAD LENGTH", "Length of payload to/from the NSCA agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCA server to use the same value for it to work.");

		DEFINE_SETTING_I(READ_TIMEOUT, NSCA_SERVER_SECTION, GENERIC_KEY_SOCK_READ_TIMEOUT, 30);
		//DESCRIBE_SETTING(READ_TIMEOUT, "SOCKET TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out.");

	}

	namespace scheduler {
		DEFINE_PATH(SECTION, SCHEDULER_SECTION);
		DESCRIBE_SETTING(SECTION, "SCHEDULER SECTION", "Section for the Scheduler module.");

		DEFINE_PATH(SCHEDULES_SECTION, SCHEDULER_SECTION_SCH);
		DESCRIBE_SETTING(SCHEDULES_SECTION, "SCHEDULES SECTION", "Section for defining schedules for the Scheduler module.");

		DEFINE_PATH(DEFAULT_SCHEDULE_SECTION, SCHEDULER_SECTION_DEF);
		DESCRIBE_SETTING(DEFAULT_SCHEDULE_SECTION, "DEFAULT SCHEDULER SECTION", "Default settings for all scheduled commands");

		DEFINE_SETTING_I(THREADS, SCHEDULER_SECTION, "debug threads", 1);
		DESCRIBE_SETTING_ADVANCED(THREADS, "THREADS", "Number of threads to use int he thread pool (increase if you have many scheduled items)");

		DEFINE_SETTING_S(INTERVAL, SCHEDULER_SECTION_FAKE, "interval", "5m");
		DESCRIBE_SETTING(INTERVAL, "SCHEDULE INTERVAL", "Time in seconds between each check");

		DEFINE_SETTING_S(COMMAND, SCHEDULER_SECTION_FAKE, "command", "check_ok");
		DESCRIBE_SETTING(COMMAND, "SCHEDULE COMMAND", "Command to run");

		DEFINE_SETTING_S(CHANNEL, SCHEDULER_SECTION_FAKE, "channel", "NSCA");
		DESCRIBE_SETTING(CHANNEL, "SCHEDULE CHANNEL", "Channel to send results on");

		DEFINE_SETTING_S(REPORT_MODE, SCHEDULER_SECTION_FAKE, "report", "all");
		DESCRIBE_SETTING(REPORT_MODE, "REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)");

		DEFINE_SETTING_S(INTERVAL_D, SCHEDULER_SECTION_DEF, "interval", "5m");
		DESCRIBE_SETTING(INTERVAL_D, "SCHEDULE INTERVAL", "Time in seconds between each check");

		DEFINE_SETTING_S(COMMAND_D, SCHEDULER_SECTION_DEF, "command", "check_ok");
		DESCRIBE_SETTING(COMMAND_D, "SCHEDULE COMMAND", "Command to run");

		DEFINE_SETTING_S(CHANNEL_D, SCHEDULER_SECTION_DEF, "channel", "NSCA");
		DESCRIBE_SETTING(CHANNEL_D, "SCHEDULE CHANNEL", "Channel to send results on");

		DEFINE_SETTING_S(REPORT_MODE_D, SCHEDULER_SECTION_DEF, "report", "all");
		DESCRIBE_SETTING(REPORT_MODE_D, "REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)");

	}

	namespace task_scheduler {
		DEFINE_PATH(SECTION, TASK_SCHED_SECTION);
		DESCRIBE_SETTING_ADVANCED(SECTION, "TASK SCHEDULER", "???");

		DEFINE_SETTING_S(SYNTAX, TASK_SCHED_SECTION, "syntax", "%title% last run: %most-recent-run-time% (%exit-code%)");
		DESCRIBE_SETTING_ADVANCED(SYNTAX, "SYNTAX", "Set this to use a specific syntax string for all commands (that don't specify one)");
	}

	namespace lua {
		DEFINE_PATH(SECTION, LUA_SECTION);
		DESCRIBE_SETTING_ADVANCED(SECTION, "LUA SECTION", "A list of LUA script to load at startup. In difference to \"external checks\" all LUA scripts are loaded at startup. Names have no meaning since the script (on boot) submit which commands are available and tie that to various functions.");
	}

	namespace log {
		DEFINE_PATH(SECTION, LOG_SECTION);
		//DESCRIBE_SETTING_ADVANCED(SECTION, "LOG SECTION", "Configure loggning properties.");

		DEFINE_SETTING_S(FILENAME, LOG_SECTION, "file", "nsclient.log");
		//DESCRIBE_SETTING_ADVANCED(FILENAME, "SYNTAX", "The file to write log data to. If no directory is used this is relative to the NSClient++ binary.");

		DEFINE_SETTING_S(ROOT, LOG_SECTION, "root", "auto");
		//DESCRIBE_SETTING_ADVANCED(ROOT, "TODO", "TODO");

		DEFINE_SETTING_S(DATEMASK, LOG_SECTION, "date format", "%Y-%m-%d %H:%M:%S");
		//DESCRIBE_SETTING_ADVANCED(DATEMASK, "DATEMASK", "The date format used when logging to a file.");

		DEFINE_SETTING_S(LOG_MASK, LOG_SECTION, "log mask", "normal");
		//DESCRIBE_SETTING_ADVANCED(LOG_MASK, "LOG MASK", "The log mask information, error, warning, critical, debug");

		DEFINE_SETTING_B(DEBUG_LOG, LOG_SECTION, "debug", false);
		//DESCRIBE_SETTING_ADVANCED(DEBUG_LOG, "DEBUG LOGGING", "Enable debug logging can help track down errors and find problems but will impact overall perfoamnce negativly.");
	}
}
