/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <error/error.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Objbase.h>

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