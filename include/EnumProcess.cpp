/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>
#include <iostream>

#include "EnumProcess.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CEnumProcess::CEnumProcess() : PSAPI(NULL), VDMDBG(NULL), FVDMEnumTaskWOWEx(NULL)
{
	PSAPI = ::LoadLibrary(_TEXT("PSAPI"));
	if (PSAPI)  
	{
		// Find PSAPI functions
		FEnumProcesses = (PFEnumProcesses)::GetProcAddress(PSAPI, "EnumProcesses");
		FEnumProcessModules = (PFEnumProcessModules)::GetProcAddress(PSAPI, "EnumProcessModules");
#ifdef UNICODE
		FGetModuleFileNameEx = (PFGetModuleFileNameEx)::GetProcAddress(PSAPI, "GetModuleFileNameExW");
#else
		FGetModuleFileNameEx = (PFGetModuleFileNameEx)::GetProcAddress(PSAPI, "GetModuleFileNameExA");
#endif
	}

	VDMDBG = ::LoadLibrary(_TEXT("VDMDBG"));
	if (VDMDBG)
	{
		// Find VDMdbg functions
		FVDMEnumTaskWOWEx = (PFVDMEnumTaskWOWEx)::GetProcAddress(VDMDBG, "VDMEnumTaskWOWEx");
	}
}

CEnumProcess::~CEnumProcess()
{
	if (PSAPI) FreeLibrary(PSAPI);
	if (VDMDBG) FreeLibrary(VDMDBG);
}

struct find_16bit_container {
	std::list<CEnumProcess::CProcessEntry> *target;
	DWORD pid;
};
BOOL CALLBACK Enum16Proc( DWORD dwThreadId, WORD hMod16, WORD hTask16, PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined )
{
	find_16bit_container *container = reinterpret_cast<find_16bit_container*>(lpUserDefined);
	CEnumProcess::CProcessEntry pEntry;
	pEntry.dwPID = container->pid;
	pEntry.command_line = strEx::string_to_wstring(pszFileName);
	std::wstring::size_type pos = pEntry.command_line.find_last_of(_T("\\"));
	if (pos != std::wstring::npos)
		pEntry.filename = pEntry.command_line.substr(++pos);
	else
		pEntry.filename = pEntry.command_line;
	container->target->push_back(pEntry);
	return FALSE;
}


CEnumProcess::process_list CEnumProcess::enumerate_processes(bool expand_command_line, bool find_16bit, CEnumProcess::error_reporter *error_interface, unsigned int buffer_size) {
	std::list<CProcessEntry> ret;
	DWORD *dwPIDs = new DWORD[buffer_size+1];
	DWORD cbNeeded = 0;
	BOOL OK = FEnumProcesses(dwPIDs, buffer_size*sizeof(DWORD), &cbNeeded);
	if (cbNeeded >= DEFAULT_BUFFER_SIZE*sizeof(DWORD)) {
		delete [] dwPIDs;
		return enumerate_processes(expand_command_line, find_16bit, error_interface, buffer_size + 1024); 
	}
	if (!OK) {
		delete [] dwPIDs;
		throw process_enumeration_exception(_T("Failed to enumerate process: ") + error::lookup::last_error());
	}
	unsigned int process_count = cbNeeded/sizeof(DWORD);
	for (unsigned int i = 0;i <process_count; ++i) {
		if (dwPIDs[i] == 0)
			continue;
		CProcessEntry entry;
		try {
			try {
				entry = describe_pid(dwPIDs[i], expand_command_line);
			} catch (process_enumeration_exception &e) {
				if (error_interface!=NULL)
					error_interface->report_warning(e.what());
				entry = describe_pid(dwPIDs[i], false);
			}
			if (VDMDBG!=NULL&&find_16bit) {
				if( _wcsicmp(entry.filename.substr(0,9).c_str(), _T("NTVDM.EXE")) == 0) {
					find_16bit_container container;
					container.target = &ret;
					container.pid = entry.dwPID;
					FVDMEnumTaskWOWEx(entry.dwPID, (TASKENUMPROCEX)&Enum16Proc, (LPARAM) &container);
				}
			}
			ret.push_back(entry);
		} catch (process_enumeration_exception &e) {
			if (error_interface!=NULL)
				error_interface->report_error(_T("Unhandled exception describing PID: ") + strEx::itos(dwPIDs[i]) + _T(": ") + e.what());
		} catch (...) {
			if (error_interface!=NULL)
				error_interface->report_error(_T("Unknown exception describing PID: ") + strEx::itos(dwPIDs[i]));
		}
	}
	delete [] dwPIDs;
	return ret;
}

