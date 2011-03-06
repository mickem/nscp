//-------------------------------------------------------------------------------------------------
// <copyright file="firewall.cpp" company="Microsoft">
//    Copyright (c) Microsoft Corporation.  All rights reserved.
//    
//    The use and distribution terms for this software are covered by the
//    Common Public License 1.0 (http://opensource.org/licenses/cpl.php)
//    which can be found in the file CPL.TXT at the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by
//    the terms of this license.
//    
//    You must not remove this notice, or any other, from this software.
// </copyright>
// 
// <summary>
//    Firewall custom action code.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"
#include "../main_dll/installer_helper.hpp"
#include <atlbase.h>

LPCWSTR vcsFirewallExceptionQuery =
    L"SELECT `Name`, `RemoteAddresses`, `Port`, `Protocol`, `Program`, `Attributes`, `Component_` FROM `WixFirewallException`";
enum eFirewallExceptionQuery { feqName = 1, feqRemoteAddresses, feqPort, feqProtocol, feqProgram, feqAttributes, feqComponent };
enum eFirewallExceptionTarget { fetPort = 1, fetApplication, fetUnknown };
enum eFirewallExceptionAttributes { feaIgnoreFailures = 1 };



/******************************************************************
 SchedFirewallExceptions - immediate custom action worker to 
   register and remove firewall exceptions.

********************************************************************/
static UINT SchedFirewallExceptions(__in MSIHANDLE hInstall, msi_helper::WCA_TODO todoSched)
{
	msi_helper h(hInstall, _T("SchedFirewallExceptions"));
	try {
		int cFirewallExceptions = 0;
		h.logMessage(_T("SchedFirewallExceptions: ") + strEx::itos(todoSched));

		// anything to do?
		if (!h.table_exists(L"WixFirewallException")) {
			h.logMessage(_T("WixFirewallException table doesn't exist, so there are no firewall exceptions to configure."));
			return ERROR_SUCCESS;
		}

		// query and loop through all the firewall exceptions
		PMSIHANDLE hView = h.open_execute_view(vcsFirewallExceptionQuery);
		if (h.isNull(hView)) {
			h.logMessage(_T("WixFirewallException!"));
			return ERROR_INSTALL_FAILURE;
		}

		msi_helper::custom_action_data_w custom_data;
		PMSIHANDLE hRec = h.fetch_record(hView);
		while (hRec != NULL)
		{
			std::wstring name = h.get_record_formatted_string(hRec, feqName);
			std::wstring remoteAddress = h.get_record_formatted_string(hRec, feqRemoteAddresses);
			int port = h.get_record_formatted_integer(hRec, feqPort);
			int protocol = h.get_record_integer(hRec, feqProtocol);
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
			custom_data.write_string(name);
			custom_data.write_string(remoteAddress);
			custom_data.write_int(attributes);
			if (MSI_NULL_INTEGER == port || MSI_NULL_INTEGER == protocol) {
				// without port and protocol, we have an application exception.
				custom_data.write_int(fetApplication);
				custom_data.write_string(program);
			} else {
				// we have a port-based exception
				custom_data.write_int(fetPort);
				custom_data.write_int(port);
				custom_data.write_int(protocol);
			}
			h.logMessage(_T("Adding data to CA chunk... DONE"));
			h.logMessage(_T("CA chunk: ") + custom_data.to_string());
			hRec = h.fetch_record(hView);
		}
		// schedule ExecFirewallExceptions if there's anything to do
		if (custom_data.has_data()) {
			h.logMessage(_T("Scheduling (WixExecFirewallExceptionsInstall) firewall exception: ") + custom_data.to_string());
			if (msi_helper::WCA_TODO_INSTALL == todoSched) {
				HRESULT hr = h.do_deferred_action(L"WixRollbackFirewallExceptionsInstall", custom_data, cFirewallExceptions * COST_FIREWALL_EXCEPTION);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule firewall install exceptions rollback"));
					return hr;
				}
				hr = h.do_deferred_action(L"WixExecFirewallExceptionsInstall", custom_data, cFirewallExceptions * COST_FIREWALL_EXCEPTION);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule firewall install exceptions execution"));
					return hr;
				}
			}
			else
			{
				h.logMessage(_T("Scheduling (WixExecFirewallExceptionsUninstall) firewall exception: ") + custom_data.to_string());
				HRESULT hr = h.do_deferred_action(L"WixRollbackFirewallExceptionsUninstall", custom_data, cFirewallExceptions * COST_FIREWALL_EXCEPTION);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule firewall install exceptions rollback"));
					return hr;
				}
				hr = h.do_deferred_action(L"WixExecFirewallExceptionsUninstall", custom_data, cFirewallExceptions * COST_FIREWALL_EXCEPTION);
				if (FAILED(hr)) {
					h.errorMessage(_T("failed to schedule firewall install exceptions execution"));
					return hr;
				}
			}
		} else
			h.logMessage(_T("No firewall exceptions scheduled"));
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install firewall exception: ") + e.what());
		return ERROR_INSTALL_FAILURE;
	} catch (...) {
		h.errorMessage(_T("Failed to install firewall exception: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}

/******************************************************************
 SchedFirewallExceptionsInstall - immediate custom action entry
   point to register firewall exceptions.

********************************************************************/

extern "C" UINT __stdcall SchedFirewallExceptionsInstall(__in MSIHANDLE hInstall)
{
	return SchedFirewallExceptions(hInstall, msi_helper::WCA_TODO_INSTALL);
}


/******************************************************************
 SchedFirewallExceptionsUninstall - immediate custom action entry
   point to remove firewall exceptions.

********************************************************************/

extern "C" UINT __stdcall SchedFirewallExceptionsUninstall(__in MSIHANDLE hInstall)
{
    return SchedFirewallExceptions(hInstall, msi_helper::WCA_TODO_UNINSTALL);
}


#define ReleaseNullObject(x) if (x) { (x)->Release(); x = NULL; }


/******************************************************************
 GetFirewallProfile - get the active firewall profile as an
   INetFwProfile, which owns the lists of exceptions we're 
   updating.
********************************************************************/
HRESULT GetFirewallProfile(__in BOOL fIgnoreFailures, CComPtr<INetFwProfile> &pfwProfile)
{
	CComPtr< INetFwMgr > pfwMgr = NULL;
	CComPtr< INetFwPolicy > pfwPolicy = NULL;
	HRESULT hr = CoCreateInstance(__uuidof(NetFwMgr), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwMgr), reinterpret_cast< void** >( &pfwMgr ) );
	if (FAILED(hr))
		throw installer_exception(_T("CoCreateInstance for NetFwMgr failed!") + error::format::from_system(hr));
	hr = pfwMgr->get_LocalPolicy(&pfwPolicy);
	if (FAILED(hr))
		throw installer_exception(_T("get_LocalPolicy failed: ") + error::format::from_system(hr));
	hr = pfwPolicy->get_CurrentProfile(&pfwProfile);
	if (FAILED(hr))
		throw installer_exception(_T("get_LocalPolicy failed: ") + error::format::from_system(hr));
}

/******************************************************************
 AddApplicationException

********************************************************************/

void AddApplicationException(std::wstring file, std::wstring name, std::wstring remoteAddresses, BOOL fIgnoreFailures) {
    HRESULT hr = S_OK;
    CComBSTR bstrFile(file.c_str());
    CComBSTR bstrName(name.c_str());
    CComBSTR bstrRemoteAddresses(remoteAddresses.c_str());
    CComPtr<INetFwProfile> pfwProfile;
    CComPtr<INetFwAuthorizedApplications> pfwApps;

    // get the firewall profile, which is our entry point for adding exceptions
    GetFirewallProfile(fIgnoreFailures, pfwProfile);

    // first, let's see if the app is already on the exception list
    hr = pfwProfile->get_AuthorizedApplications(&pfwApps);
	if (FAILED(hr))
		throw installer_exception(_T("get_AuthorizedApplications failed: ") + error::format::from_system(hr));

    // try to find it (i.e., support reinstall)
	CComPtr<INetFwAuthorizedApplication> pfwApp;
    hr = pfwApps->Item(bstrFile, &pfwApp);
    if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr)
    {
        // not found, so we get to add it
        hr = ::CoCreateInstance(__uuidof(NetFwAuthorizedApplication), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwAuthorizedApplication), reinterpret_cast<void**>(&pfwApp));
		if (FAILED(hr))
			throw installer_exception(_T("NetFwAuthorizedApplication failed: ") + error::format::from_system(hr));

        // set the display name
        hr = pfwApp->put_Name(bstrName);
		if (FAILED(hr))
			throw installer_exception(_T("put_Name failed: ") + error::format::from_system(hr));

        // set path
        hr = pfwApp->put_ProcessImageFileName(bstrFile);
		if (FAILED(hr))
			throw installer_exception(_T("put_ProcessImageFileName failed: ") + error::format::from_system(hr));

        // set the allowed remote addresses
        if (remoteAddresses.length() > 0)
        {
            hr = pfwApp->put_RemoteAddresses(bstrRemoteAddresses);
			if (FAILED(hr))
				throw installer_exception(_T("put_RemoteAddresses failed: ") + error::format::from_system(hr) + _T(" '") + remoteAddresses + _T("'"));
        }

        // add it to the list of authorized apps
        hr = pfwApps->Add(pfwApp);
		if (FAILED(hr))
			throw installer_exception(_T("Failed to add authorized application: ") + error::format::from_system(hr));
	} else if (SUCCEEDED(hr)) {
		// we found an existing app exception (if we succeeded, that is)
		// enable it (just in case it was disabled)
		pfwApp->put_Enabled(VARIANT_TRUE);
	} else {
		throw installer_exception(_T("Item error: ") + error::format::from_system(hr));
    }
}

