//#define _WIN32_WINNT 0x0500

#include "installer_helper.hpp"

#include "../settings_manager/settings_manager_impl.h"
#include <config.h>

#include <file_helpers.hpp>

#include <str/wstring.hpp>
#include <str/xtos.hpp>
#include <str/utils.hpp>

#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/base_logger_impl.hpp>

#include <error/error.hpp>
#include <config.h>

#include <boost/algorithm/string.hpp>

#include <windows.h>
#include <Sddl.h>
#include <msi.h>
#include <msiquery.h>

#include <string>


const UINT COST_SERVICE_INSTALL = 2000;

bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description, std::wstring service_deps);
bool uninstall(msi_helper &h, std::wstring service_name);


void copy_file(msi_helper &h, std::wstring source, std::wstring target) {
	if (boost::filesystem::is_regular(utf8::cvt<std::string>(source))) {
		h.logMessage(L"Copying: " + source + L" to " + target);
		if (!CopyFile(source.c_str(), target.c_str(), FALSE)) {
			h.errorMessage(L"Failed to copy file: " + utf8::cvt<std::wstring>(error::lookup::last_error()));
		}
	} else {
		h.logMessage(L"Copying failed: " + source + L" to " + target + L" source was not found.");
	}

}


class msi_logger : public nsclient::logging::logger_impl {
	std::wstring error;
	std::list<std::wstring> log_;
	msi_helper *h;

public:
	msi_logger(msi_helper *h) : h(h) {
		set_log_level("trace");
	}


	void do_log(const std::string data) {
		std::wstring str = utf8::cvt<std::wstring>(data);
		if (str.empty())
			return;
		if (boost::algorithm::starts_with(str, L"error:")) {
			if (!error.empty())
				error += L"\n";
			error += str.substr(6);
		}
		log_.push_back(str);
		h->logMessage(str);
	}
	void asynch_configure() {}
	void synch_configure() {}
	bool startup() { return true; }
	bool shutdown() { return true; }

	std::wstring get_error() {
		return error;
	}
	bool has_errors() {
		return !error.empty();
	}
	std::list<std::wstring> get_errors() {
		return log_;
	}



	void nsclient::logging::logger::add_subscriber(nsclient::logging::logging_subscriber_instance) {}
	void nsclient::logging::logger::clear_subscribers(void) {}
	void nsclient::logging::logger::destroy(void) {

	}
	void nsclient::logging::logger::configure(void) {

	}
	void nsclient::logging::logger::set_backend(std::string) {

	}

};


void nsclient::logging::log_message_factory::log_fatal(std::string message) {
	std::cout << message << "\n";
}

std::string nsclient::logging::log_message_factory::create_critical(const std::string &module, const char* file, const int line, const std::string &message) {
	return "critical: " + message;
}
std::string nsclient::logging::log_message_factory::create_error(const std::string &module, const char* file, const int line, const std::string &message) {
	return "error: " + message;
}
std::string nsclient::logging::log_message_factory::create_warning(const std::string &module, const char* file, const int line, const std::string &message) {
	return "warning: " + message;
}
std::string nsclient::logging::log_message_factory::create_info(const std::string &module, const char* file, const int line, const std::string &message) {
	return "info: " + message;
}
std::string nsclient::logging::log_message_factory::create_debug(const std::string &module, const char* file, const int line, const std::string &message) {
	return "debug: " + message;
}
std::string nsclient::logging::log_message_factory::create_trace(const std::string &module, const char* file, const int line, const std::string &message) {
	return "trace: " + message;
}

struct installer_settings_provider : public settings_manager::provider_interface {

	msi_helper *h;
	std::string basepath;
	std::string old_settings_map;
	boost::shared_ptr<msi_logger> logger;


	installer_settings_provider(msi_helper *h, std::wstring basepath, std::wstring old_settings_map) 
		: h(h)
		, basepath(utf8::cvt<std::string>(basepath))
		, old_settings_map(utf8::cvt<std::string>(old_settings_map))
		, logger(new msi_logger(h))
	{
	}
	installer_settings_provider(msi_helper *h, std::wstring basepath) 
		: h(h)
		, basepath(utf8::cvt<std::string>(basepath)) 
		, old_settings_map(utf8::cvt<std::string>(old_settings_map))
		, logger(new msi_logger(h))
	{
	}

	virtual std::string expand_path(std::string file) {
		str::utils::replace(file, "${base-path}", basepath);
		str::utils::replace(file, "${exe-path}", basepath);
		str::utils::replace(file, "${shared-path}", basepath);
		return file;
	}
	std::string get_data(std::string key) {
		if (!old_settings_map.empty() && key == "old_settings_map_data") {
			return old_settings_map;
		}
		return "";
	}

	std::wstring get_error() {
		return logger->get_error();
	}
	bool has_errors() {
		return logger->has_errors();
	}
	std::list<std::wstring> get_errors() {
		return logger->get_errors();
	}


	nsclient::logging::logger_instance get_logger() const {
		return logger;
	}
};

