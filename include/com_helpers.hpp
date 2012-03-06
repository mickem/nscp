#pragma once

#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#define _WIN32_WINNT 0x0400
#endif

#include <error.hpp>
#include <windows.h>
#include <objbase.h>
#include <atlbase.h>
#include <atlsafe.h>

namespace com_helper {

	class com_exception {
		std::wstring error_;
		HRESULT result_;
	public:
		com_exception(std::wstring error) : error_(error) {}
		com_exception(std::wstring error, HRESULT result) : error_(error), result_(result) {
			error_ += error::format::from_system(result);
		}
		std::wstring getMessage() {
			return error_;
		}

	};

	class initialize_com {
		bool isInitialized_;
	public:
		initialize_com() : isInitialized_(false) {}
		~initialize_com() {
			unInitialize();
		}

		void initialize() {
			HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if (FAILED(hRes))
				throw com_exception(_T("CoInitialize failed: "), hRes);
			isInitialized_ = true;
			//hRes = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
			hRes = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
			if (FAILED(hRes)) 
				throw com_exception(_T("CoInitializeSecurity failed: "), hRes);
		}
		void unInitialize() {
			if (!isInitialized_)
				return;
			CoUninitialize();
			isInitialized_ = false;
		}
		bool isInitialized() const {
			return isInitialized_;
		}

	};

};