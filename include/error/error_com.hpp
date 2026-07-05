// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <atlbase.h>
#include <comdef.h>

#include <str/utf8.hpp>

namespace error {
class com {
 public:
  static std::string get(HRESULT srcHr) {
    CComPtr<IErrorInfo> errorInfo;
    HRESULT hr = GetErrorInfo(NULL, &errorInfo);
    if (FAILED(hr) || hr == S_FALSE) return utf8::cvt<std::string>(std::wstring(_com_error(srcHr).ErrorMessage()));
    CComBSTR bDesc, bSource;
    hr = errorInfo->GetSource(&bSource);
    if (FAILED(hr)) return utf8::cvt<std::string>(std::wstring(_com_error(srcHr).ErrorMessage()));
    hr = errorInfo->GetDescription(&bDesc);
    if (FAILED(hr)) return utf8::cvt<std::string>(std::wstring(_com_error(srcHr).ErrorMessage()));
    auto ret = utf8::cvt<std::string>(OLE2T(bSource));
    ret += " - ";
    ret += utf8::cvt<std::string>(OLE2T(bDesc));
    return ret;
  }
};
}  // namespace error
