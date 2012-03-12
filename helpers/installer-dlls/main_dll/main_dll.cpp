#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <error.hpp>
//#include <Settings.h>
//#include <config.h>
#include <config.h>
#include <ServiceCmd.h>
#include <char_buffer.hpp>
#include <file_helpers.hpp>
#include "installer_helper.hpp"
#include "../../settings_manager/settings_manager_impl.h"

const UINT COST_SERVICE_INSTALL = 2000;

bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description, std::wstring service_deps);
bool uninstall(msi_helper &h, std::wstring service_name);


void copy_file(msi_helper &h, std::wstring source, std::wstring target) {
	if (file_helpers::checks::exists(source)) {
		h.logMessage(_T("Copying: ") + source + _T(" to ") + target);
		if (!CopyFile(source.c_str(), target.c_str(), FALSE)) {
			h.errorMessage(_T("Failed to copy file: ") + error::lookup::last_error());
		}
	} else {
		h.logMessage(_T("Copying failed: ") + source + _T(" to ") + target + _T(" source was not found."));
	}

}

class installer_logger : public settings::logger_interface {
public:
	msi_helper *h;
	std::wstring error;
	std::list<std::wstring> debug_log;

	installer_logger(msi_helper *h) : h(h) {}

	virtual void err(std::string file, int line, std::wstring message) {
		error = message;
		h->logMessage(_T("ERROR: ") + message);
		debug_log.push_back(_T("ERROR: ") + message);
	}
	virtual void warn(std::string file, int line, std::wstring message) {
		h->logMessage(_T("WARN: ") + message);
		debug_log.push_back(_T("WARN: ") + message);
	}
	virtual void info(std::string file, int line, std::wstring message) {
		h->logMessage(_T("INFO: ") + message);
		debug_log.push_back(_T("INFO: ") + message);
	}
	virtual void debug(std::string file, int line, std::wstring message) {
		h->logMessage(_T("DEBUG: ") + message);
		debug_log.push_back(_T("DEBUG: ") + message);
	}
	std::list<std::wstring> get_debug() {
		return debug_log;
	}
};

struct installer_settings_provider : public settings_manager::provider_interface {

	msi_helper *h;
	std::wstring basepath;
	installer_logger logger;
	std::wstring old_settings_map;

	installer_settings_provider(msi_helper *h, std::wstring basepath, std::wstring old_settings_map) : h(h), logger(h), basepath(basepath), old_settings_map(old_settings_map) {}
	installer_settings_provider(msi_helper *h, std::wstring basepath) : h(h), logger(h), basepath(basepath) {}

	virtual std::wstring expand_path(std::wstring file) {
		strEx::replace(file, _T("${base-path}"), basepath);
		strEx::replace(file, _T("${exe-path}"), basepath);
		strEx::replace(file, _T("${shared-path}"), basepath);
		return file;
	}
	virtual void log_fatal_error(std::wstring error) {
		logger.err("", 0, error);
	}
	virtual settings::logger_interface* create_logger() {
		return &logger;
	}
	boolean has_error() {
		return !logger.error.empty();
	}
	std::wstring get_error() {
		return logger.error;
	}
	std::list<std::wstring> get_debug() {
		return logger.get_debug();
	}
	std::wstring get_data(std::wstring key) {
		if (!old_settings_map.empty() && key == _T("old_settings_map_data")) {
			return old_settings_map;
		}
		return _T("");
	}
};

