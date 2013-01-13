#pragma once
#include <atlbase.h>

namespace error {
	class com {
	public:
		static std::wstring get() {
//			USES_CONVERSION;
			CComPtr<IErrorInfo> errorInfo;
			HRESULT hr = GetErrorInfo(NULL, &errorInfo);
			if (FAILED(hr) || hr == S_FALSE)
				return _T("unknown error: ") + error::format::from_system(hr);
			CComBSTR bDesc, bSource;
			hr = errorInfo->GetSource(&bSource);
			if (FAILED(hr))
				return _T("unknown error: ") + error::format::from_system(hr);
			hr = errorInfo->GetDescription(&bDesc);
			if (FAILED(hr))
				return _T("unknown error: ") + error::format::from_system(hr);
			std::wstring ret = OLE2T(bSource);
			ret += _T(" - ");
			ret += OLE2T(bDesc);
			return ret;
		}
	};
}
