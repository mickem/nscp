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


bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name = _T(""), std::wstring service_long_name = _T(""), std::wstring service_description = _T("")) {
	if (service_short_name.empty())
		service_short_name = SZSERVICENAME;
	if (service_long_name.empty())
		service_long_name = SZSERVICEDISPLAYNAME;
	if (service_description.empty())
		service_description = SZSERVICEDISPLAYNAME;
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

bool uninstall(msi_helper &h, std::wstring service_name = _T("")) {
	if (service_name.empty())
		service_name = SZSERVICENAME;
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

inline void write_string_item_if_changed(msi_helper &h, std::wstring property, std::wstring path, std::wstring key) {
	if (h.isChangedProperyAndOld(property))
		Settings::getInstance()->setString(path, key, h.getPropery(property));
	else
		h.logMessage(property + _T(" was not changed..."));
}
bool write_config(msi_helper &h, std::wstring path, std::wstring file) {
	std::wstring t;
	try {
		if (h.getPropery(_T("CONF_CAN_WRITE")) != _T("1")) {
			h.updateProgress(_T("File is not writable (writing to registry not supported)"), file);
			return true;
		}
		Settings::getInstance()->setFile(path, file, true);
		write_string_item_if_changed(h, _T("ALLOWED_HOSTS"), MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS);
		write_string_item_if_changed(h, _T("NSCLIENT_PWD"), MAIN_SECTION_TITLE, MAIN_SETTINGS_PWD);

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
		if (!list.empty())
			Settings::getInstance()->writeSection(_T("modules"), list);
		Settings::getInstance()->write();
	} catch (SettingsException e) {
		h.errorMessage(_T("Failed to write new configuration file: ") + file + _T(": ") + e.getMessage());
		return ERROR_SUCCESS;
	}
	return true;
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


//#pragma comment(linker, "/EXPORT:UninstallService=_UninstallService@4")
extern "C" UINT __stdcall UninstallService (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("UninstallService"));
	try {
		h.logMessage(_T("Remove mode is: ") + h.getPropery(_T("REMOVE")));
		h.logMessage(_T("mime mode is: ") + h.getPropery(_T("REMOVE_MIME")));
		h.setProperty(_T("REMOVE_MIME"), _T("test"));

		h.startProgress(10000, 2*10000, _T("Removing service: [2] ([1])..."), _T("Removing service: [2] ([1])... (X)"));
		if (!uninstall(h))
			return ERROR_INSTALL_FAILURE;
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to install service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}

//#pragma comment(linker, "/EXPORT:UpdateConfig=_UpdateConfig@4")
extern "C" UINT __stdcall UpdateConfig (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("UpdateConfig"));
	try {
		std::wstring target = h.getTargetPath(_T("INSTALLLOCATION"));
		std::wstring svc_exe = h.getPropery(_T("INSTALL_SVC_EXE"));
		std::wstring svc_sname = h.getPropery(_T("INSTALL_SVC_SHORTNAME"));
		std::wstring svc_lname = h.getPropery(_T("INSTALL_SVC_LONGNAME"));
		std::wstring svc_desc = h.getPropery(_T("INSTALL_SVC_DESC"));

		std::wstring main_conf = h.getPropery(_T("MAIN_CONFIGURATION_FILE"));
		std::wstring custom_conf = h.getPropery(_T("CUSTOM_CONFIGURATION_FILE"));

		std::wstring write = target + _T("\\") + custom_conf;
		h.logMessage(_T("config file (update): ") + write);

		h.logMessage(_T("service: ") + svc_exe);
		h.logMessage(_T("service short name: ") + svc_sname);
		h.logMessage(_T("service long name: ") + svc_lname);
		h.logMessage(_T("service desc: ") + svc_desc);

		std::wstring filename = target + _T("\\") + svc_exe;
		h.logMessage(_T("service exe: ") + filename);

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

		h.startProgress(10000, 2*10000, _T("Seting up service: [2] ([1])..."), _T("Seting up service: [2] ([1])... (X)"));
		if (!install(h, filename, svc_sname, svc_lname, svc_desc)) {
			h.logMessage(_T("service installtion failed"));
			return ERROR_INSTALL_FAILURE;
		}
		h.logMessage(_T("service installed"));
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to install service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}

extern "C" UINT __stdcall StartTheService (MSIHANDLE hInstall) {
	msi_helper h(hInstall, _T("StartService"));
	try {
		h.logMessage(_T("Remove mode is: ") + h.getPropery(_T("REMOVE")));
		h.logMessage(_T("mime mode is: ") + h.getPropery(_T("REMOVE_MIME")));
		h.logMessage(_T("START_SERVICE_ON_EXIT mode is: ") + h.getPropery(_T("START_SERVICE_ON_EXIT")));
		if (h.getPropery(_T("REMOVE")) == _T("ALL")) {
			h.logMessage(_T("Uninstalling so we wont remove anything..."));
			return ERROR_SUCCESS;
		}
		std::wstring svc_sname = h.getPropery(_T("INSTALL_SVC_SHORTNAME"));
		h.logMessage(_T("service short name: ") + svc_sname);

		std::wstring val = h.getPropery(_T("START_SERVICE_ON_EXIT"));
		if (val == _T("1")) {
			h.startProgress(10000, 2*10000, _T("Seting up service: [2] ([1])..."), _T("Seting up service: [2] ([1])... (X)"));
			if (!start(h, svc_sname)) {
				h.logMessage(_T("service failed to start"));
				// Ignore this error due to various bugs and problems...
				//return ERROR_INSTALL_FAILURE;
			}
			h.logMessage(_T("service started"));
		}

		val = h.getPropery(_T("DONATE_ON_EXIT"));
		if (val == _T("1")) {
			long r = (long)ShellExecute(NULL, _T("open"), _T("https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=michael@medin.name&item_name=Fans+of+NSClient%2B%2B&item_number=Installer+Campaign&amount=10%2e00&currency_code=EUR&return=http%3A//nsclient.org"), NULL, NULL, SW_SHOWNORMAL);
			if (r > 32)
				return ERROR_SUCCESS;
			msi_helper h(hInstall, _T("Donate"));
			h.errorMessage(_T("Failed to start web browser..."));
			return ERROR_INSTALL_FAILURE;
		}
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to start service: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to start service: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}
