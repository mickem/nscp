//#define _WIN32_WINNT 0x0500
#include <string>

#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <error.hpp>
#include <config.h>
#include <ServiceCmd.h>
#include <char_buffer.hpp>
#include <file_helpers.hpp>
#include "installer_helper.hpp"
#include <Sddl.h>
#include "../settings_manager/settings_manager_impl.h"
#include <nsclient/logger.hpp>
#include <nsclient/base_logger_impl.hpp>
#include <wstring.hpp>
#include <settings/config.hpp>

const UINT COST_SERVICE_INSTALL = 2000;

bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description, std::wstring service_deps);
bool uninstall(msi_helper &h, std::wstring service_name);


void copy_file(msi_helper &h, std::wstring source, std::wstring target) {
	if (boost::filesystem::is_regular(utf8::cvt<std::string>(source))) {
		h.logMessage(_T("Copying: ") + source + _T(" to ") + target);
		if (!CopyFile(source.c_str(), target.c_str(), FALSE)) {
			h.errorMessage(_T("Failed to copy file: ") + utf8::cvt<std::wstring>(error::lookup::last_error()));
		}
	} else {
		h.logMessage(_T("Copying failed: ") + source + _T(" to ") + target + _T(" source was not found."));
	}

}

std::string nsclient::logging::logger_helper::create(const std::string&, NSCAPI::log_level::level level, const char*, const int, const std::string &message) {
	if (level < NSCAPI::log_level::info)
		return "E" + message;
	return "I" + message;
}


class msi_logger : public nsclient::logging::logging_interface_impl {
public:
	std::wstring error;
	std::list<std::wstring> log_;
	msi_logger()  {}


	void do_log(const std::string data) {
		std::wstring str = utf8::cvt<std::wstring>(data);
		if (str.empty())
			return;
		if (str[0] == L'E') {
			if (!error.empty())
				error += _T("\n");
			error += str.substr(1);
		}
		log_.push_back(str.substr(1));
	}
	void asynch_configure() {}
	void synch_configure() {}
	bool startup() { return true; }
	bool shutdown() { return true; }

	std::wstring get_error() {
		return error;
	}
	boolean has_errors() {
		return !error.empty();
	}
};


static msi_logger *impl = NULL;

msi_logger* get_impl() {
	if (impl == NULL)
		impl = new msi_logger();
	return impl;
}


nsclient::logging::logger_interface* nsclient::logging::logger::get_logger() {
	return get_impl();
}

void nsclient::logging::logger::subscribe_raw(raw_subscriber_type subscriber) {}
void nsclient::logging::logger::clear_subscribers() {}
bool nsclient::logging::logger::startup() { return true; }
bool nsclient::logging::logger::shutdown() { return true; }
void nsclient::logging::logger::configure() {}

void nsclient::logging::logger::set_log_level(NSCAPI::log_level::level) {}
void nsclient::logging::logger::set_log_level(std::string level) {}
struct installer_settings_provider : public settings_manager::provider_interface {

	msi_helper *h;
	std::string basepath;
	std::string old_settings_map;

	installer_settings_provider(msi_helper *h, std::wstring basepath, std::wstring old_settings_map) : h(h), basepath(utf8::cvt<std::string>(basepath)), old_settings_map(utf8::cvt<std::string>(old_settings_map)) {}
	installer_settings_provider(msi_helper *h, std::wstring basepath) : h(h), basepath(utf8::cvt<std::string>(basepath)) {}

	virtual std::string expand_path(std::string file) {
		strEx::replace(file, "${base-path}", basepath);
		strEx::replace(file, "${exe-path}", basepath);
		strEx::replace(file, "${shared-path}", basepath);
		return file;
	}
	std::string get_data(std::string key) {
		if (!old_settings_map.empty() && key == "old_settings_map_data") {
			return old_settings_map;
		}
		return "";
	}
};

