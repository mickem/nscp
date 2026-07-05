// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

 #include <managed/convert.hpp>

#include <string>
#include <functional>

#include <NSCAPI.h>

#include <win/windows.hpp>

#include <list>

#include <str/utf8.hpp>

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

using namespace PB::Commands;

std::string to_nstring(System::String^ s) {
	pin_ptr<const wchar_t> pinString = PtrToStringChars(s);
	return utf8::cvt<std::string>(std::wstring(pinString));
}

std::string to_nstring(protobuf_data^ request) {
	char *buffer = new char[request->Length + 1];
	memset(buffer, 0, request->Length + 1);
	Marshal::Copy(request, 0, IntPtr(buffer), (int)request->Length);
	std::string ret(buffer, (std::string::size_type)request->Length);
	delete[] buffer;
	return ret;
}

protobuf_data^ to_pbd(const std::string &buffer) {
	protobuf_data^ arr = gcnew protobuf_data(buffer.size());
	Marshal::Copy(IntPtr(const_cast<char*>(buffer.c_str())), arr, 0, arr->Length);
	return arr;
}

System::String^ to_mstring(const std::string &s) {
	return gcnew System::String(utf8::cvt<std::wstring>(s).c_str());
}
System::String^ to_mstring(const std::wstring &s) {
	return gcnew System::String(s.c_str());
}

array<String^>^ to_mlist(const std::list<std::string> list) {
	array<System::String^>^ ret = gcnew array<System::String^>(list.size());
	typedef std::list<std::string>::const_iterator iter_t;
	int j = 0;

	for (iter_t i = list.begin(); i != list.end(); ++i)
		ret[j++] = gcnew System::String(utf8::cvt<std::wstring>(*i).c_str());
	return ret;
};
