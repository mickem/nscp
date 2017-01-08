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