static const wchar_t alphanum[] = _T("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
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
// - configuration not allowed		=> CONF_CAN_CHANGE=0, CONF_OLD_FOUND=0, CONF_HAS_ERRORS=0
// - target not found				=> CONF_CAN_CHANGE=1, CONF_OLD_FOUND=0, CONF_HAS_ERRORS=0
// - target found + config read		=> CONF_CAN_CHANGE=1, CONF_OLD_FOUND=1, CONF_HAS_ERRORS=0
// - target found + config NOT read => CONF_CAN_CHANGE=0, CONF_OLD_FOUND=?, CONF_HAS_ERRORS=1
//
// Interpretation:
// CONF_HAS_ERRORS=1	=> Dont allow anything (inform of issue)
// CONF_CAN_CHANGE=1	=> Allow change
// CONF_OLD_FOUND=0		=> Allow setting boot.ini


std::wstring read_map_data(msi_helper &h) {
	std::wstring ret;
	PMSIHANDLE hView = h.open_execute_view(_T("SELECT Data FROM Binary WHERE Name='OldSettingsMap'"));
	if (h.isNull(hView)) {
		h.logMessage(_T("Failed to query service view!"));
		return _T("");
	}

	PMSIHANDLE hRec = h.fetch_record(hView);
	if (hRec != NULL) {
		ret = h.get_record_blob(hRec, 1);
		::MsiCloseHandle(hRec);
	}
	::MsiCloseHandle(hView);
	return ret;
}

extern "C" UINT __stdcall ApplyTool(MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ImportConfig"));
	try {
		h.logMessage("Applying monitorin tool config");
		std::wstring tool = h.getPropery(_T("MONITORING_TOOL"));
		h.logMessage(_T("Monitoring tool is: ") + tool);

		if (tool == _T("OP5")) {
			h.setProperty(_T("NSCLIENT_PWD"), _T(""));
			h.setProperty(_T("NSCLIENT_PWD_OLD"), _T(""));
			h.setProperty(_T("NSCLIENT_PWD_DEFAULT"), _T(""));
			h.setProperty(_T("CONF_CHECKS_OLD"), _T("1"));
			h.setProperty(_T("CONF_CHECKS"), _T("1"));
			h.setProperty(_T("CONF_NRPE_OLD"), _T("1"));
			h.setProperty(_T("CONF_NRPE"), _T("1"));
			h.setProperty(_T("CONF_NSCA_OLD"), _T("1"));
			h.setProperty(_T("CONF_NSCA"), _T("1"));
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
			h.setProperty(_T("CONF_WEB_OLD"), _T("0"));
			h.setProperty(_T("CONF_WEB"), _T("0"));
			h.setProperty(_T("CONF_INCLUDES"), _T("op5;op5.ini"));
			h.setProperty(_T("NRPEMODE_OLD"), _T("LEGACY"));
			h.setProperty(_T("NRPEMODE"), _T("LEGACY"));
			h.setProperty(_T("INSTALL_SAMPLE_CONFIG"), _T(""));
			h.setProperty(_T("GENERATE_SAMPLE_CONFIG"), _T(""));
			h.setProperty(_T("CONFIGURATION_TYPE"), _T("registry://HKEY_LOCAL_MACHINE/software/NSClient++"));
			h.setFeatureLocal(_T("OP5Montoring"));
		} else if (tool == _T("GENERIC")) {
			h.setProperty(_T("NSCLIENT_PWD"), genpwd(16));
			h.setProperty(_T("NSCLIENT_PWD_OLD"), _T(""));
			h.setProperty(_T("CONF_CHECKS_OLD"), _T(""));
			h.setProperty(_T("CONF_CHECKS"), _T("1"));
			h.setProperty(_T("CONF_NRPE_OLD"), _T(""));
			h.setProperty(_T("CONF_NRPE"), _T("1"));
			h.setProperty(_T("CONF_NSCA_OLD"), _T(""));
			h.setProperty(_T("CONF_NSCA"), _T(""));
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
			h.setProperty(_T("CONF_WEB_OLD"), _T(""));
			h.setProperty(_T("CONF_WEB"), _T(""));
			h.setProperty(_T("CONF_INCLUDES"), _T(""));
			h.setProperty(_T("NRPEMODE_OLD"), _T(""));
			h.setProperty(_T("NRPEMODE"), _T("SAFE"));
			h.setProperty(_T("INSTALL_SAMPLE_CONFIG"), _T(""));
			h.setProperty(_T("GENERATE_SAMPLE_CONFIG"), _T(""));
			h.setProperty(_T("CONFIGURATION_TYPE"), _T("ini://${shared-path}/nsclient.ini"));
			h.setFeatureAbsent(_T("OP5Montoring"));
		}

	} catch (installer_exception e) {
		h.logMessage(_T("Failed to apply monitoring tool: ") + e.what());
		return ERROR_SUCCESS;
	} catch (...) {
		h.logMessage(_T("Failed to apply monitoring tool: Unknown exception"));
		return ERROR_SUCCESS;
	}
	return ERROR_SUCCESS;
}



extern "C" UINT __stdcall ImportConfig(MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ImportConfig"));
	try {
		h.logMessage("importing config");
		std::wstring target = h.getTargetPath(_T("INSTALLLOCATION"));
		std::wstring allow = h.getPropery(_T("ALLOW_CONFIGURATION"));

		std::wstring pwd = h.getPropery(_T("NSCLIENT_PWD"));
		if (pwd == _T("$GEN$")) {
			h.setProperty(_T("NSCLIENT_PWD"), genpwd(16));
		}
		std::wstring map_data = read_map_data(h);
		if (allow == _T("0")) {
			h.logMessage(_T("Configuration not allowed: ") + allow);
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
			h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
			h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));
			return ERROR_SUCCESS;
		}

 		if (!boost::filesystem::is_directory(utf8::cvt<std::string>(target))) {
 			h.logMessage(_T("Target folder not found: ") + target);
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
 			h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
			h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));
			return ERROR_SUCCESS;
		}

		installer_settings_provider provider(&h, target, map_data);
		if (!settings_manager::init_settings(&provider, "")) {
			h.logMessage(_T("Settings context had fatal errors"));
			h.setProperty(_T("CONF_OLD_ERROR"), get_impl()->get_error());
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
			h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
			h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
		}
		if (get_impl()->has_errors()) {
			h.logMessage(_T("Settings context reported errors (debug log end)"));
			BOOST_FOREACH(std::wstring l, get_impl()->log_) {
				h.logMessage(l);
			}
			h.logMessage(_T("Settings context reported errors (debug log end)"));
			if (!settings_manager::has_boot_conf()) {
				h.logMessage(_T("boot.conf was NOT found (so no new configuration)"));
				if (settings_manager::context_exists(DEFAULT_CONF_OLD_LOCATION)) {
					h.logMessage("Old configuration found: " DEFAULT_CONF_OLD_LOCATION);
					h.setProperty(_T("CONF_OLD_ERROR"), std::wstring(_T("Old configuration (")) + utf8::cvt<std::wstring>(DEFAULT_CONF_OLD_LOCATION) + _T(") was found but we got errors accessing it: ") + get_impl()->get_error());
					h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
					h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
					h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
					return ERROR_SUCCESS;
				} else {
					h.logMessage(_T("Failed to read configuration but no configuration was found (so we are assuming there is no configuration)."));
					h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
					h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
					h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));
					return ERROR_SUCCESS;
				}
			} else {
				h.logMessage(_T("boot.conf was found but we got errors booting it..."));
				h.setProperty(_T("CONF_OLD_ERROR"), get_impl()->get_error());
				h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
				h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
				h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
				return ERROR_SUCCESS;
			}
		}

		h.setProperty(_T("CONFIGURATION_TYPE"), utf8::cvt<std::wstring>(settings_manager::get_settings()->get_context()));
		h.logMessage("CONFIGURATION_TYPE=" + settings_manager::get_settings()->get_context());
		h.logMessage("CONFIGURATION_TYPE=" + settings_manager::get_settings()->get_info());
		h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
		h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));

		h.setPropertyAndOld(_T("ALLOWED_HOSTS"), utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "allowed hosts", "")));
		h.setPropertyAndOld(_T("NSCLIENT_PWD"), utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "password", "")));

		h.setPropertyAndOldBool(_T("CONF_NRPE"), has_mod("NRPEServer"));
		h.setPropertyAndOldBool(_T("CONF_SCHEDULER"), has_mod("Scheduler"));
		h.setPropertyAndOldBool(_T("CONF_NSCA"), has_mod("NSCAClient"));
		h.setPropertyAndOldBool(_T("CONF_NSCLIENT"), has_mod("NSClientServer"));
		h.setPropertyAndOldBool(_T("CONF_WEB"), has_mod("WEBServer"));

		std::string insecure = settings_manager::get_settings()->get_string("/settings/NRPE/server", "insecure", "");
		std::string verify = settings_manager::get_settings()->get_string("/settings/NRPE/server", "verify mode", "");
		h.logMessage(_T("insecure: ") + utf8::cvt<std::wstring>(insecure));
		h.logMessage(_T("verify: ") + utf8::cvt<std::wstring>(verify));
		if (insecure == "true" || insecure == "1")
			h.setPropertyAndOld(_T("NRPEMODE"), _T("LEGACY"));
		else if (verify == "peer-cert")
			h.setPropertyAndOld(_T("NRPEMODE"), _T("SECURE"));
		else
			h.setPropertyAndOld(_T("NRPEMODE"), _T("SAFE"));
		h.logMessage(_T("NRPEMODE: ") + h.getPropery(_T("NRPEMODE")));

		h.setPropertyAndOld(_T("NSCLIENT_PWD"), utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "password", "")));

		h.setPropertyAndOldBool(_T("CONF_CHECKS"), has_mod("CheckSystem") && has_mod("CheckDisk") && has_mod("CheckEventLog") && has_mod("CheckHelpers") &&
					has_mod("CheckExternalScripts") && has_mod("CheckNSCP"));
		settings_manager::destroy_settings();
	} catch (installer_exception e) {
		h.logMessage(_T("Failed to read old configuration file: ") + e.what());
		h.setProperty(_T("CONF_OLD_ERROR"), e.what());
		h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
		h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
		h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
		return ERROR_SUCCESS;
	} catch (...) {
		h.logMessage(_T("Failed to read old configuration file: Unknown exception"));
		h.setProperty(_T("CONF_OLD_ERROR"), _T("Unknown exception!"));
		h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
		h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
		h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
		return ERROR_SUCCESS;
	}
	return ERROR_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool write_config(msi_helper &h, std::wstring path, std::wstring file);