std::wstring has_key(std::wstring path, std::wstring key) {
	return settings_manager::get_settings()->has_key(path, key)?_T("1"):_T("");
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

extern "C" UINT __stdcall ImportConfig(MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ImportConfig"));
	try {
		std::wstring target = h.getTargetPath(_T("INSTALLLOCATION"));
		std::wstring main = h.getPropery(_T("MAIN_CONFIGURATION_FILE"));
		std::wstring custom = h.getPropery(_T("CUSTOM_CONFIGURATION_FILE"));
		std::wstring allow = h.getPropery(_T("ALLOW_CONFIGURATION"));

		std::wstring tmpPath = h.getTempPath();

		std::wstring map_data = read_map_data(h);

		if (allow == _T("0")) {
			h.logMessage(_T("Configuration not allowed: ") + allow);
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
			h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
			h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));
			return ERROR_SUCCESS;
		}

 		if (!file_helpers::checks::exists(target)) {
 			h.logMessage(_T("Target folder not found: ") + target);
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
 			h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
			h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));
			return ERROR_SUCCESS;
		}

		boost::filesystem::wpath restore_path = h.getTempPath();
		restore_path = restore_path / (_T("old_nsc.ini"));
		boost::filesystem::wpath old_path = target;
		old_path = old_path / (_T("nsc.ini"));

		h.logMessage(_T("Looking for old settings file (for archiving): ") + old_path.string());
		h.logMessage(_T("Using restore path: ") + restore_path.string());
		if (boost::filesystem::exists(old_path)) {
			h.logMessage(_T("Found old file: ") + strEx::itos(boost::filesystem::file_size(old_path)));
			h.setProperty(_T("RESTORE_FILE"), restore_path.string());
			copy_file(h, old_path.string(), restore_path.string());
		}
		if (boost::filesystem::exists(restore_path))
			h.logMessage(_T("Found restore file: ") + strEx::itos(boost::filesystem::file_size(restore_path)));

		installer_settings_provider provider(&h, target, map_data);
		if (!settings_manager::init_settings(&provider, _T(""))) {
			h.logMessage(_T("Settings context had fatal errors"));
			h.setProperty(_T("CONF_OLD_ERROR"), provider.get_error());
			h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
			h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
			h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
		}
		if (provider.has_error()) {
			h.logMessage(_T("Settings context reported errors (debug log end)"));
			BOOST_FOREACH(std::wstring l, provider.get_debug()) {
				h.logMessage(l);
			}
			h.logMessage(_T("Settings context reported errors (debug log end)"));
			if (!settings_manager::has_boot_conf()) {
				h.logMessage(_T("boot.conf was NOT found (so no new configuration)"));
				if (settings_manager::context_exists(DEFAULT_CONF_OLD_LOCATION)) {
					h.logMessage(std::wstring(_T("Old configuration found: ")) + DEFAULT_CONF_OLD_LOCATION);
					h.setProperty(_T("CONF_OLD_ERROR"), std::wstring(_T("Old configuration (")) + DEFAULT_CONF_OLD_LOCATION + _T(") was found but we got errors accessing it: ") + provider.get_error());
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
				h.setProperty(_T("CONF_OLD_ERROR"), provider.get_error());
				h.setProperty(_T("CONF_CAN_CHANGE"), _T("0"));
				h.setProperty(_T("CONF_OLD_FOUND"), _T("0"));
				h.setProperty(_T("CONF_HAS_ERRORS"), _T("1"));
				return ERROR_SUCCESS;
			}
		}

		h.setProperty(_T("CONFIGURATION_TYPE"), settings_manager::get_settings()->get_context());
		h.logMessage(_T("CONFIGURATION_TYPE=") + settings_manager::get_settings()->get_context());
		h.logMessage(_T("CONFIGURATION_TYPE=") + settings_manager::get_settings()->get_info());
		h.setProperty(_T("CONF_CAN_CHANGE"), _T("1"));
		h.setProperty(_T("CONF_HAS_ERRORS"), _T("0"));

		h.setPropertyAndOld(_T("ALLOWED_HOSTS"), settings_manager::get_settings()->get_string(_T("/settings/default"), _T("allowed hosts"), _T("")));
		h.setPropertyAndOld(_T("NSCLIENT_PWD"), settings_manager::get_settings()->get_string(_T("/settings/default"), _T("password"), _T("")));

		std::wstring modpath = _T("/modules");
		h.setPropertyAndOld(_T("CONF_NRPE"), has_key(modpath, _T("NRPEServer")));
		h.setPropertyAndOld(_T("CONF_SCHEDULER"), has_key(modpath, _T("Scheduler")));
		h.setPropertyAndOld(_T("CONF_NSCA"), has_key(modpath, _T("NSCAClient")));
		h.setPropertyAndOld(_T("CONF_NSCLIENT"), has_key(modpath, _T("NSClientServer")));
		h.setPropertyAndOld(_T("CONF_WMI"), has_key(modpath, _T("CheckWMI")));

		if (settings_manager::get_settings()->has_key(modpath, _T("CheckSystem")) &&
			settings_manager::get_settings()->has_key(modpath, _T("CheckDisk")) &&
			settings_manager::get_settings()->has_key(modpath, _T("CheckEventLog")) &&
			settings_manager::get_settings()->has_key(modpath, _T("CheckHelpers")) &&
			settings_manager::get_settings()->has_key(modpath, _T("CheckExternalScripts")) &&
			settings_manager::get_settings()->has_key(modpath, _T("CheckNSCP"))
			) {
				h.setPropertyAndOld(_T("CONF_CHECKS"), _T("1"));
		} else {
			h.setPropertyAndOld(_T("CONF_CHECKS"), _T("0"));
		}
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


void write_changed_key(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring path, std::wstring key, std::wstring val = _T("")) {
	data.write_int(h.propertyTouched(prop)?1:2);
	data.write_string(path);
	data.write_string(key);
	if (val.empty())
		data.write_string(h.getPropery(prop));
	else
		data.write_string(val);
}

void write_key(msi_helper::custom_action_data_w &data, int mode, std::wstring path, std::wstring key, std::wstring val) {
	data.write_int(mode);
	data.write_string(path);
	data.write_string(key);
	data.write_string(val);
}

extern "C" UINT __stdcall ScheduleWriteConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ScheduleWriteConfig"));
	try {
		if (h.getPropery(_T("CONF_CAN_CHANGE")) != _T("1")) {
			return ERROR_SUCCESS;
		}
		msi_helper::custom_action_data_w data;
		data.write_string(h.getTargetPath(_T("INSTALLLOCATION")));
		data.write_string(h.getPropery(_T("CONFIGURATION_TYPE")));
		data.write_string(h.getPropery(_T("RESTORE_FILE")));
		data.write_int(h.getPropery(_T("ADD_DEFAULTS"))==_T("1")?1:0);

		std::wstring modpath = _T("/modules");
		std::wstring modval = _T("");
		write_changed_key(h, data, _T("CONF_NRPE"), modpath, _T("NRPEServer"), modval);
		write_changed_key(h, data, _T("CONF_SCHEDULER"), modpath, _T("Scheduler"), modval);
		write_changed_key(h, data, _T("CONF_NSCA"), modpath, _T("NSCAClient"), modval);
		write_changed_key(h, data, _T("CONF_NSCLIENT"), modpath, _T("NSClientServer"), modval);
		write_changed_key(h, data, _T("CONF_WMI"), modpath, _T("CheckWMI"), modval);

		if (h.propertyTouched(_T("CONF_CHECKS"))) {
			write_key(data, 1, modpath, _T("CheckSystem"), modval);
			write_key(data, 1, modpath, _T("CheckDisk"), modval);
			write_key(data, 1, modpath, _T("CheckEventLog"), modval);
			write_key(data, 1, modpath, _T("CheckHelpers"), modval);
			write_key(data, 1, modpath, _T("CheckExternalScripts"), modval);
			write_key(data, 1, modpath, _T("CheckNSCP"), modval);
		}

		std::wstring defpath = _T("/settings/default");
		write_changed_key(h, data, _T("ALLOWED_HOSTS"), defpath, _T("allowed hosts"));
		write_changed_key(h, data, _T("NSCLIENT_PWD"), defpath, _T("password"));

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
		std::wstring context = data.get_next_string();
		std::wstring restore = data.get_next_string();
		int add_defaults = data.get_next_int();

		h.logMessage(_T("Target: ") + target);
		h.logMessage(_T("Context: ") + context);
		h.logMessage(_T("Restore: ") + restore);

		boost::filesystem::wpath path = target;
		boost::filesystem::wpath old_path = path / _T("nsc.ini.old");
		path = path / _T("nsc.ini");

		boost::filesystem::wpath restore_path = restore;

		if (boost::filesystem::exists(old_path))
			h.logMessage(_T("Found old (.old) file: ") + strEx::itos(boost::filesystem::file_size(old_path)));
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Found old file: ") + strEx::itos(boost::filesystem::file_size(path)));
		if (boost::filesystem::exists(restore_path))
			h.logMessage(_T("Found restore file: ") + strEx::itos(boost::filesystem::file_size(restore_path)));

		if (boost::filesystem::exists(restore_path)) {
			if (!boost::filesystem::exists(path)) {
				h.logMessage(_T("Restoring nsc.ini configuration file"));
				copy_file(h, restore_path.string(), path.string());
			}
			if (!boost::filesystem::exists(old_path)) {
				h.logMessage(_T("Creating backup nsc.ini.old configuration file"));
				copy_file(h, restore_path.string(), old_path.string());
			}
		}

		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (001): ") + strEx::itos(boost::filesystem::file_size(path)));

		installer_settings_provider provider(&h, target);
		if (!settings_manager::init_settings(&provider, context)) {
			h.errorMessage(_T("Failed to boot settings: ") + provider.get_error());
			return ERROR_INSTALL_FAILURE;
		}
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (002): ") + strEx::itos(boost::filesystem::file_size(path)));

		h.logMessage(_T("Switching to: ") + context);
		settings_manager::change_context(context);
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (003): ") + strEx::itos(boost::filesystem::file_size(path)));

		while (data.has_more()) {
			unsigned int mode = data.get_next_int();
			std::wstring path = data.get_next_string();
			std::wstring key = data.get_next_string();
			std::wstring val = data.get_next_string();

			if (mode == 1) {
				h.logMessage(_T("Set key: ") + path + _T("/") + key + _T(" = ") + val);
				settings_manager::get_settings()->set_string(path, key, val);
			} else if (mode == 2) {
				h.logMessage(_T("***UNSUPPORTED*** Remove key: ") + path + _T("/") + key + _T(" = ") + val);
			} else {
				h.errorMessage(_T("Unknown mode in CA data: ") + strEx::itos(mode) + _T(": ") + data.to_string());
				return ERROR_INSTALL_FAILURE;
			}
		}
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (004): ") + strEx::itos(boost::filesystem::file_size(path)));
		settings_manager::get_settings()->save();
		if (boost::filesystem::exists(path))
			h.logMessage(_T("Size (005): ") + strEx::itos(boost::filesystem::file_size(path)));
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
					h.errorMessage(_T("Failed to start process: ") + error::lookup::last_error());
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