#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <error.hpp>
#include <Settings.h>
#include <config.h>
#include <ServiceCmd.h>
#include <char_buffer.hpp>
#include <file_helpers.hpp>
#include "installer_helper.hpp"

const UINT COST_SERVICE_INSTALL = 2000;

LPCWSTR vcsServiceQuery =
L"SELECT `ShortName`, `LongName`, `Description`, `Program`, `Attributes`, `Component_` FROM `Services`";
enum eServiceQuery { feqShortName = 1, feqLongName, feqDesc, feqProgram, feqAttributes, feqComponent };
enum eFirewallExceptionAttributes { feaIgnoreFailures = 1 };

bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description);
bool uninstall(msi_helper &h, std::wstring service_name);
UINT SchedServiceMgmt(__in MSIHANDLE hInstall, msi_helper::WCA_TODO todoSched);

extern "C" UINT __stdcall ScheduleInstallService(MSIHANDLE hInstall) {
	return SchedServiceMgmt(hInstall, msi_helper::WCA_TODO_INSTALL);
}
extern "C" UINT __stdcall ScheduleUnInstallService(MSIHANDLE hInstall) {
	return SchedServiceMgmt(hInstall, msi_helper::WCA_TODO_UNINSTALL);
}
extern "C" UINT __stdcall ExecServiceInstall(__in MSIHANDLE hInstall) {
	HRESULT hr = S_OK;
	// initialize
	msi_helper h(hInstall, _T("ExecServiceInstall"));
	try {
		msi_helper::custom_action_data_r data(h.getPropery(L"CustomActionData"));

		// loop through all the passed in data
		while (data.has_more()) {
			// extract the custom action data and if rolling back, swap INSTALL and UNINSTALL
			int iTodo = data.get_next_int();
			if (::MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK))
			{
				if (msi_helper::WCA_TODO_INSTALL == iTodo)
				{
					iTodo = msi_helper::WCA_TODO_UNINSTALL;
				}
				else if (msi_helper::WCA_TODO_UNINSTALL == iTodo)
				{
					iTodo = msi_helper::WCA_TODO_INSTALL;
				}
			}

			std::wstring shortname = data.get_next_string();
			std::wstring longname = data.get_next_string();
			std::wstring desc = data.get_next_string();
			int attr = data.get_next_int();
			BOOL fIgnoreFailures = feaIgnoreFailures == (attr & feaIgnoreFailures);
			std::wstring file = data.get_next_string();
			switch (iTodo) {
			case msi_helper::WCA_TODO_INSTALL:
			case msi_helper::WCA_TODO_REINSTALL:
				h.logMessage(_T("Installing service install: ") + shortname + _T(", ") + file);
				install(h, file, shortname, longname, desc);
				break;

			case msi_helper::WCA_TODO_UNINSTALL:
				h.logMessage(_T("Installing service install: ") + shortname + _T(", ") + file);
				uninstall(h, shortname);
				break;
			default:
				h.logMessage(_T("IGNORING service install: ") + shortname + _T(", ") + file);
			}
		}
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install service: ") + e.what());
		return ERROR_SUCCESS;
	} catch (...) {
		h.errorMessage(_T("Failed to install service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;

}


UINT SchedServiceMgmt(__in MSIHANDLE hInstall, msi_helper::WCA_TODO todoSched)
{
	msi_helper h(hInstall, _T("SchedServiceInstall"));
	try {
		int cFirewallExceptions = 0;
		h.logMessage(_T("SchedServiceInstall: ") + strEx::itos(todoSched));
		// anything to do?
		if (!h.table_exists(L"Services")) {
			h.logMessage(_T("Services table doesn't exist, so there are no services to configure."));
			return ERROR_SUCCESS;
		}

		// query and loop through all the firewall exceptions
		PMSIHANDLE hView = h.open_execute_view(vcsServiceQuery);
		if (h.isNull(hView)) {
			h.logMessage(_T("Failed to query service view!"));
			return ERROR_INSTALL_FAILURE;
		}

		msi_helper::custom_action_data_w custom_data;
		PMSIHANDLE hRec = h.fetch_record(hView);
		while (hRec != NULL)
		{
			std::wstring shortname = h.get_record_formatted_string(hRec, feqShortName);
			std::wstring longname = h.get_record_formatted_string(hRec, feqLongName);
			std::wstring desc = h.get_record_formatted_string(hRec, feqDesc);
			std::wstring program = h.get_record_formatted_string(hRec, feqProgram);
			int attributes = h.get_record_integer(hRec, feqAttributes);
			std::wstring component = h.get_record_string(hRec, feqComponent);

			// figure out what we're doing for this exception, treating reinstall the same as install
			msi_helper::WCA_TODO todoComponent = h.get_component_todo(component);
			if ((msi_helper::WCA_TODO_REINSTALL == todoComponent ? msi_helper::WCA_TODO_INSTALL : todoComponent) != todoSched) {
				h.logMessage(_T("Component '") + component + _T("' action state (") + strEx::itos(todoComponent) + _T(") doesn't match request (") + strEx::itos(todoSched) + _T(")"));
				hRec = h.fetch_record(hView);
				continue;
			}
			h.logMessage(_T("Adding data to CA chunk... "));
			// action :: name :: remoteaddresses :: attributes :: target :: {port::protocol | path}
			++cFirewallExceptions;
			custom_data.write_int(todoComponent);
			custom_data.write_string(shortname);
			custom_data.write_string(longname);
			custom_data.write_string(desc);
			custom_data.write_int(attributes);
			//custom_data.write_int(fetApplication);
			custom_data.write_string(program);
			h.logMessage(_T("Adding data to CA chunk... DONE"));
			h.logMessage(_T("CA chunk: ") + custom_data.to_string());
			hRec = h.fetch_record(hView);
		}
		// schedule ExecFirewallExceptions if there's anything to do
		if (custom_data.has_data()) {
			h.logMessage(_T("Scheduling (WixExecServiceInstall) firewall exception: ") + custom_data.to_string());
			if (msi_helper::WCA_TODO_INSTALL == todoSched) {
				HRESULT hr = h.do_deferred_action(L"WixRollbackServiceInstall", custom_data, cFirewallExceptions * COST_SERVICE_INSTALL);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule service install exceptions rollback"));
					return hr;
				}
				hr = h.do_deferred_action(L"WixExecServiceInstall", custom_data, cFirewallExceptions * COST_SERVICE_INSTALL);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule service install exceptions execution"));
					return hr;
				}
			}
			else
			{
				h.logMessage(_T("Scheduling (WixExecServiceUninstall) firewall exception: ") + custom_data.to_string());
				HRESULT hr = h.do_deferred_action(L"WixRollbackServiceUninstall", custom_data, cFirewallExceptions * COST_SERVICE_INSTALL);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule service install exceptions rollback"));
					return hr;
				}
				hr = h.do_deferred_action(L"WixExecServiceUninstall", custom_data, cFirewallExceptions * COST_SERVICE_INSTALL);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule service install exceptions execution"));
					return hr;
				}
			}
		} else
			h.logMessage(_T("No services scheduled"));
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to install service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//#pragma comment(linker, "/EXPORT:ImportConfig=_ImportConfig@4")
extern "C" UINT __stdcall ImportConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ImportConfig"));
	try {
		std::wstring target = h.getTargetPath(_T("INSTALLLOCATION"));
		std::wstring main = h.getPropery(_T("MAIN_CONFIGURATION_FILE"));
		std::wstring custom = h.getPropery(_T("CUSTOM_CONFIGURATION_FILE"));

		h.setupMyProperty(_T("ALLOWED_HOSTS"), MAIN_ALLOWED_HOSTS_DEFAULT);
		h.setupMyProperty(_T("NSCLIENT_PWD"), MAIN_SETTINGS_PWD_DEFAULT);
		h.setupMyProperty(_T("CONF_NRPE"), _T(""));
		h.setupMyProperty(_T("CONF_NSCLIENT"), _T(""));
		h.setupMyProperty(_T("CONF_NSCA"), _T(""));
		h.setupMyProperty(_T("CONF_CHECKS"), _T(""));
		h.setupMyProperty(_T("CONF_WMI"), _T(""));

		std::wstring filename = target + _T("\\") + main;
		if (!file_helpers::checks::exists(filename)) {
			h.setProperty(_T("CONF_CHECKS_GRAY"), _T(""));
			h.setProperty(_T("CONF_CAN_WRITE"), _T("1"));
			h.setProperty(_T("CONF_OLD_NOT_FOUND"), _T("1"));
			return ERROR_SUCCESS;
		}

		Settings::getInstance()->setFile(target, main);
		if (Settings::getInstance()->getActiveType() == _T("INI-file")) {
			h.setProperty(_T("CONF_CAN_WRITE"), _T("1"));
			CopyFile(filename.c_str(), (h.getTempPath() + _T("\\old_nsc.ini")).c_str(), FALSE);
		} else
			h.setProperty(_T("CONF_CAN_WRITE"), _T("0"));




		std::wstring allowed_hosts = Settings::getInstance()->getString(MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS, MAIN_ALLOWED_HOSTS_DEFAULT);
		h.setMyProperty(_T("ALLOWED_HOSTS"), allowed_hosts);

		std::wstring password = Settings::getInstance()->getString(MAIN_SECTION_TITLE, MAIN_SETTINGS_PWD, MAIN_SETTINGS_PWD_DEFAULT);
		h.setMyProperty(_T("NSCLIENT_PWD"), password);

		int found_common_checks = 0;
		settings_base::sectionList list = Settings::getInstance()->getSection(_T("modules"));
		for (settings_base::sectionList::const_iterator cit=list.begin(); cit!=list.end();++cit) {
			if (*cit == _T("NRPEListener.dll")) {
				h.setMyProperty(_T("CONF_NRPE"), _T("1"));
			}
			if (*cit == _T("NSClientListener.dll")) {
				h.setMyProperty(_T("CONF_NSCLIENT"), _T("1"));
			}
			if (*cit == _T("NSCAAgent.dll")) {
				h.setMyProperty(_T("CONF_NSCA"), _T("1"));
			}
			if (*cit == _T("CheckWMI.dll")) {
				h.setMyProperty(_T("CONF_WMI"), _T("1"));
			}
			if (
				(*cit == _T("FileLogger.dll")) ||
				(*cit == _T("CheckSystem.dll")) ||
				(*cit == _T("CheckDisk.dll")) ||
				(*cit == _T("CheckEventLog.dll")) ||
				(*cit == _T("CheckHelpers.dll"))
				) {
				found_common_checks++;
			}
		}
		if (found_common_checks == 5) {
			h.setMyProperty(_T("CONF_CHECKS"), _T("1"));
			h.setProperty(_T("CONF_CHECKS_GRAY"), _T(""));
		} else if (found_common_checks == 0) {
			h.setMyProperty(_T("CONF_CHECKS"), _T(""));
			h.setProperty(_T("CONF_CHECKS_GRAY"), _T(""));
		} else {
			h.setMyProperty(_T("CONF_CHECKS"), _T("1"));
			h.setProperty(_T("CONF_CHECKS_GRAY"), _T("1"));
		}
		Settings::destroyInstance();
		h.setProperty(_T("CONF_OLD_NOT_FOUND"), _T("0"));
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to read old configuration file: ") + e.what());
		h.setProperty(_T("INSCON_CONFIGURE"),_T("0"));
		h.setProperty(_T("CONF_CAN_WRITE"), _T("0"));
		return ERROR_INSTALL_FAILURE;
	} catch (SettingsException e) {
		h.errorMessage(_T("Failed to read old configuration file: ") + e.getMessage());
		h.setProperty(_T("INSCON_CONFIGURE"),_T("0"));
		h.setProperty(_T("CONF_CAN_WRITE"), _T("0"));
		return ERROR_SUCCESS;
	} catch (...) {
		h.errorMessage(_T("Failed to read old configuration file: <UNKNOWN EXCEPTION>"));
		h.setProperty(_T("INSCON_CONFIGURE"),_T("0"));
		h.setProperty(_T("CONF_CAN_WRITE"), _T("0"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}


bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description) {
	h.updateProgress(_T("Preparing to install service"), service_short_name);
	try {
		if (serviceControll::isStarted(service_short_name)) {
			h.updateProgress(_T("Stopping service"), service_short_name);
			serviceControll::Stop(service_short_name);
		}
		if (serviceControll::isInstalled(service_short_name)) {
			h.updateProgress(_T("Uninstalling service"), service_short_name);
			serviceControll::Uninstall(service_short_name);
		}
		h.updateProgress(_T("Installing service"), service_short_name);
		serviceControll::Install(service_short_name, service_long_name, SZDEPENDENCIES, SERVICE_WIN32_OWN_PROCESS, exe);
	} catch (const serviceControll::SCException& e) {
		h.errorMessage(_T("Failed to install service: ") + e.error_);
		return false;
	}
	try {
		serviceControll::SetDescription(service_short_name, service_description);
	} catch (const serviceControll::SCException& e) {
		h.errorMessage(_T("Failed to set description of service: ") + e.error_);
	}
	return true;
}

bool uninstall(msi_helper &h, std::wstring service_name) {
	h.updateProgress(_T("Preparing to uninstall service"), service_name);
	try {
		if (!serviceControll::isInstalled(service_name))
			return true;
		if (serviceControll::isStarted(service_name)) {
			h.updateProgress(_T("Stopping service"), service_name);
			serviceControll::Stop(service_name);
		}
		h.updateProgress(_T("Uninstalling service"), service_name);
		serviceControll::Uninstall(service_name);
	} catch (const serviceControll::SCException& e) {
		h.errorMessage(_T("Failed to uninstall service: ") + e.error_);
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool write_config(msi_helper &h, std::wstring path, std::wstring file);

extern "C" UINT __stdcall ScheduleWriteConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("ScheduleWriteConfig"));
	try {
		std::wstring target = h.getTargetPath(_T("INSTALLLOCATION"));
		std::wstring main_conf = h.getPropery(_T("MAIN_CONFIGURATION_FILE"));
		std::wstring custom_conf = h.getPropery(_T("CUSTOM_CONFIGURATION_FILE"));

		std::wstring write = target + _T("\\") + custom_conf;
		h.logMessage(_T("config file (update): ") + write);

		if (h.getPropery(_T("CONF_CAN_WRITE")) == _T("1")) {
			if (h.getPropery(_T("KEEP_WHICH_CONFIG")) == _T("OLD")) {
				CopyFile((target + _T("\\nsc.ini")).c_str(), (target + _T("\\nsc.new")).c_str(), FALSE);
				CopyFile((h.getTempPath() + _T("\\old_nsc.ini")).c_str(), (target + _T("\\nsc.ini")).c_str(), FALSE);
			} else {
				CopyFile((h.getTempPath() + _T("\\old_nsc.ini")).c_str(), (target + _T("\\nsc.old")).c_str(), FALSE);
			}
		}
		if (h.getPropery(_T("KEEP_WHICH_CONFIG")) == _T("NEW")) {
			if (!write_config(h, target, custom_conf)) {
				h.logMessage(_T("Failed to write configuration"));
				return ERROR_INSTALL_FAILURE;
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
		std::wstring path = data.get_next_string();
		std::wstring file = data.get_next_string();
		Settings::getInstance()->setFile(path, file, true);

		while (data.has_more()) {
			unsigned int mode = data.get_next_int();
			if (mode == 1) {
			// loop through all the passed in data
				std::wstring path = data.get_next_string();
				std::wstring key = data.get_next_string();
				std::wstring prop = data.get_next_string();
				h.logMessage(_T("Writing to config file: ") + path + _T("/") + key + _T(" = ") + prop);
				Settings::getInstance()->setString(path, key, prop);
				//TODO write config
			} else if (mode == 2) {
				std::list<std::wstring> list = data.get_next_list();
				h.logMessage(_T("Writing modules section: ") + strEx::itos(list.size()));
				Settings::getInstance()->writeSection(_T("modules"), list);
			} else {
				h.errorMessage(_T("Unknown mode in CA data: ") + strEx::itos(mode) + _T(": ") + data.to_string());
				return ERROR_INSTALL_FAILURE;
			}
		}
		Settings::getInstance()->write();
	} catch (SettingsException e) {
		h.errorMessage(_T("Failed to write configuration file: ") + e.getMessage());
		return ERROR_SUCCESS;
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to install service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}

inline void write_string_item_if_changed(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring property, std::wstring path, std::wstring key) {
	if (h.isChangedProperyAndOld(property)) {
		data.write_int(1);
		data.write_string(path);
		data.write_string(key);
		data.write_string(h.getPropery(property));
	} else
		h.logMessage(property + _T(" was not changed..."));
}
bool write_config(msi_helper &h, std::wstring path, std::wstring file) {
	std::wstring t;
	msi_helper::custom_action_data_w data;

	data.write_string(path);
	data.write_string(file);

	//cd_prop.write_int(1);
	if (h.getPropery(_T("CONF_CAN_WRITE")) != _T("1")) {
		h.updateProgress(_T("File is not writable (writing to registry not supported)"), file);
		return true;
	}

	//Settings::getInstance()->setFile(path, file, true);
	write_string_item_if_changed(h, data, _T("ALLOWED_HOSTS"), MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS);
	write_string_item_if_changed(h, data, _T("NSCLIENT_PWD"), MAIN_SECTION_TITLE, MAIN_SETTINGS_PWD);

	settings_base::sectionList list;
	if (h.isChangedProperyAndOld(_T("CONF_NRPE")))
		list.push_back(_T("NRPEListener.dll"));
	if (h.isChangedProperyAndOld(_T("CONF_NSCLIENT")))
		list.push_back(_T("NSClientListener.dll"));
	if (h.isChangedProperyAndOld(_T("CONF_NSCA")))
		list.push_back(_T("NSCAAgent.dll"));
	if (h.isChangedProperyAndOld(_T("CONF_WMI")))
		list.push_back(_T("CheckWMI.dll"));
	if (h.isChangedProperyAndOld(_T("CONF_CHECKS"))) {
		list.push_back(_T("FileLogger.dll"));
		list.push_back(_T("CheckSystem.dll"));
		list.push_back(_T("CheckDisk.dll"));
		list.push_back(_T("CheckEventLog.dll"));
		list.push_back(_T("CheckHelpers.dll"));
	}
	if (list.size() > 0) {
		data.write_int(2);
		data.write_list(list);
	}
	if (data.has_data()) {
		h.logMessage(_T("Scheduling (ExecWriteConfig): ") + data.to_string());
		HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
		if (FAILED(hr)) {
			h.errorMessage(_T("failed to schedule config update"));
			return hr;
		}
	}
}


bool start(msi_helper &h, std::wstring service_name = _T("")) {
	if (service_name.empty())
		service_name = SZSERVICENAME;
	if (!serviceControll::isInstalled(service_name)) {
		h.logMessage(_T("Service was not installed (so we cannot start it): ")+ service_name);
		return false;
	}

	h.updateProgress(_T("Preparing to start service"), service_name);
	try {
		if (!serviceControll::isStarted(service_name)) {
			h.updateProgress(_T("Starting service"), service_name);
			serviceControll::Start(service_name);
		}
	} catch (const serviceControll::SCException& e) {
		h.errorMessage(_T("Failed to start service: ") + e.error_);
		return false;
	}
	return true;
}

bool stop(msi_helper &h, std::wstring service_name = _T("")) {
	if (service_name.empty())
		service_name = SZSERVICENAME;
	h.updateProgress(_T("Preparing to stopp service"), service_name);
	try {
		if (serviceControll::isStarted(service_name)) {
			h.updateProgress(_T("Stopping service"), service_name);
			serviceControll::Stop(service_name);
		}
	} catch (const serviceControll::SCException& e) {
		h.errorMessage(_T("Failed to stop service: ") + e.error_);
		return false;
	}
	return true;
}


//#pragma comment(linker, "/EXPORT:UpdateConfig=_UpdateConfig@4")

extern "C" UINT __stdcall StartAllServices (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("StartService"));
	try {
		std::wstring val = h.getPropery(_T("START_SERVICE_ON_EXIT"));
		if (val == _T("1")) {
			// anything to do?
			if (h.table_exists(L"Services")) {
				PMSIHANDLE hView = h.open_execute_view(vcsServiceQuery);
				if (h.isNull(hView)) {
					h.logMessage(_T("Failed to query service view!"));
					return ERROR_INSTALL_FAILURE;
				}

				msi_helper::custom_action_data_w custom_data;
				PMSIHANDLE hRec = h.fetch_record(hView);
				while (hRec != NULL)
				{
					std::wstring shortname = h.get_record_formatted_string(hRec, feqShortName);
					std::wstring component = h.get_record_string(hRec, feqComponent);

					// figure out what we're doing for this exception, treating reinstall the same as install
					msi_helper::WCA_TODO todoComponent = h.get_component_todo(component);
					if ((msi_helper::WCA_TODO_REINSTALL == todoComponent ? msi_helper::WCA_TODO_INSTALL : todoComponent) != msi_helper::WCA_TODO_INSTALL) {
						h.logMessage(_T("Component '") + component + _T("' action state (") + strEx::itos(todoComponent) + _T(") doesn't match request (") + strEx::itos(msi_helper::WCA_TODO_INSTALL) + _T(")"));
						hRec = h.fetch_record(hView);
						continue;
					}
					try {
						if (!serviceControll::isStarted(shortname)) {
							h.updateProgress(_T("Starting service"), shortname);
							serviceControll::Start(shortname);
						}
					} catch (const serviceControll::SCException& e) {
						h.logMessage(_T("Failed to start service: ") + shortname + _T(": ") + e.error_);
					}
					hRec = h.fetch_record(hView);
				}
			}
		}
		val = h.getPropery(_T("DONATE_ON_EXIT"));
		if (val == _T("1")) {
			long r = (long)ShellExecute(NULL, _T("open"), _T("https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=michael@medin.name&item_name=Fans+of+NSClient%2B%2B&item_number=Installer+Campaign&amount=10%2e00&currency_code=EUR&return=http%3A//nsclient.org"), NULL, NULL, SW_SHOWNORMAL);
			if (r > 32)
				return ERROR_SUCCESS;
			h.errorMessage(_T("Failed to start web browser for donations..."));
			return ERROR_INSTALL_FAILURE;
		}
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to process finalizing stuff: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to process finalizing stuff: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}


extern "C" UINT __stdcall StopAllServices (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("StopAllServices"));
	try {

		// anything to do?
		if (!h.table_exists(L"Services")) {
			h.logMessage(_T("Services table doesn't exist, so there are no services to configure."));
			return ERROR_SUCCESS;
		}

		// query and loop through all the firewall exceptions
		PMSIHANDLE hView = h.open_execute_view(vcsServiceQuery);
		if (h.isNull(hView)) {
			h.logMessage(_T("Failed to query service view!"));
			return ERROR_INSTALL_FAILURE;
		}

		msi_helper::custom_action_data_w custom_data;
		PMSIHANDLE hRec = h.fetch_record(hView);
		while (hRec != NULL)
		{
			std::wstring shortname = h.get_record_formatted_string(hRec, feqShortName);
			std::wstring component = h.get_record_string(hRec, feqComponent);

			// figure out what we're doing for this exception, treating reinstall the same as install
			msi_helper::WCA_TODO todoComponent = h.get_component_todo(component);
			if (todoComponent == msi_helper::WCA_TODO_REINSTALL)
				todoComponent = msi_helper::WCA_TODO_INSTALL;
			h.logMessage(_T("Component '") + component + _T("' action state (") + strEx::itos(todoComponent));
			if (todoComponent != msi_helper::WCA_TODO_INSTALL && todoComponent != msi_helper::WCA_TODO_UNINSTALL) {
				h.logMessage(_T("Component '") + component + _T("' action state (") + strEx::itos(todoComponent) + _T(") doesn't match request (IN/UN/RE)"));
				hRec = h.fetch_record(hView);
				continue;
			}
			try {
				if (serviceControll::isStarted(shortname)) {
					h.updateProgress(_T("Stopping service"), shortname);
					serviceControll::Stop(shortname);
				}
			} catch (const serviceControll::SCException& e) {
				h.logMessage(_T("Failed to stop service: ") + shortname + _T(": ") + e.error_);
			}
			hRec = h.fetch_record(hView);
		}
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to stop service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to stop service: <UNKNOWN EXCEPTION>"));
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
				TCHAR *cmd = new TCHAR[command.length()+1];
				wcsncpy_s(cmd, command.length()+1, command.c_str(), command.length());
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