/******************************************************************
 RemoveApplicationException

********************************************************************/
static HRESULT RemoveApplicationException( std::wstring file, BOOL fIgnoreFailures) {
	CComBSTR bstrFile(file.c_str());
	CComPtr<INetFwProfile> pfwProfile;
	CComPtr<INetFwAuthorizedApplications> pfwApps;

	// get the firewall profile, which is our entry point for adding exceptions
	GetFirewallProfile(fIgnoreFailures, pfwProfile);

	
    // now get the list of app exceptions and remove the one
	HRESULT hr = pfwProfile->get_AuthorizedApplications(&pfwApps);
	if (FAILED(hr))
		throw installer_exception(_T("get_AuthorizedApplications failed: ") + error::format::from_system(hr));

    pfwApps->Remove(bstrFile);
}

/******************************************************************
 AddPortException

********************************************************************/
/*
static HRESULT AddPortException(
    __in LPCWSTR wzName,
    __in_opt LPCWSTR wzRemoteAddresses,
    __in BOOL fIgnoreFailures,
    __in int iPort,
    __in int iProtocol
    )
{
    HRESULT hr = S_OK;
    BSTR bstrName = NULL;
    BSTR bstrRemoteAddresses = NULL;
    INetFwProfile* pfwProfile = NULL;
    INetFwOpenPorts* pfwPorts = NULL;
    INetFwOpenPort* pfwPort = NULL;

    // convert to BSTRs to make COM happy
    bstrName = ::SysAllocString(wzName);
    ExitOnNull(bstrName, hr, E_OUTOFMEMORY, "failed SysAllocString for name");
    bstrRemoteAddresses = ::SysAllocString(wzRemoteAddresses);
    ExitOnNull(bstrRemoteAddresses, hr, E_OUTOFMEMORY, "failed SysAllocString for remote addresses");

    // create and initialize a new open port object
    hr = ::CoCreateInstance(__uuidof(NetFwOpenPort), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwOpenPort), reinterpret_cast<void**>(&pfwPort));
    ExitOnFailure(hr, "failed to create new open port");

    hr = pfwPort->put_Port(iPort);
    ExitOnFailure(hr, "failed to set exception port");

    hr = pfwPort->put_Protocol(static_cast<NET_FW_IP_PROTOCOL>(iProtocol));
    ExitOnFailure(hr, "failed to set exception protocol");

    if (bstrRemoteAddresses && *bstrRemoteAddresses)
    {
        hr = pfwPort->put_RemoteAddresses(bstrRemoteAddresses);
        ExitOnFailure1(hr, "failed to set exception remote addresses '%S'", bstrRemoteAddresses);
    }

    hr = pfwPort->put_Name(bstrName);
    ExitOnFailure(hr, "failed to set exception name");

    // get the firewall profile, its current list of open ports, and add ours
    hr = GetFirewallProfile(fIgnoreFailures, &pfwProfile);
    ExitOnFailure(hr, "failed to get firewall profile");
    if (S_FALSE == hr) // user or package author chose to ignore missing firewall
    {
        ExitFunction();
    }

    hr = pfwProfile->get_GloballyOpenPorts(&pfwPorts);
    ExitOnFailure(hr, "failed to get open ports");

    hr = pfwPorts->Add(pfwPort);
    ExitOnFailure(hr, "failed to add exception to global list");

LExit:
    ReleaseBSTR(bstrRemoteAddresses);
    ReleaseBSTR(bstrName);
    ReleaseObject(pfwProfile);
    ReleaseObject(pfwPorts);
    ReleaseObject(pfwPort);

    return fIgnoreFailures ? S_OK : hr;
}*/

