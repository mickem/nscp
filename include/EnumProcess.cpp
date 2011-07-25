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


void CEnumProcess::enable_token_privilege(LPTSTR privilege)
{
	HANDLE hToken;                       
	TOKEN_PRIVILEGES token_privileges;                 
	DWORD dwSize;                       
	ZeroMemory (&token_privileges, sizeof(token_privileges));
	token_privileges.PrivilegeCount = 1;
	if ( !OpenProcessToken (GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
		throw process_enumeration_exception(_T("Failed to open process token: ") + error::lookup::last_error());
	if (!LookupPrivilegeValue ( NULL, privilege, &token_privileges.Privileges[0].Luid)) { 
		CloseHandle (hToken);
		throw process_enumeration_exception(_T("Failed to lookup privilege: ") + error::lookup::last_error());
	}
	token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges ( hToken, FALSE, &token_privileges, 0, NULL, &dwSize)) { 
		CloseHandle (hToken);
		throw process_enumeration_exception(_T("Failed to adjust token privilege: ") + error::lookup::last_error());
	}
	CloseHandle (hToken);
}

void CEnumProcess::disable_token_privilege(LPTSTR privilege)
{
	HANDLE hToken;                       
	TOKEN_PRIVILEGES token_privileges;                 
	DWORD dwSize;                       
	ZeroMemory (&token_privileges, sizeof (token_privileges));
	token_privileges.PrivilegeCount = 1;
	if ( !OpenProcessToken (GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
		throw process_enumeration_exception(_T("Failed to open process token: ") + error::lookup::last_error());
	if (!LookupPrivilegeValue ( NULL, privilege, &token_privileges.Privileges[0].Luid)) { 
		CloseHandle (hToken);
		throw process_enumeration_exception(_T("Failed to lookup privilege: ") + error::lookup::last_error());
	}
	token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;
	if (!AdjustTokenPrivileges ( hToken, FALSE, &token_privileges, 0, NULL, &dwSize)) { 
		CloseHandle (hToken);
		throw process_enumeration_exception(_T("Failed to adjust token privilege: ") + error::lookup::last_error());
	}
	CloseHandle (hToken);
}

CEnumProcess::process_list CEnumProcess::enumerate_processes(bool expand_command_line, bool find_16bit, CEnumProcess::error_reporter *error_interface, unsigned int buffer_size) {
	if (error_interface!=NULL)
		error_interface->report_debug_enter(_T("enumerate_processes"));
	try {
		if (error_interface!=NULL)
			error_interface->report_debug_enter(_T("enable_token_privilege"));
		enable_token_privilege(SE_DEBUG_NAME);
		if (error_interface!=NULL)
			error_interface->report_debug_exit(_T("enable_token_privilege"));
	} catch (process_enumeration_exception &e) {
		if (error_interface!=NULL)
			error_interface->report_warning(e.what());
	} 

	std::list<CProcessEntry> ret;
	DWORD *dwPIDs = new DWORD[buffer_size+1];
	DWORD cbNeeded = 0;
	if (error_interface!=NULL)
		error_interface->report_debug_enter(_T("FEnumProcesses"));
	BOOL OK = FEnumProcesses(dwPIDs, buffer_size*sizeof(DWORD), &cbNeeded);
	if (error_interface!=NULL)
		error_interface->report_debug_exit(_T("FEnumProcesses"));
	if (cbNeeded >= DEFAULT_BUFFER_SIZE*sizeof(DWORD)) {
		delete [] dwPIDs;
		if (error_interface!=NULL)
			error_interface->report_debug(_T("Need larger buffer: ") + strEx::itos(buffer_size));
		return enumerate_processes(expand_command_line, find_16bit, error_interface, buffer_size * 10); 
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
		entry.hung = false;
		try {
// 			if (error_interface!=NULL)
// 				error_interface->report_debug_enter(_T("describe_pid"));
			try {
				entry = describe_pid(dwPIDs[i], expand_command_line);
			} catch (process_enumeration_exception &e) {
				if (error_interface!=NULL)
					error_interface->report_debug(e.what());
				if (expand_command_line) {
					try {
				entry = describe_pid(dwPIDs[i], false);
					} catch (process_enumeration_exception &e) {
						if (error_interface!=NULL)
							error_interface->report_debug(e.what());
					}
				}
			}
// 			if (error_interface!=NULL)
// 				error_interface->report_debug_exit(_T("describe_pid"));
			if (VDMDBG!=NULL&&find_16bit) {
				if (error_interface!=NULL)
					error_interface->report_debug(_T("Looking for 16bit apps"));
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
				error_interface->report_error(_T("Unhandeled exception describing PID: ") + strEx::itos(dwPIDs[i]) + _T(": ") + e.what());
		} catch (...) {
			if (error_interface!=NULL)
				error_interface->report_error(_T("Unknown exception describing PID: ") + strEx::itos(dwPIDs[i]));
		}
	}

	std::vector<DWORD> hung_pids = find_crashed_pids(error_interface);
	for (process_list::iterator entry = ret.begin(); entry != ret.end(); ++entry) {
		if (std::find(hung_pids.begin(), hung_pids.end(), entry->dwPID) != hung_pids.end())
			(*entry).hung = true;
		else
			(*entry).hung = false;
	}

	delete [] dwPIDs;
	if (error_interface!=NULL)
		error_interface->report_debug_exit(_T("enumerate_processes"));
	return ret;
}

struct enum_data {
	CEnumProcess::error_reporter * error_interface;
	std::vector<DWORD> crashed_pids;

};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam ) {
	enum_data *data = reinterpret_cast<enum_data*>(lParam);
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	if (GetWindow(hwnd, GW_OWNER) != NULL)
		return TRUE;
	PDWORD result;
	if (!SendMessageTimeout(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 500, reinterpret_cast<PDWORD_PTR>(&result))) {
		if (data->error_interface!=NULL)
			data->error_interface->report_debug(_T("pid: ") + strEx::itos(pid) + _T(" was hung"));
		data->crashed_pids.push_back(pid);
	}

// 	TCHAR *buffer = new TCHAR[1024];
// 	int len = GetWindowText(hwnd, buffer, 1023);
// 	buffer[30] = 0;
// 	if (data->error_interface!=NULL)
// 		data->error_interface->report_debug(_T("pid: ") + res + strEx::itos(pid) + _T(" - ") + strEx::itos(len) + _T(" - ") + buffer);
// 	//std::wcout << _T("pid: ") << pid << _T(" - ") << len << _T(" : ") << buffer << std::endl;
// 	delete [] buffer;
	return TRUE;
}

std::vector<DWORD> CEnumProcess::find_crashed_pids(CEnumProcess::error_reporter * error_interface) {
	if (error_interface)
		error_interface->report_debug_enter(_T("find_crashed_pids"));
	enum_data data;
	data.error_interface = error_interface;
	if(!EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&data))) {
		if (error_interface)
			error_interface->report_error(_T("Failed to enumerate windows: ") + error::lookup::last_error());
	}
	if (error_interface)
		error_interface->report_debug_exit(_T("find_crashed_pids"));
	return data.crashed_pids;
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
		throw process_enumeration_exception(GetLastError(), _T("Failed to open process: ") + strEx::itos(pid) + _T(": "));
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
			CloseHandle(hProc);
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

