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
				return "unknown com error 1: " + error::format::from_system(hr);
			CComBSTR bDesc, bSource;
			hr = errorInfo->GetSource(&bSource);
			if (FAILED(hr))
				return "unknown com error 2: " + error::format::from_system(hr);
			hr = errorInfo->GetDescription(&bDesc);
			if (FAILED(hr))
				return "unknown com error 3: " + error::format::from_system(hr);
			std::string ret = utf8::cvt<std::string>(OLE2T(bSource));
			ret += " - ";
			ret += utf8::cvt<std::string>(OLE2T(bDesc));
			return ret;
		}
	};
}
