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

#include <netfw.h>


#pragma once


namespace installer {
	class firewall_exception {
	private:
		std::wstring error_;
	public:
		firewall_exception(std::wstring error) : error_(error) {}
		firewall_exception(std::wstring error, DWORD code) : error_(error) {
			// TODO format error
		}

	};
	class firewall {
	public:
		INetFwMgr* aquire_firewall_mgr() {
			HRESULT hr = S_OK;
			INetFwMgr* fwMgr = NULL;

			// Create an instance of the firewall settings manager.
			hr = CoCreateInstance(__uuidof(NetFwMgr), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwMgr), (void**)&fwMgr );
			if (FAILED(hr)) {
				throw 
				printf("CoCreateInstance failed: 0x%08lx\n", hr);
				goto error;
			}

			// Retrieve the local firewall policy.
			hr = fwMgr->get_LocalPolicy(&fwPolicy);
			if (FAILED(hr))
			{
				printf("get_LocalPolicy failed: 0x%08lx\n", hr);
				goto error;
			}

			// Retrieve the firewall profile currently in effect.
			hr = fwPolicy->get_CurrentProfile(fwProfile);
			if (FAILED(hr))
			{
				printf("get_CurrentProfile failed: 0x%08lx\n", hr);
				goto error;
			}

error:

			// Release the local firewall policy.
			if (fwPolicy != NULL)
			{
				fwPolicy->Release();
			}

			// Release the firewall settings manager.
			if (fwMgr != NULL)
			{
				fwMgr->Release();
			}

			return hr;
		}


	};

}