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

 #include <managed/convert.hpp>

#include <types.hpp>

#include <string>
#include <functional>

#include <NSCAPI.h>
//#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <win/windows.hpp>

#include <list>

#include <utf8.hpp>

//#include <nscapi/macros.hpp>

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
