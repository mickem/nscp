/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <error/error.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace com_helper {
	class com_exception {
		std::string error_;
		HRESULT result_;
	public:
		com_exception(std::string error) : error_(error) {}
		com_exception(std::string error, HRESULT result) : error_(error), result_(result) {
			error_ += error::format::from_system(result);
		}
		std::string reason() {
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
				throw com_exception("CoInitialize failed: ", hRes);
			isInitialized_ = true;
			//hRes = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
			hRes = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
			if (FAILED(hRes))
				throw com_exception("CoInitializeSecurity failed: ", hRes);
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