static const wchar_t alphanum[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
std::wstring genpwd(const int len) {
	srand((unsigned)time(NULL ));
	std::wstring ret;
	for(int i=0; i < len; i++)
		ret += alphanum[rand() %  ((sizeof(alphanum)/sizeof(wchar_t))-1)];
	return ret;
}

bool has_mod(std::string key) {
	std::string val = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, key, "0");
	return val == "enabled" || val == "1";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Many options:
// - configuration not allowed		=> CONF_CAN_CHANGE=0, CONF_HAS_ERRORS=0
// - target not found				=> CONF_CAN_CHANGE=1, CONF_HAS_ERRORS=0
// - target found + config read		=> CONF_CAN_CHANGE=1, CONF_HAS_ERRORS=0
// - target found + config NOT read => CONF_CAN_CHANGE=0, CONF_HAS_ERRORS=1
//
// Interpretation:
// CONF_HAS_ERRORS=1	=> Dont allow anything (inform of issue)
// CONF_CAN_CHANGE=1	=> Allow change


std::wstring read_map_data(msi_helper &h) {
	std::wstring ret;
	PMSIHANDLE hView = h.open_execute_view(L"SELECT Data FROM Binary WHERE Name='OldSettingsMap'");
	if (h.isNull(hView)) {
		h.logMessage(L"Failed to query service view!");
		return L" ";
	}

	PMSIHANDLE hRec = h.fetch_record(hView);
	if (hRec != NULL) {
		ret = h.get_record_blob(hRec, 1);
		::MsiCloseHandle(hRec);
	}
	::MsiCloseHandle(hView);
	return ret;
}


#define ALLOWED_HOSTS L"ALLOWED_HOSTS"
#define NSCLIENT_PWD L"NSCLIENT_PWD"
#define NSCLIENT_PWD_DEFAULT L"NSCLIENT_PWD_DEFAULT"
#define CONF_SCHEDULER L"CONF_SCHEDULER"
#define CONF_CHECKS L"CONF_CHECKS"
#define CONF_NRPE L"CONF_NRPE"
#define CONF_NSCA L"CONF_NSCA"
#define CONF_WEB L"CONF_WEB"
#define CONF_NSCLIENT L"CONF_NSCLIENT"
#define NRPEMODE L"NRPEMODE"
#define MONITORING_TOOL L"MONITORING_TOOL"
#define MONITORING_TOOL_OP5 L"OP5"

#define OP5_SERVER L"OP5_SERVER"
#define OP5_USER L"OP5_USER"
#define OP5_PASSWORD L"OP5_PASSWORD"
#define OP5_HOSTGROUPS L"OP5_HOSTGROUPS"
#define OP5_CONTACTGROUP L"OP5_CONTACTGROUP"

#define CONFIGURATION_TYPE L"CONFIGURATION_TYPE"
#define CONF_CAN_CHANGE L"CONF_CAN_CHANGE"
#define CONF_INCLUDES L"CONF_INCLUDES"
#define INSTALL_SAMPLE_CONFIG L"INSTALL_SAMPLE_CONFIG"
#define GENERATE_SAMPLE_CONFIG L"GENERATE_SAMPLE_CONFIG"
#define CONF_OLD_ERROR L"CONF_OLD_ERROR"
#define CONF_HAS_ERRORS L"CONF_HAS_ERRORS"


#define KEY L"KEY_"

#define KEY_ALLOWED_HOSTS KEY ALLOWED_HOSTS
#define KEY_NSCLIENT_PWD KEY NSCLIENT_PWD
#define KEY_NSCLIENT_PWD_DEFAULT KEY NSCLIENT_PWD_DEFAULT
#define KEY_CONF_SCHEDULER KEY CONF_SCHEDULER
#define KEY_CONF_CHECKS KEY CONF_CHECKS
#define KEY_CONF_NRPE KEY CONF_NRPE
#define KEY_CONF_NSCA KEY CONF_NSCA
#define KEY_CONF_WEB KEY CONF_WEB
#define KEY_CONF_NSCLIENT KEY CONF_NSCLIENT
#define KEY_NRPEMODE KEY NRPEMODE

#define KEY_CONFIGURATION_TYPE KEY CONFIGURATION_TYPE
#define KEY_CONF_INCLUDES KEY CONF_INCLUDES
#define KEY_INSTALL_SAMPLE_CONFIG KEY INSTALL_SAMPLE_CONFIG
#define KEY_GENERATE_SAMPLE_CONFIG KEY GENERATE_SAMPLE_CONFIG

#define KEY_CONF_CAN_CHANGE KEY CONF_CAN_CHANGE
#define KEY_CONF_OLD_ERROR KEY CONF_OLD_ERROR
#define KEY_CONF_HAS_ERRORS KEY CONF_HAS_ERRORS


void dump_config(msi_helper &h, std::wstring title) {
		h.dumpReason(title);
		h.dumpProperty(KEY_ALLOWED_HOSTS);
		h.dumpProperty(KEY_NSCLIENT_PWD);
		h.dumpProperty(KEY_NSCLIENT_PWD_DEFAULT);
		h.dumpProperty(KEY_CONF_SCHEDULER);
		h.dumpProperty(KEY_CONF_CHECKS);
		h.dumpProperty(KEY_CONF_NRPE);
		h.dumpProperty(KEY_CONF_NSCA);
		h.dumpProperty(KEY_CONF_WEB);
		h.dumpProperty(KEY_CONF_NSCLIENT);
		h.dumpProperty(KEY_NRPEMODE);

		h.dumpProperty(KEY_CONFIGURATION_TYPE);
		h.dumpProperty(KEY_CONF_INCLUDES);
		h.dumpProperty(KEY_INSTALL_SAMPLE_CONFIG);
		h.dumpProperty(KEY_GENERATE_SAMPLE_CONFIG);

		h.dumpProperty(KEY_CONF_CAN_CHANGE);
}


extern "C" UINT __stdcall DetectTool(MSIHANDLE hInstall) {
	msi_helper h(hInstall, L"DetectTool");

	try {
		h.logMessage("Detecting monitoring tool config");
		if (!boost::algorithm::trim_copy(h.getPropery(OP5_SERVER)).empty()) {
			h.setProperty(MONITORING_TOOL, MONITORING_TOOL_OP5);
		}
		std::wstring tool = h.getPropery(MONITORING_TOOL);
		h.logMessage(L"Detected monitoring tool is: " + tool);
		dump_config(h, L"After DetectTool");
	} catch (installer_exception e) {
		h.logMessage(L"Failed to apply monitoring tool: " + e.what());
		return ERROR_SUCCESS;
	} catch (...) {
		h.logMessage(L"Failed to apply monitoring tool: Unknown exception");
		return ERROR_SUCCESS;
	}
	return ERROR_SUCCESS;
}
	
extern "C" UINT __stdcall ApplyTool(MSIHANDLE hInstall) {
	msi_helper h(hInstall, L"ApplyTool");
	try {
		h.logMessage("Applying monitoring tool config");
		std::wstring tool = h.getPropery(MONITORING_TOOL);
		h.logMessage(L"Monitoring tool is: " + tool);

		if (tool == MONITORING_TOOL_OP5) {
			h.setPropertyAndDefault(KEY_NSCLIENT_PWD, L"");
			h.setProperty(KEY_NSCLIENT_PWD_DEFAULT, L"");
			h.setPropertyAndDefault(KEY_CONF_CHECKS, L"1");
			h.setPropertyAndDefault(KEY_CONF_NRPE, L"1");
			h.setPropertyAndDefault(KEY_CONF_NSCA, L"1");
			h.setPropertyAndDefault(KEY_CONF_WEB, L"");
			h.setPropertyAndDefault(KEY_CONF_NSCLIENT, L"1");
			h.setPropertyAndDefault(KEY_NRPEMODE, L"LEGACY");

			h.setProperty(KEY_CONF_CAN_CHANGE, L"1");
			h.setProperty(KEY_CONF_INCLUDES, L"op5;op5.ini");
			h.setProperty(KEY_INSTALL_SAMPLE_CONFIG, L"");
			h.setProperty(KEY_GENERATE_SAMPLE_CONFIG, L"");
			h.setProperty(KEY_CONFIGURATION_TYPE, L"registry://HKEY_LOCAL_MACHINE/software/NSClient++");
			h.setFeatureLocal(L"OP5Montoring");
		} else if (tool == L"GENERIC") {
			h.setPropertyAndDefault(KEY_ALLOWED_HOSTS, L"127.0.0.1", L"");

			h.setPropertyAndDefault(KEY_NSCLIENT_PWD, genpwd(16), L"");
			h.setPropertyAndDefault(KEY_CONF_CHECKS, L"1", L"");
			h.setPropertyAndDefault(KEY_CONF_NRPE, L"1", L"");
			h.setPropertyAndDefault(KEY_CONF_NSCA, L"");
			h.setPropertyAndDefault(KEY_CONF_WEB, L"");
			h.setPropertyAndDefault(KEY_CONF_NSCLIENT, L"");
			h.setPropertyAndDefault(KEY_NRPEMODE, L"SAFE", L"");

			h.setProperty(KEY_CONF_CAN_CHANGE, L"1");
			h.setProperty(KEY_CONF_INCLUDES, L"");
			h.setProperty(KEY_INSTALL_SAMPLE_CONFIG, L"");
			h.setProperty(KEY_GENERATE_SAMPLE_CONFIG, L"");
			h.setProperty(KEY_CONFIGURATION_TYPE, L"ini://${shared-path}/nsclient.ini");
			h.setFeatureAbsent(L"OP5Montoring");
		}

		h.applyProperty(KEY_ALLOWED_HOSTS, ALLOWED_HOSTS);
		h.applyProperty(KEY_NSCLIENT_PWD, NSCLIENT_PWD);
		h.applyProperty(KEY_NSCLIENT_PWD_DEFAULT, NSCLIENT_PWD_DEFAULT);
		h.applyProperty(KEY_CONF_SCHEDULER, CONF_SCHEDULER);
		h.applyProperty(KEY_CONF_CHECKS, CONF_CHECKS);
		h.applyProperty(KEY_CONF_NRPE, CONF_NRPE);
		h.applyProperty(KEY_CONF_NSCA, CONF_NSCA);
		h.applyProperty(KEY_CONF_WEB, CONF_WEB);
		h.applyProperty(KEY_CONF_NSCLIENT, CONF_NSCLIENT);
		h.applyProperty(KEY_NRPEMODE, NRPEMODE);

		h.applyProperty(KEY_CONFIGURATION_TYPE, CONFIGURATION_TYPE);
		h.applyProperty(KEY_CONF_INCLUDES, CONF_INCLUDES);
		h.applyProperty(KEY_INSTALL_SAMPLE_CONFIG, INSTALL_SAMPLE_CONFIG);
		h.applyProperty(KEY_GENERATE_SAMPLE_CONFIG, GENERATE_SAMPLE_CONFIG);

		h.setPropertyIfEmpty(KEY_CONF_CAN_CHANGE, L"1");
		h.setPropertyIfEmpty(KEY_CONFIGURATION_TYPE, L"ini://${shared-path}/nsclient.ini");

		dump_config(h, L"After ApplyConfig");

	} catch (installer_exception e) {
		h.logMessage(L"Failed to apply monitoring tool: " + e.what());
		return ERROR_SUCCESS;
	} catch (...) {
		h.logMessage(L"Failed to apply monitoring tool: Unknown exception");
		return ERROR_SUCCESS;
	}
	return ERROR_SUCCESS;
}



extern "C" UINT __stdcall ImportConfig(MSIHANDLE hInstall) {
	msi_helper h(hInstall, L"ImportConfig");
	try {
		h.logMessage("importing config");
		std::wstring target = h.getTargetPath(L"INSTALLLOCATION");
		std::wstring allow = h.getPropery(L"ALLOW_CONFIGURATION");

		std::wstring map_data = read_map_data(h);
		if (allow == L"0") {
			h.logMessage(L"Configuration not allowed: " + allow);
			h.setProperty(KEY_CONF_CAN_CHANGE, L"0");
			h.setProperty(KEY_CONF_HAS_ERRORS, L"0");
			dump_config(h, L"After ImportConfig");
			return ERROR_SUCCESS;
		}

 		if (!boost::filesystem::is_directory(utf8::cvt<std::string>(target))) {
 			h.logMessage(L"Target folder not found: " + target);
			h.setProperty(KEY_CONF_CAN_CHANGE, L"1");
			h.setProperty(KEY_CONF_HAS_ERRORS, L"0");
			dump_config(h, L"After ImportConfig");
			return ERROR_SUCCESS;
		}

		installer_settings_provider provider(&h, target, map_data);
		if (!settings_manager::init_installer_settings(&provider, "")) {
			h.logMessage(L"Settings context had fatal errors");
			h.setProperty(KEY_CONF_OLD_ERROR, provider.get_error());
			h.setProperty(KEY_CONF_CAN_CHANGE, L"0");
			h.setProperty(KEY_CONF_HAS_ERRORS, L"1");
		}
		if (provider.has_errors()) {
			h.logMessage(L"Settings context reported errors (debug log end)");
			BOOST_FOREACH(std::wstring l, provider.get_errors()) {
				h.logMessage(l);
			}
			h.logMessage(L"Settings context reported errors (debug log end)");
			if (!settings_manager::has_boot_conf()) {
				h.logMessage(L"boot.conf was NOT found (so no new configuration)");
				if (settings_manager::context_exists(DEFAULT_CONF_OLD_LOCATION)) {
					h.logMessage("Old configuration found: " DEFAULT_CONF_OLD_LOCATION);
					h.setProperty(KEY_CONF_OLD_ERROR, std::wstring(L"Old configuration (") + utf8::cvt<std::wstring>(DEFAULT_CONF_OLD_LOCATION) + L") was found but we got errors accessing it: " + provider.get_error());
					h.setProperty(KEY_CONF_CAN_CHANGE, L"0");
					h.setProperty(KEY_CONF_HAS_ERRORS, L"1");
					dump_config(h, L"After ImportConfig");
					return ERROR_SUCCESS;
				} else {
					h.logMessage(L"Failed to read configuration but no configuration was found (so we are assuming there is no configuration).");
					h.setProperty(KEY_CONF_CAN_CHANGE, L"1");
					h.setProperty(KEY_CONF_HAS_ERRORS, L"0");
					dump_config(h, L"After ImportConfig");
					return ERROR_SUCCESS;
				}
			} else {
				h.logMessage(L"boot.conf was found but we got errors booting it...");
				h.setProperty(KEY_CONF_OLD_ERROR, provider.get_error());
				h.setProperty(KEY_CONF_CAN_CHANGE, L"0");
				h.setProperty(KEY_CONF_HAS_ERRORS, L"1");
				dump_config(h, L"After ImportConfig");
				return ERROR_SUCCESS;
			}
		}

		h.setProperty(KEY_CONFIGURATION_TYPE, utf8::cvt<std::wstring>(settings_manager::get_settings()->get_context()));
		h.logMessage("CONFIGURATION_TYPE=" + settings_manager::get_settings()->get_context());
		h.logMessage("CONFIGURATION_TYPE=" + settings_manager::get_settings()->get_info());
		h.setProperty(KEY_CONF_CAN_CHANGE, L"1");
		h.setProperty(KEY_CONF_HAS_ERRORS, L"0");

		h.setPropertyAndDefault(KEY_ALLOWED_HOSTS, utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "allowed hosts", "")));
		h.setPropertyAndDefault(KEY_NSCLIENT_PWD, utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "password", "")));

		h.setPropertyAndDefaultBool(KEY_CONF_NRPE, has_mod("NRPEServer"));
		h.setPropertyAndDefaultBool(KEY_CONF_SCHEDULER, has_mod("Scheduler"));
		h.setPropertyAndDefaultBool(KEY_CONF_NSCA, has_mod("NSCAClient"));
		h.setPropertyAndDefaultBool(KEY_CONF_NSCLIENT, has_mod("NSClientServer"));
		h.setPropertyAndDefaultBool(KEY_CONF_WEB, has_mod("WEBServer"));

		std::string insecure = settings_manager::get_settings()->get_string("/settings/NRPE/server", "insecure", "");
		std::string verify = settings_manager::get_settings()->get_string("/settings/NRPE/server", "verify mode", "");
		h.logMessage(L"insecure: " + utf8::cvt<std::wstring>(insecure));
		h.logMessage(L"verify: " + utf8::cvt<std::wstring>(verify));
		if (insecure == "true" || insecure == "1")
			h.setPropertyAndDefault(KEY_NRPEMODE, L"LEGACY");
		else if (verify == "peer-cert")
			h.setPropertyAndDefault(KEY_NRPEMODE, L"SECURE");
		else
			h.setPropertyAndDefault(KEY_NRPEMODE, L"SAFE");
		h.logMessage(L"NRPEMODE: " + h.getPropery(KEY_NRPEMODE));

		h.setPropertyAndDefault(KEY_NSCLIENT_PWD, utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "password", "")));

		h.setPropertyAndDefaultBool(KEY_CONF_CHECKS, has_mod("CheckSystem") && has_mod("CheckDisk") && has_mod("CheckEventLog") && has_mod("CheckHelpers") &&
					has_mod("CheckExternalScripts") && has_mod("CheckNSCP"));
		settings_manager::destroy_settings();


		h.applyProperty(KEY_ALLOWED_HOSTS, ALLOWED_HOSTS);
		h.applyProperty(KEY_NSCLIENT_PWD, NSCLIENT_PWD);
		h.applyProperty(KEY_NSCLIENT_PWD_DEFAULT, NSCLIENT_PWD_DEFAULT);
		h.applyProperty(KEY_CONF_SCHEDULER, CONF_SCHEDULER);
		h.applyProperty(KEY_CONF_CHECKS, CONF_CHECKS);
		h.applyProperty(KEY_CONF_NRPE, CONF_NRPE);
		h.applyProperty(KEY_CONF_NSCA, CONF_NSCA);
		h.applyProperty(KEY_CONF_WEB, CONF_WEB);
		h.applyProperty(KEY_CONF_NSCLIENT, CONF_NSCLIENT);
		h.applyProperty(KEY_NRPEMODE, NRPEMODE);

		h.applyProperty(KEY_CONFIGURATION_TYPE, CONFIGURATION_TYPE);
		h.applyProperty(KEY_CONF_INCLUDES, CONF_INCLUDES);
		h.applyProperty(KEY_INSTALL_SAMPLE_CONFIG, INSTALL_SAMPLE_CONFIG);
		h.applyProperty(KEY_GENERATE_SAMPLE_CONFIG, GENERATE_SAMPLE_CONFIG);


		dump_config(h, L"After ImportConfig");

	} catch (installer_exception e) {
		h.logMessage(L"Failed to read old configuration file: " + e.what());
		h.setProperty(KEY_CONF_OLD_ERROR, e.what());
		h.setProperty(KEY_CONF_CAN_CHANGE, L"0");
		h.setProperty(KEY_CONF_HAS_ERRORS, L"1");
		return ERROR_SUCCESS;
	} catch (...) {
		h.logMessage(L"Failed to read old configuration file: Unknown exception");
		h.setProperty(KEY_CONF_OLD_ERROR, L"Unknown exception!");
		h.setProperty(KEY_CONF_CAN_CHANGE, L"0");
		h.setProperty(KEY_CONF_HAS_ERRORS, L"1");
		return ERROR_SUCCESS;
	}
	return ERROR_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool write_config(msi_helper &h, std::wstring path, std::wstring file);