/******************************************************************
 RemovePortException

********************************************************************/
/*
static HRESULT RemovePortException(
    __in int iPort,
    __in int iProtocol,
    __in BOOL fIgnoreFailures
    )
{
    HRESULT hr = S_OK;
    INetFwProfile* pfwProfile = NULL;
    INetFwOpenPorts* pfwPorts = NULL;

    // get the firewall profile, which is our entry point for adding exceptions
    hr = GetFirewallProfile(fIgnoreFailures, &pfwProfile);
    ExitOnFailure(hr, "failed to get firewall profile");
    if (S_FALSE == hr) // user or package author chose to ignore missing firewall
    {
        ExitFunction();
    }

    hr = pfwProfile->get_GloballyOpenPorts(&pfwPorts);
    ExitOnFailure(hr, "failed to get open ports");

    hr = pfwPorts->Remove(iPort, static_cast<NET_FW_IP_PROTOCOL>(iProtocol));
    ExitOnFailure2(hr, "failed to remove open port %d, protocol %d", iPort, iProtocol);

LExit:
    return fIgnoreFailures ? S_OK : hr;
}
*/


/******************************************************************
 ExecFirewallExceptions - deferred custom action entry point to 
   register and remove firewall exceptions.

********************************************************************/
extern "C" UINT __stdcall ExecFirewallExceptions(__in MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    // initialize
	msi_helper h(hInstall, _T("ExecFirewallExceptions"));
	try {

	msi_helper::custom_action_data_r data(h.getPropery(L"CustomActionData"));

    hr = ::CoInitialize(NULL);
	if (FAILED(hr)) {
		h.errorMessage(_T("Failed to initialize COM"));
		return ERROR_INSTALL_FAILURE;
	}

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

		std::wstring name = data.get_next_string();
		std::wstring remote_addr = data.get_next_string();
		int attr = data.get_next_int();
        BOOL fIgnoreFailures = feaIgnoreFailures == (attr & feaIgnoreFailures);

        int target = data.get_next_int();
        switch (target) {
			/*
        case fetPort:
            hr = WcaReadIntegerFromCaData(&pwz, &iPort);
            ExitOnFailure(hr, "failed to read port from custom action data");
            hr = WcaReadIntegerFromCaData(&pwz, &iProtocol);
            ExitOnFailure(hr, "failed to read protocol from custom action data");

            switch (iTodo)
            {
            case WCA_TODO_INSTALL:
            case WCA_TODO_REINSTALL:
                WcaLog(LOGMSG_STANDARD, "Installing firewall exception %S on port %d, protocol %d", pwzName, iPort, iProtocol);
                hr = AddPortException(pwzName, pwzRemoteAddresses, fIgnoreFailures, iPort, iProtocol);
                ExitOnFailure3(hr, "failed to add/update port exception for name '%S' on port %d, protocol %d", pwzName, iPort, iProtocol);
                break;

            case WCA_TODO_UNINSTALL:
                WcaLog(LOGMSG_STANDARD, "Uninstalling firewall exception %S on port %d, protocol %d", pwzName, iPort, iProtocol);
                hr = RemovePortException(iPort, iProtocol, fIgnoreFailures);
                ExitOnFailure3(hr, "failed to remove port exception for name '%S' on port %d, protocol %d", pwzName, iPort, iProtocol);
                break;
            }
            break;
*/
        case fetApplication:
			{
				std::wstring file = data.get_next_string();
				switch (iTodo)
				{
				case msi_helper::WCA_TODO_INSTALL:
				case msi_helper::WCA_TODO_REINSTALL:
					h.logMessage(_T("Installing firewall exception: ") + name + _T(", ") + file);
					AddApplicationException(file, name, remote_addr, fIgnoreFailures);
					break;

				case msi_helper::WCA_TODO_UNINSTALL:
					h.logMessage(_T("Uninstalling firewall exception: ") + name + _T(", ") + file);
					RemoveApplicationException(file, fIgnoreFailures);
					break;
				default:
					h.logMessage(_T("IGNORING firewall exception: ") + name + _T(", ") + file);
				}
			}
            break;
		default:
			h.logMessage(_T("IGNORING (target) firewall exception: ") + name + _T(" Target is: ") + strEx::itos(target));
        }
    }
    ::CoUninitialize();
	} catch (installer_exception e) {
		h.errorMessage(_T("Failed to install firewall exception: ") + e.what());
		return ERROR_SUCCESS;
	} catch (...) {
		h.errorMessage(_T("Failed to install firewall exception: <UNKNOWN EXCEPTION>"));
		return ERROR_INSTALL_FAILURE;
	}
	return ERROR_SUCCESS;
}
