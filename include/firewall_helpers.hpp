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