void write_changed_key(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring path, std::wstring key) {
	std::wstring val = h.getPropery(prop);
	if (val == _T("$GEN$"))
		val = genpwd(12);
	if (!h.propertyNotOld(prop)) {
		h.logMessage(_T("IGNORING property not changed: ") + prop + _T("; ") + path + _T(".") + key + _T("=") + val);
		return;
	}
	h.logMessage(_T("write_changed_key: ") + prop + _T("; ") + path + _T(".") + key + _T("=") + val);
	data.write_int(1);
	data.write_string(path);
	data.write_string(key);
	data.write_string(val);
}

void write_key(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring path, std::wstring key, std::wstring val) {
	data.write_int(mode);
	data.write_string(path);
	data.write_string(key);
	data.write_string(val);
	h.logMessage(_T("write_key: ") + path + _T(".") + key + _T("=") + val);
}

extern "C" UINT __stdcall ScheduleWriteConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ScheduleWriteConfig"));
	try {
		if (h.getPropery(_T("CONF_CAN_CHANGE")) != _T("1")) {
			h.logMessage(_T("Configuration changes not allowed: set CONF_CAN_CHANGE=1"));
			return ERROR_SUCCESS;
		}
		msi_helper::custom_action_data_w data;
		data.write_string(h.getTargetPath(_T("INSTALLLOCATION")));
		data.write_string(h.getPropery(_T("CONFIGURATION_TYPE")));
		data.write_string(h.getPropery(_T("RESTORE_FILE")));
		data.write_int(h.getPropery(_T("ADD_DEFAULTS"))==_T("1")?1:0);

		std::wstring confInclude = h.getPropery(_T("CONF_INCLUDES"));
		h.logMessage(_T("Adding include: ") + confInclude);
		if (!confInclude.empty()) {
			std::vector<std::wstring> lst;
			boost::split(lst, confInclude, boost::is_any_of(_T(";")));
			for (int i=0;i+1<lst.size();i+=2) {
				h.logMessage(_T(" + : ") + lst[i] + _T("=") + lst[i+1]);
				write_key(h, data, 1, _T("/includes"), lst[i], lst[i+1]);
			}
		}

		std::wstring modpath = _T(MAIN_MODULES_SECTION);
		write_changed_key(h, data, _T("CONF_NRPE"), modpath, _T("NRPEServer"));
		write_changed_key(h, data, _T("CONF_SCHEDULER"), modpath, _T("Scheduler"));
		write_changed_key(h, data, _T("CONF_NSCA"), modpath, _T("NSCAClient"));
		write_changed_key(h, data, _T("CONF_NSCLIENT"), modpath, _T("NSClientServer"));
		write_changed_key(h, data, _T("CONF_WMI"), modpath, _T("CheckWMI"));
		write_changed_key(h, data, _T("CONF_WEB"), modpath, _T("WEBSErver"));

		if (h.propertyNotOld(_T("CONF_CHECKS"))) {
			std::wstring modval = h.getPropery(_T("CONF_CHECKS"));
			write_key(h, data, 1, modpath, _T("CheckSystem"), modval);
			write_key(h, data, 1, modpath, _T("CheckDisk"), modval);
			write_key(h, data, 1, modpath, _T("CheckEventLog"), modval);
			write_key(h, data, 1, modpath, _T("CheckHelpers"), modval);
			write_key(h, data, 1, modpath, _T("CheckExternalScripts"), modval);
			write_key(h, data, 1, modpath, _T("CheckNSCP"), modval);
		}
		if (h.getPropery(_T("CONF_NRPE")) == _T("1")) {
			if (h.propertyNotOld(_T("NRPEMODE"))) {
				std::wstring mode = h.getPropery(_T("NRPEMODE"));
				if (mode == _T("LEGACY")) {
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("insecure"), _T("true"));
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("ssl options"), _T(""));
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("verify mode"), _T(""));
				} else {
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("insecure"), _T("false"));
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("ssl options"), _T("no-sslv2,no-sslv3"));
				}
				if (mode == _T("SAFE"))
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("verify mode"), _T("peer-cert"));
				else
					write_key(h, data, 1, _T("/settings/NRPE/server"), _T("verify mode"), _T("none"));
			}
		}

		std::wstring defpath = _T("/settings/default");
		write_changed_key(h, data, _T("ALLOWED_HOSTS"), defpath, _T("allowed hosts"));
		write_changed_key(h, data, _T("NSCLIENT_PWD"), defpath, _T("password"));

		std::wstring confSet = h.getPropery(_T("CONF_SET"));
		h.logMessage(_T("Adding conf: ") + confSet);
		if (!confSet.empty()) {
			std::vector<std::wstring> lst;
			boost::split(lst, confSet, boost::is_any_of(_T(";")));
			for (int i = 0; i + 2 < lst.size(); i += 3) {
				h.logMessage(_T(" + : ") + lst[i] + _T(" ") + lst[i + 1] + _T("=") + lst[i + 2]);
				write_key(h, data, 1, lst[i], lst[i + 1], lst[i + 2]);
			}
		}

		if (data.has_data()) {
			h.logMessage(_T("Scheduling (ExecWriteConfig): ") + data.to_string());
			HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
			if (FAILED(hr)) {
				h.errorMessage(_T("failed to schedule config update"));
				return hr;
			}
		}
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to install service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}
extern "C" UINT __stdcall ExecWriteConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ExecWriteConfig"));
	try {
		h.logMessage(_T("RAW: ") + h.getPropery(L"CustomActionData"));
		msi_helper::custom_action_data_r data(h.getPropery(L"CustomActionData"));
		h.logMessage(_T("Got CA data: ") + data.to_string());
		std::wstring target = data.get_next_string();
		std::wstring context_w = data.get_next_string();
		std::string context = utf8::cvt<std::string>(context_w);
		std::wstring restore = data.get_next_string();
		int add_defaults = data.get_next_int();

		h.logMessage(_T("Target: ") + target);
		h.logMessage("Context: " + context);
		h.logMessage(_T("Restore: ") + restore);

		boost::filesystem::path path = target;
		boost::filesystem::path old_path = path / "nsc.ini.old";
		path = path / "nsc.ini";

		boost::filesystem::path restore_path = restore;

		if (boost::filesystem::exists(old_path))
			h.logMessage(_T("Found old (.old) file: ") + strEx::xtos(boost::filesystem::file_size(old_path)));
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Found old file: ") + strEx::xtos(boost::filesystem::file_size(path)));
		if (boost::filesystem::exists(restore_path))
			h.logMessage(_T("Found restore file: ") + strEx::xtos(boost::filesystem::file_size(restore_path)));

		if (boost::filesystem::exists(restore_path)) {
			if (!boost::filesystem::exists(path)) {
				h.logMessage(_T("Restoring nsc.ini configuration file"));
				copy_file(h, restore_path.wstring(), path.wstring());
			}
			if (!boost::filesystem::exists(old_path)) {
				h.logMessage(_T("Creating backup nsc.ini.old configuration file"));
				copy_file(h, restore_path.wstring(), old_path.wstring());
			}
		}

		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (001): ") + strEx::xtos(boost::filesystem::file_size(path)));

		installer_settings_provider provider(&h, target);
		if (!settings_manager::init_settings(&provider, context)) {
			h.errorMessage(_T("Failed to boot settings: ") + get_impl()->get_error());
			return ERROR_INSTALL_FAILURE;
		}
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (002): ") + strEx::xtos(boost::filesystem::file_size(path)));

		h.logMessage("Switching to: " + context);
		settings_manager::change_context(context);
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (003): ") + strEx::xtos(boost::filesystem::file_size(path)));

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
				h.errorMessage(_T("Unknown mode in CA data: ") + strEx::xtos(mode) + _T(": ") + data.to_string());
				return ERROR_INSTALL_FAILURE;
			}
		}
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (004): ") + strEx::xtos(boost::filesystem::file_size(path)));
		settings_manager::get_settings()->save();
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (005): ") + strEx::xtos(boost::filesystem::file_size(path)));
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to write configuration: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to write configuration: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}



extern "C" UINT __stdcall NeedUninstall (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("NeedUninstall"));
	try {
		std::list<std::wstring> list = h.enumProducts();
		for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
			if ((*cit) == _T("{E7CF81FE-8505-4D4A-8ED3-48949C8E4D5B}")) {
				h.errorMessage(_T("Found old NSClient++/OP5 client installed, will uninstall it now!"));
				std::wstring command = _T("msiexec /uninstall ") + (*cit);
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
						h.errorMessage(_T("Failed to wait for process (probably not such a big deal, the uninstall usualy takes alonger)!"));
				} else {
					h.errorMessage(_T("Failed to start process: ") + utf8::cvt<std::wstring>(error::lookup::last_error()));
				}
			}
		}
	
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to start service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to start service: <UNKNOWN EXCEPTION>"));
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
	ret = MsiGetProperty (hInstall, _T("TRANSLATE_SID"), szSid, &size);

	if(ret != ERROR_SUCCESS) {
		return 4444;
	}

	size = MAX_PATH;
	ret = MsiGetProperty (hInstall, _T("TRANSLATE_SID_PROPERTY"), szSidProperty, &size);

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