CEnumProcess::CProcessEntry CEnumProcess::describe_pid(DWORD pid, bool expand_command_line) {
	CProcessEntry entry;
	entry.dwPID = pid;
	// Open process to get filename
	DWORD openArgs = PROCESS_QUERY_INFORMATION|PROCESS_VM_READ;
	if (expand_command_line)
		openArgs |= PROCESS_VM_OPERATION;
	HANDLE hProc = OpenProcess(openArgs, FALSE, pid);
	if (!hProc) {
		throw process_enumeration_exception(_T("Failed to open process: ") + strEx::itos(pid) + _T(": ") + error::lookup::last_error());
	}
	if (expand_command_line) {
		entry.command_line = GetCommandLine(hProc);
	}
	HMODULE hMod;
	DWORD size;
	// Get the first module (the process itself)
	if( FEnumProcessModules(hProc, &hMod, sizeof(hMod), &size) ) {
		TCHAR buffer[MAX_FILENAME+1];
		if( !FGetModuleFileNameEx( hProc, hMod, reinterpret_cast<LPTSTR>(&buffer), MAX_FILENAME) ) { 
			throw process_enumeration_exception(_T("Failed to find name for: ") + strEx::itos(pid) + _T(": ") + error::lookup::last_error());
		} else {
			std::wstring path = buffer;
			std::wstring::size_type pos = path.find_last_of(_T("\\"));
			if (pos != std::wstring::npos) {
				path = path.substr(++pos);
			}
			entry.filename = path;
		}
	}
	CloseHandle(hProc);
	return entry;
}

// Process data block is found in an NT machine.
// on an Intel system at 0x00020000  which is the 32
// memory page. At offset 0x0498 is what I believe to be
// the process' startup directory which is followed by
// the system's PATH. Next is  process full command
// followed by the exe name.
#define PROCESS_DATA_BLOCK_ADDRESS      (LPVOID)0x00020498
// align pointer
#define ALIGNMENT(x) ( (x & 0xFFFFFFFC) ? (x & 0xFFFFFFFC) + sizeof(DWORD) : x )

std::wstring CEnumProcess::GetCommandLine(HANDLE hProcess)
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo (&sysinfo);

	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQueryEx (hProcess, PROCESS_DATA_BLOCK_ADDRESS, &mbi, sizeof(mbi) ) == 0)
		throw process_enumeration_exception(_T("VirtualQueryEx failed: ") + error::lookup::last_error());
	LPBYTE lpBuffer = (LPBYTE)malloc (sysinfo.dwPageSize);
	if (lpBuffer == NULL)
		throw process_enumeration_exception(_T("Failed to allocate buffer"));
	SIZE_T dwBytesRead;
	if (!ReadProcessMemory( hProcess, mbi.BaseAddress, (LPVOID)lpBuffer, sysinfo.dwPageSize, &dwBytesRead)) {
		free(lpBuffer);
		throw process_enumeration_exception(_T("ReadProcessMemory failed: ") + error::lookup::last_error());
	}
	LPBYTE lpPos = lpPos = lpBuffer + ((DWORD)PROCESS_DATA_BLOCK_ADDRESS - (DWORD)mbi.BaseAddress);

	// Skip programs current directory and path
	lpPos += (wcslen((LPWSTR)lpPos) + 1) * sizeof(WCHAR);

	// Aligned on a DWORD boundary skip it, and copy the next string into
	// buffer and null terminate it.
	lpPos = (LPBYTE)ALIGNMENT((DWORD)lpPos);
	lpPos += (wcslen((LPWSTR)lpPos) + 1) * sizeof(WCHAR);

	// Sometimes there is an extra \0 here
	/*
	if ( *lpPos == '\0' ) 
		lpPos += sizeof(WCHAR);
	*/

	DWORD nStrLength = (wcslen((LPWSTR)lpPos) + 1) * sizeof(WCHAR);
	WCHAR *buffer = new TCHAR[nStrLength+2];
	buffer[0] = L'\0';
	if(nStrLength > sizeof(WCHAR)) {
		wcsncpy(buffer, (LPWSTR)lpPos, nStrLength);
		buffer[nStrLength] = L'\0';
	}
	free(lpBuffer);
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

