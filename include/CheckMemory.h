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

#include <windows.h>
#include <error/error.hpp>


typedef BOOL (WINAPI *PFGlobalMemoryStatusEx)(LPMEMORYSTATUSEX lpBuffer);
typedef BOOL (WINAPI *PFGlobalMemoryStatus)(LPMEMORYSTATUS lpBuffer);

namespace CheckMemMethod
{
	const int None		= 0x0;
	const int Extended	= 0x1;
	const int Normal	= 0x2;
} 

class CheckMemoryException {
private:
	std::string name_;
	std::string msg_;
public:
	CheckMemoryException(std::string name,std::string msg) : name_(name), msg_(msg) {};

	std::string reason() {
		return "Service: " + name_ + " caused: " + msg_;
	}
};

class CheckMemory {
private:
	typedef struct {
		unsigned long long total;
		unsigned long long avail;
	} memType;
public:
	typedef struct {
		unsigned long dwMemoryLoad;  
		memType phys;
		memType commited;
		memType virt;
		memType page;
		double availExtendedVirtual;
	} memData;

public:
	CheckMemory() : FEGlobalMemoryStatusEx(NULL), FEGlobalMemoryStatus(NULL), method_(CheckMemMethod::None)
	{
		hKernel32 = ::LoadLibrary(L"Kernel32");
		if (hKernel32)  
		{
			FEGlobalMemoryStatusEx = (PFGlobalMemoryStatusEx)::GetProcAddress(hKernel32, "GlobalMemoryStatusEx");
			FEGlobalMemoryStatus = (PFGlobalMemoryStatus)::GetProcAddress(hKernel32, "GlobalMemoryStatus");
		}
		int method = getAvailableMethods();
		if ((method&CheckMemMethod::Extended)==CheckMemMethod::Extended)
			method_ = CheckMemMethod::Extended;
		else
			method_ = method;
	}
	virtual ~CheckMemory() {
		if (hKernel32) FreeLibrary(hKernel32);
	}

	int getAvailableMethods() {
		int ret = CheckMemMethod::None;
		if (FEGlobalMemoryStatusEx)
			ret |= CheckMemMethod::Extended;
		if (FEGlobalMemoryStatus)
			ret |= CheckMemMethod::Normal;
		return ret;
	}
	int getSuggestedMethod();
	int setMethod(int method) {
		if ((getAvailableMethods()&method) != 0)
			method_ = method;
		return method_;
	}

	memData getMemoryStatus() {
		memData ret;
		if (method_ == CheckMemMethod::Extended) {
			MEMORYSTATUSEX buffer;
			buffer.dwLength = sizeof(buffer);
			if (!FEGlobalMemoryStatusEx(&buffer))
				throw CheckMemoryException("CheckMemory", "GlobalMemoryStatusEx failed: " + error::lookup::last_error());
			ret.phys.total = buffer.ullTotalPhys;
			ret.phys.avail = buffer.ullAvailPhys;
			ret.virt.total = buffer.ullTotalVirtual;
			ret.virt.avail = buffer.ullAvailVirtual;
			ret.commited.total = buffer.ullTotalPageFile;
			ret.commited.avail = buffer.ullAvailPageFile;
			ret.page.total = buffer.ullTotalPageFile-buffer.ullTotalPhys;
			ret.page.avail = buffer.ullAvailPageFile-buffer.ullAvailPhys;
		} else if (method_ == CheckMemMethod::Normal) {
			MEMORYSTATUS buffer;
			buffer.dwLength = sizeof(buffer);
			if (!FEGlobalMemoryStatus(&buffer))
				throw CheckMemoryException("CheckMemory", "GlobalMemoryStatus failed: " + error::lookup::last_error());
			ret.phys.total = buffer.dwTotalPhys;
			ret.phys.avail = buffer.dwAvailPhys;
			ret.virt.total = buffer.dwTotalVirtual;
			ret.virt.avail = buffer.dwAvailVirtual;
			ret.commited.total = buffer.dwTotalPageFile;
			ret.commited.avail = buffer.dwAvailPageFile;
		}
		return ret;
	}
	std::string to_string() const { return "mem"; }
private:
	int method_;
	HMODULE hKernel32;
	PFGlobalMemoryStatusEx	FEGlobalMemoryStatusEx;
	PFGlobalMemoryStatus	FEGlobalMemoryStatus;
};