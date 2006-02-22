#include "StdAfx.h"
#include ".\wmiquery.h"

#include <objidl.h>
#include <Wbemidl.h>
#include <map>

WMIQuery::WMIQuery(void) : bInitialized(false)
{
}

WMIQuery::~WMIQuery(void)
{
	if (bInitialized)
		unInitialize();
}


bool WMIQuery::initialize()
{
	if (CoInitialize(NULL) != S_OK)
		return false;
	bInitialized = true;
	if(CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,0,0) != S_OK) {
		return false;
	}
	return true;
}
void WMIQuery::unInitialize()
{
	CoUninitialize();
	bInitialized = false;
}


std::map<std::string,int> WMIQuery::execute(std::string query)
{
	std::map<std::string,int> ret;
	IWbemLocator * pIWbemLocator = NULL;
	BSTR bstrNamespace = (L"root\\cimv2");
	//BSTR bstrNamespace = (L"root\\default");
	HRESULT hRes = CoCreateInstance(CLSID_WbemAdministrativeLocator,NULL,CLSCTX_INPROC_SERVER|CLSCTX_LOCAL_SERVER, 
		IID_IUnknown,(void**)&pIWbemLocator);
	if (FAILED(hRes)) {
		throw WMIException("CoCreateInstance for CLSID_WbemAdministrativeLocator failed!", hRes);
	}
	IWbemServices * pWbemServices = NULL;
	hRes = pIWbemLocator->ConnectServer(bstrNamespace,NULL,NULL,NULL,0,NULL,NULL,&pWbemServices);
	if (FAILED(hRes)) {
		pIWbemLocator->Release();
		pIWbemLocator = NULL;
		throw WMIException("ConnectServer failed!", hRes);
	}
	CComBSTR strQuery(query.c_str());
	BSTR strQL = (L"WQL");
	IEnumWbemClassObject * pEnumObject = NULL;
	hRes = pWbemServices->ExecQuery(strQL, strQuery,WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&pEnumObject);
	if (FAILED(hRes)) {
		pWbemServices->Release();
		pIWbemLocator->Release();
		pIWbemLocator = NULL;
		throw WMIException("ExecQuery failed:" + query, hRes);
	}
	hRes = pEnumObject->Reset();
	if (FAILED(hRes)) {
		pWbemServices->Release();
		pIWbemLocator->Release();
		pIWbemLocator = NULL;
		throw WMIException("ExecQuery failed:" + query, hRes);
	}
	ULONG uCount = 1, uReturned;
	IWbemClassObject * pClassObject = NULL;
	hRes = pEnumObject->Next(WBEM_INFINITE,uCount, &pClassObject, &uReturned);
	if (FAILED(hRes)) {
		pWbemServices->Release();
		pIWbemLocator->Release();
		pIWbemLocator = NULL;
		throw WMIException("ExecQuery failed!" + query, hRes);
	}


	SAFEARRAY* pstrNames;
	hRes = pClassObject->GetNames(NULL,WBEM_FLAG_ALWAYS|WBEM_FLAG_NONSYSTEM_ONLY,NULL,&pstrNames);
	if (FAILED(hRes)) {
		pClassObject->Release();
		pWbemServices->Release();
		pIWbemLocator->Release();
		throw WMIException("GetNames failed!" + query, hRes);
	}
	CComSafeArray<BSTR> arr = pstrNames;
	long index = 0, begin, end;
	begin = arr.GetLowerBound();
	end = arr.GetUpperBound();
	for ( index = begin; index <= end; index++ ) {
		BSTR bStr = arr.GetAt(index);
		CString str = bStr;
		std::string std_str = str;
		CComVariant vValue;
		hRes = pClassObject->Get(bStr, 0, &vValue, 0, 0);
		if (vValue.vt == VT_INT) {
			ret[std_str] = vValue.intVal;
			//std::cout << (LPCTSTR)str << " = (INT) " << vValue.intVal << std::endl;
		} else if (vValue.vt == VT_I4) {
			ret[std_str] = vValue.lVal;
			//std::cout << (LPCTSTR)str << " = (I4) " << vValue.lVal << std::endl;
		} else if (vValue.vt == VT_UINT) {
			ret[std_str] = vValue.uintVal;
			//std::cout << (LPCTSTR)str << " = (UINT) " << vValue.uintVal << std::endl;
		} else if (vValue.vt == VT_BSTR) {
			std::cout << (LPCTSTR)str << " = UNSUPPORTED (BSTR)" << std::endl;
			CString val = vValue;
			//ret[std_str] = std::string(val);
		} else {
			std::cout << (LPCTSTR)str << " = UNSUPPORTED" << vValue.vt << std::endl;
		}
	}
	pIWbemLocator->Release();
	pWbemServices->Release();
	pEnumObject->Release();
	pClassObject->Release();
	return ret;
}