void write_key(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring path, std::wstring key, std::wstring val) {
	data.write_int(mode);
	data.write_string(path);
	data.write_string(key);
	data.write_string(val);
	h.logMessage(L"write_key: " + path + L"." + key + L"=" + val);
}

void write_key_mod(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring key, std::wstring val) {
	std::wstring path = utf8::cvt<std::wstring>(MAIN_MODULES_SECTION);
	if (val == L"1") {
		write_key(h, data, mode, path, key, L"enabled");
	} else {
		write_key(h, data, mode, path, key, L"disabled");
	}
}


void write_changed_key(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring path, std::wstring key) {
	std::wstring val = h.getPropery(prop);
	if (!h.propertyNotDefault(prop)) {
		h.logMessage(L"IGNORING property not changed: " + prop + L"; " + path + L"." + key + L"=" + val);
		return;
	}
	h.logMessage(L"write_changed_key: " + prop + L"; " + path + L"." + key + L"=" + val);
	write_key(h, data, 1, path, key, val);
}

void write_changed_key_mod(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring key) {
	std::wstring val = h.getPropery(prop);
	if (!h.propertyNotDefault(prop)) {
		h.logMessage(L"write_changed_key_mod: IGNORING property not changed: " + prop + L"; <modules>." + key + L"=" + val);
		return;
	}
	h.logMessage(L"write_changed_key_mod: " + prop + L"; <modules>." + key + L"=" + val);
	write_key_mod(h, data, 1, key, val);
}

