#pragma once
#include <atlbase.h>
#include <utf8.hpp>

namespace error {
	class com {
	public:
		static std::string get() {
//			USES_CONVERSION;
			CComPtr<IErrorInfo> errorInfo;
			HRESULT hr = GetErrorInfo(NULL, &errorInfo);
			if (FAILED(hr) || hr == S_FALSE)
				return "unknown error: " + error::format::from_system(hr);
			CComBSTR bDesc, bSource;
			hr = errorInfo->GetSource(&bSource);
			if (FAILED(hr))
				return "unknown error: " + error::format::from_system(hr);
			hr = errorInfo->GetDescription(&bDesc);
			if (FAILED(hr))
				return "unknown error: " + error::format::from_system(hr);
			std::string ret = utf8::cvt<std::string>(OLE2T(bSource));
			ret += " - ";
			ret += utf8::cvt<std::string>(OLE2T(bDesc));
			return ret;
		}
	};
}
