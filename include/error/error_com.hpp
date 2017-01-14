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
#include <atlbase.h>
#include <utf8.hpp>
#include <comdef.h>

namespace error {
	class com {
	public:
		static std::string get(HRESULT srcHr) {
			CComPtr<IErrorInfo> errorInfo;
			HRESULT hr = GetErrorInfo(NULL, &errorInfo);
			if (FAILED(hr) || hr == S_FALSE)
				return utf8::cvt<std::string>(std::wstring(_com_error(srcHr).ErrorMessage()));
			CComBSTR bDesc, bSource;
			hr = errorInfo->GetSource(&bSource);
			if (FAILED(hr))
				return utf8::cvt<std::string>(std::wstring(_com_error(srcHr).ErrorMessage()));
			hr = errorInfo->GetDescription(&bDesc);
			if (FAILED(hr))
				return utf8::cvt<std::string>(std::wstring(_com_error(srcHr).ErrorMessage()));
			std::string ret = utf8::cvt<std::string>(OLE2T(bSource));
			ret += " - ";
			ret += utf8::cvt<std::string>(OLE2T(bDesc));
			return ret;
		}
	};
}