bool write_property_if_set(msi_helper &h, msi_helper::custom_action_data_w &data, const std::wstring prop, std::wstring path, std::wstring key) {
	std::wstring val = boost::algorithm::trim_copy(h.getPropery(prop));
	if (!val.empty()) {
		h.logMessage(L"write_changed_key_mod: " + prop + L"; <modules>." + key + L"=" + val);
		write_key(h, data, 1, path, key, val);
		return true;
	} else {
		h.logMessage(L"IGNORING property not set: " + prop + L"; " + path + L"." + key + L"=" + val);
	}
	return false;
}

extern "C" UINT __stdcall ScheduleWriteConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, L"ScheduleWriteConfig");
	try {

		h.dumpReason(L"Before ScheduleWriteConfig");
		h.dumpProperty(KEY_ALLOWED_HOSTS);
		h.dumpProperty(KEY_NSCLIENT_PWD);
		h.dumpProperty(KEY_NSCLIENT_PWD_DEFAULT);
		h.dumpProperty(KEY_CONF_SCHEDULER);
		h.dumpProperty(KEY_CONF_CHECKS);
		h.dumpProperty(KEY_CONF_NRPE);
		h.dumpProperty(KEY_CONF_NSCA);
		h.dumpProperty(KEY_CONF_WEB);
		h.dumpProperty(KEY_CONF_NSCLIENT);
		h.dumpProperty(KEY_NRPEMODE);

		h.dumpProperty(KEY_CONFIGURATION_TYPE);
		h.dumpProperty(KEY_CONF_INCLUDES);
		h.dumpProperty(KEY_INSTALL_SAMPLE_CONFIG);
		h.dumpProperty(KEY_GENERATE_SAMPLE_CONFIG);

		h.dumpProperty(KEY_CONF_CAN_CHANGE);


		h.dumpProperty(OP5_SERVER);
		h.dumpProperty(OP5_USER);
		h.dumpProperty(OP5_PASSWORD);
		h.dumpProperty(OP5_HOSTGROUPS);
		h.dumpProperty(OP5_CONTACTGROUP);

		                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
		if (h.getPropery(KEY_CONF_CAN_CHANGE) != L"1") {
			h.logMessage(L"Configuration changes not allowed: set CONF_CAN_CHANGE=1");
			return ERROR_SUCCESS;
		}
		msi_helper::custom_action_data_w data;
		data.write_string(h.getTargetPath(L"INSTALLLOCATION"));
		data.write_string(h.getPropery(KEY_CONFIGURATION_TYPE));
		data.write_string(h.getPropery(L"RESTORE_FILE"));
		data.write_int(h.getPropery(L"ADD_DEFAULTS")==L"1"?1:0);

		std::wstring confInclude = h.getPropery(KEY_CONF_INCLUDES);
		h.logMessage(L"Adding include: " + confInclude);
		if (!confInclude.empty()) {
			std::vector<std::wstring> lst;
			boost::split(lst, confInclude, boost::is_any_of(L";"));
			for (int i=0;i+1<lst.size();i+=2) {
				h.logMessage(L" + : " + lst[i] + L"=" + lst[i+1]);
				write_key(h, data, 1, L"/includes", lst[i], lst[i+1]);
			}
		}

		write_changed_key_mod(h, data, KEY_CONF_NRPE, L"NRPEServer");
		write_changed_key_mod(h, data, KEY_CONF_SCHEDULER, L"Scheduler");
		write_changed_key_mod(h, data, KEY_CONF_NSCA, L"NSCAClient");
		write_changed_key_mod(h, data, KEY_CONF_NSCLIENT, L"NSClientServer");
		write_changed_key_mod(h, data, L"CONF_WMI", L"CheckWMI");
		write_changed_key_mod(h, data, KEY_CONF_WEB, L"WEBServer");

		if (h.propertyNotDefault(KEY_CONF_CHECKS)) {
			std::wstring modval = h.getPropery(KEY_CONF_CHECKS);
			if (modval == L"1") {
				modval = L"enabled";
			} else {
				modval = L"disabled";
			}
			write_key_mod(h, data, 1, L"CheckSystem", modval);
			write_key_mod(h, data, 1, L"CheckDisk", modval);
			write_key_mod(h, data, 1, L"CheckEventLog", modval);
			write_key_mod(h, data, 1, L"CheckHelpers", modval);
			write_key_mod(h, data, 1, L"CheckExternalScripts", modval);
			write_key_mod(h, data, 1, L"CheckNSCP", modval);
		}
		if (h.getPropery(KEY_CONF_NRPE) == L"1") {
			if (h.propertyNotDefault(KEY_NRPEMODE)) {
				std::wstring mode = h.getPropery(KEY_NRPEMODE);
				if (mode == L"LEGACY") {
					write_key(h, data, 1, L"/settings/NRPE/server", L"insecure", L"true");
					write_key(h, data, 1, L"/settings/NRPE/server", L"ssl options", L"");
					write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"");
				} else {
					write_key(h, data, 1, L"/settings/NRPE/server", L"insecure", L"false");
					write_key(h, data, 1, L"/settings/NRPE/server", L"ssl options", L"no-sslv2,no-sslv3");
				}
				if (mode == L"SAFE")
					write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"peer-cert");
				else
					write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"none");
			}
		}

		std::wstring defpath = L"/settings/default";
		write_changed_key(h, data, KEY_ALLOWED_HOSTS, defpath, L"allowed hosts");
		write_changed_key(h, data, KEY_NSCLIENT_PWD, defpath, L"password");

		std::wstring confSet = h.getPropery(L"CONF_SET");
		h.logMessage(L"Adding conf: " + confSet);
		if (!confSet.empty()) {
			std::vector<std::wstring> lst;
			boost::split(lst, confSet, boost::is_any_of(L";"));
			for (int i = 0; i + 2 < lst.size(); i += 3) {
				h.logMessage(L" + : " + lst[i] + L" " + lst[i + 1] + L"=" + lst[i + 2]);
				write_key(h, data, 1, lst[i], lst[i + 1], lst[i + 2]);
			}
		}

		if (write_property_if_set(h, data, OP5_SERVER, L"/settings/op5", L"server")) {
			write_key(h, data, 1, L"/modules", L"OP5Client", L"enabled");
		}
		write_property_if_set(h, data, OP5_USER, L"/settings/op5", L"user");
		write_property_if_set(h, data, OP5_PASSWORD, L"/settings/op5", L"password");
		write_property_if_set(h, data, OP5_HOSTGROUPS, L"/settings/op5", L"hostgroups");
		write_property_if_set(h, data, OP5_CONTACTGROUP, L"/settings/op5", L"contactgroups");

		if (data.has_data()) {
			h.logMessage(L"Scheduling (ExecWriteConfig): " + data.to_string());
			HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
			if (FAILED(hr)) {
				h.errorMessage(L"failed to schedule config update");
				return hr;
			}
		}
	} catch (installer_exception e) {
		h.errorMessage(L"Failed to install service: " + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(L"Failed to install service: <UNKNOWN EXCEPTION>");
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}
extern "C" UINT __stdcall ExecWriteConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, L"ExecWriteConfig");
	try {
		h.logMessage(L"RAW: " + h.getPropery(L"CustomActionData"));
		msi_helper::custom_action_data_r data(h.getPropery(L"CustomActionData"));
		h.logMessage(L"Got CA data: " + data.to_string());
		std::wstring target = data.get_next_string();
		std::wstring context_w = data.get_next_string();
		std::string context = utf8::cvt<std::string>(context_w);
		std::wstring restore = data.get_next_string();
		int add_defaults = data.get_next_int();

		h.logMessage(L"Target: " + target);
		h.logMessage("Context: " + context);
		h.logMessage(L"Restore: " + restore);

		boost::filesystem::path path = target;
		boost::filesystem::path old_path = path / "nsc.ini.old";
		path = path / "nsc.ini";

		boost::filesystem::path restore_path = restore;

		if (boost::filesystem::exists(old_path))
			h.logMessage(L"Found old (.old) file: " + strEx::xtos(boost::filesystem::file_size(old_path)));
		if (boost::filesystem::exists(path))
			h.logMessage(L"Found old file: " + strEx::xtos(boost::filesystem::file_size(path)));
		if (boost::filesystem::exists(restore_path))
			h.logMessage(L"Found restore file: " + strEx::xtos(boost::filesystem::file_size(restore_path)));

		if (boost::filesystem::exists(restore_path)) {
			h.logMessage(L"Restore path exists: " + restore);
			if (!boost::filesystem::exists(path)) {
				h.logMessage(L"Restoring nsc.ini configuration file");
				copy_file(h, restore_path.wstring(), path.wstring());
			}
			if (!boost::filesystem::exists(old_path)) {
				h.logMessage(L"Creating backup nsc.ini.old configuration file");
				copy_file(h, restore_path.wstring(), old_path.wstring());
			}
		}

		if (boost::filesystem::exists(path))
			h.logMessage(L"Size (001): " + strEx::xtos(boost::filesystem::file_size(path)));

		installer_settings_provider provider(&h, target);
		if (!settings_manager::init_installer_settings(&provider, context)) {
			h.errorMessage(L"Failed to boot settings: " + provider.get_error());
			h.logMessage(L"Switching context: " + context_w);
			settings_manager::change_context(context);
			return ERROR_SUCCESS;
		}
		if (boost::filesystem::exists(path))
			h.logMessage(L"Size (002): " + strEx::xtos(boost::filesystem::file_size(path)));

		h.logMessage("Switching to: " + context);
		settings_manager::change_context(context);
		if (boost::filesystem::exists(path))
			h.logMessage(L"Size (003): " + strEx::xtos(boost::filesystem::file_size(path)));

		while (data.has_more()) {
			unsigned int mode = data.get_next_int();
			std::string path = utf8::cvt<std::string>(data.get_next_string());
			std::string key = utf8::cvt<std::string>(data.get_next_string());
			std::string val = utf8::cvt<std::string>(data.get_next_string());

			if (mode == 1) {
				h.logMessage("Set key: " + path + "/" + key + " = " + val);
				settings_manager::get_settings()->set_string(path, key, val);
			} else if (mode == 2) {
				h.logMessage("***UNSUPPORTED*** Remove key: " + path + "/" + key + " = " + val);
			} else {
				h.errorMessage(L"Unknown mode in CA data: " + strEx::xtos(mode) + L": " + data.to_string());
				return ERROR_INSTALL_FAILURE;
			}
		}
		if (boost::filesystem::exists(path))
			h.logMessage(L"Size (004): " + strEx::xtos(boost::filesystem::file_size(path)));
		settings_manager::get_settings()->save();
		if (boost::filesystem::exists(path))
			h.logMessage(L"Size (005): " + strEx::xtos(boost::filesystem::file_size(path)));
	} catch (const installer_exception &e) {
		h.errorMessage(L"Failed to write configuration: " + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (const std::exception &e) {
		h.errorMessage(L"Failed to write configuration: " + utf8::to_unicode(e.what()));
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(L"Failed to write configuration: <UNKNOWN EXCEPTION>");
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}



extern "C" UINT __stdcall NeedUninstall (MSIHANDLE hInstall) {
	msi_helper h(hInstall, L"NeedUninstall");
	try {
		std::list<std::wstring> list = h.enumProducts();
		for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
			if ((*cit) == L"{E7CF81FE-8505-4D4A-8ED3-48949C8E4D5B}") {
				h.errorMessage(L"Found old NSClient++/OP5 client installed, will uninstall it now!");
				std::wstring command = L"msiexec /uninstall " + (*cit);
				wchar_t *cmd = new wchar_t[command.length()+1];
				wcsncpy(cmd, command.c_str(), command.length());
				cmd[command.length()] = 0;
				PROCESS_INFORMATION pi;
				STARTUPINFO si;
				ZeroMemory(&si, sizeof(STARTUPINFO));
				si.cb = sizeof(STARTUPINFO);

				BOOL processOK = CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
				delete [] cmd;
				if (processOK) {
					DWORD dwstate = WaitForSingleObject(pi.hProcess, 1000*60);
					if (dwstate == WAIT_TIMEOUT)
						h.errorMessage(L"Failed to wait for process (probably not such a big deal, the uninstall usualy takes alonger)!");
				} else {
					h.errorMessage(L"Failed to start process: " + utf8::cvt<std::wstring>(error::lookup::last_error()));
				}
			}
		}
	
	} catch (installer_exception e) {
		h.errorMessage(L"Failed to start service: " + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(L"Failed to start service: <UNKNOWN EXCEPTION>");
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
};


extern "C" UINT __stdcall TranslateSid (MSIHANDLE hInstall) {
	TCHAR szSid[MAX_PATH] = {0};
	TCHAR szSidProperty[MAX_PATH] = {0};
	TCHAR szName[MAX_PATH] = {0};
	DWORD size = MAX_PATH;
	UINT ret = 0;
	ret = MsiGetProperty (hInstall, L"TRANSLATE_SID", szSid, &size);

	if(ret != ERROR_SUCCESS) {
		return 4444;
	}

	size = MAX_PATH;
	ret = MsiGetProperty (hInstall, L"TRANSLATE_SID_PROPERTY", szSidProperty, &size);

	if(ret != ERROR_SUCCESS) {
		return 4445;
	}

	PSID pSID = NULL;

	if(!ConvertStringSidToSid(szSid, &pSID)) {
		return 4446;
	}

	size = MAX_PATH;
	TCHAR szRefDomain[MAX_PATH] = {0};
	SID_NAME_USE nameUse;
	DWORD refSize = MAX_PATH;
	if(!LookupAccountSid(NULL, pSID, szName, &size, szRefDomain, &refSize, &nameUse)) {
		if(pSID != NULL) {
			LocalFree(pSID);
		}
		return 4447;
	}

	ret = MsiSetProperty (hInstall, szSidProperty, szName);
	if(!ConvertStringSidToSid(szSid, &pSID)) {
		if(pSID != NULL) {
			LocalFree(pSID);
		}
		return 4448;
	}

	if(pSID != NULL) {
		LocalFree(pSID);
	}
	return ERROR_SUCCESS;
}

