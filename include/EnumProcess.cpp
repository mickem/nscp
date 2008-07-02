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


CEnumProcess::CEnumProcess() : m_pProcesses(NULL), m_pModules(NULL), m_pCurrentP(NULL), m_pCurrentM(NULL), lpString(NULL), PSAPI(NULL)
{
	lpString = new TCHAR[MAX_FILENAME+1];

	PSAPI = ::LoadLibrary(_TEXT("PSAPI"));
	if (PSAPI)  
	{
		// Setup variables
		m_MAX_COUNT = 256;
		m_cProcesses = 0;
		m_cModules   = 0;

		// Find PSAPI functions
		FEnumProcesses = (PFEnumProcesses)::GetProcAddress(PSAPI, "EnumProcesses");
		FEnumProcessModules = (PFEnumProcessModules)::GetProcAddress(PSAPI, "EnumProcessModules");
#ifdef UNICODE
		FGetModuleFileNameEx = (PFGetModuleFileNameEx)::GetProcAddress(PSAPI, "GetModuleFileNameExW");
#else
		FGetModuleFileNameEx = (PFGetModuleFileNameEx)::GetProcAddress(PSAPI, "GetModuleFileNameExA");
#endif
	}

	// Find the preferred method of enumeration
	m_method = ENUM_METHOD::NONE;
	int method = GetAvailableMethods();
	if (method == (method|ENUM_METHOD::PSAPI))    m_method = ENUM_METHOD::PSAPI;

}

CEnumProcess::~CEnumProcess()
{
	delete [] lpString;
	if (m_pProcesses) {delete[] m_pProcesses;}
	if (m_pModules)   {delete[] m_pModules;}
	if (PSAPI) FreeLibrary(PSAPI);
}



int CEnumProcess::GetAvailableMethods() {
	int res = 0;
	// Does all psapi functions exist?
	if (PSAPI&&FEnumProcesses&&FEnumProcessModules&&FGetModuleFileNameEx) 
		res += ENUM_METHOD::PSAPI;
	return res;
}

int CEnumProcess::SetMethod(int method) {
	int avail = GetAvailableMethods();
	if (avail == (method|avail)) 
		m_method = method;
	return m_method;
}

int CEnumProcess::GetSuggestedMethod()
{
	return m_method;
}
// Retrieves the first process in the enumeration. Should obviously be called before
// GetProcessNext
////////////////////////////////////////////////////////////////////////////////////
BOOL CEnumProcess::GetProcessFirst(CEnumProcess::CProcessEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) {
		return FALSE; 
	} else if ((ENUM_METHOD::PSAPI|m_method) == m_method) {
		// Use PSAPI functions
		// ----------------------
		if (m_pProcesses) {delete[] m_pProcesses;}
		m_pProcesses = new DWORD[m_MAX_COUNT];
		m_pCurrentP = m_pProcesses;
		DWORD cbNeeded = 0;
		BOOL OK = FEnumProcesses(m_pProcesses, m_MAX_COUNT*sizeof(DWORD), &cbNeeded);

		// We might need more memory here..
		if (cbNeeded >= m_MAX_COUNT*sizeof(DWORD)) 
		{
			m_MAX_COUNT += 256;
			return GetProcessFirst(pEntry); // Try again.
		}

		if (!OK) return FALSE;
		m_cProcesses = cbNeeded/sizeof(DWORD); 
		return FillPStructPSAPI(*m_pProcesses, pEntry);
	} else {
		return FALSE;
	}
	return TRUE;
}

// Returns the following process
////////////////////////////////////////////////////////////////
BOOL CEnumProcess::GetProcessNext(CEnumProcess::CProcessEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) return FALSE; 

	// Use ToolHelp functions
	// ----------------------
	if ((ENUM_METHOD::PSAPI|m_method) == m_method) {
		// Use PSAPI functions
		// ----------------------
		if (--m_cProcesses <= 0) return FALSE;
		FillPStructPSAPI(*++m_pCurrentP, pEntry);
	} else {
		return FALSE;
	}
	return TRUE;
}


BOOL CEnumProcess::GetModuleFirst(DWORD dwPID, CEnumProcess::CModuleEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) return FALSE; 
	if ((ENUM_METHOD::PSAPI|m_method) == m_method) {
		// Use PSAPI functions
		// ----------------------
		if (m_pModules) {delete[] m_pModules;}
		m_pModules = new HMODULE[m_MAX_COUNT];
		m_pCurrentM = m_pModules;
		DWORD cbNeeded = 0;
		HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
		if (hProc)
		{
			BOOL OK = FEnumProcessModules(hProc, m_pModules, m_MAX_COUNT*sizeof(HMODULE), &cbNeeded);
			CloseHandle(hProc);

			// We might need more memory here..
			if (cbNeeded >= m_MAX_COUNT*sizeof(HMODULE)) 
			{
				m_MAX_COUNT += 256;
				return GetModuleFirst(dwPID, pEntry); // Try again.
			}

			if (!OK) return FALSE;

			m_cModules = cbNeeded/sizeof(HMODULE); 
			return FillMStructPSAPI(dwPID, *m_pCurrentM, pEntry);
		}
		return FALSE;
	} else {
		return FALSE;
	}
}


BOOL CEnumProcess::GetModuleNext(DWORD dwPID, CEnumProcess::CModuleEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) return FALSE; 
	if ((ENUM_METHOD::PSAPI|m_method) == m_method) {
		// Use PSAPI functions
		// ----------------------
		if (--m_cModules <= 0) return FALSE;
		return FillMStructPSAPI(dwPID, *++m_pCurrentM, pEntry);
	} else {
		return FALSE;
	}

}


BOOL CEnumProcess::EnableTokenPrivilege (LPTSTR privilege)
{
	HANDLE hToken;                        
	TOKEN_PRIVILEGES token_privileges;                  
	DWORD dwSize;                        
	ZeroMemory (&token_privileges, sizeof (token_privileges));
	token_privileges.PrivilegeCount = 1;
	if ( !OpenProcessToken (GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
		return FALSE;
	if (!LookupPrivilegeValue ( NULL, privilege, &token_privileges.Privileges[0].Luid))
	{ 
		CloseHandle (hToken);
		return FALSE;
	}

	token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges ( hToken, FALSE, &token_privileges, 0, NULL, &dwSize))
	{ 
		CloseHandle (hToken);
		return FALSE;
	}
	CloseHandle (hToken);
	return TRUE;
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
		throw EnumProcException(_T("VirtualQueryEx failed"), GetLastError());
	LPBYTE lpBuffer = (LPBYTE)malloc (sysinfo.dwPageSize);
	if (lpBuffer == NULL)
		throw EnumProcException(_T("Failed to allocate buffer"));
	SIZE_T dwBytesRead;
	if (!ReadProcessMemory( hProcess, mbi.BaseAddress, (LPVOID)lpBuffer, sysinfo.dwPageSize, &dwBytesRead)) {
		free(lpBuffer);
		throw EnumProcException(_T("ReadProcessMemory failed"), GetLastError());
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


BOOL CEnumProcess::FillPStructPSAPI(DWORD dwPID, CEnumProcess::CProcessEntry* pEntry)
{
	pEntry->dwPID = dwPID;
	// Open process to get filename
	bool bCmdLine = pEntry->getCommandLine();
	DWORD openArgs = PROCESS_QUERY_INFORMATION|PROCESS_VM_READ;
	if (bCmdLine)
		openArgs |= PROCESS_VM_OPERATION;
	HANDLE hProc = OpenProcess(openArgs, FALSE, dwPID);
	if (!hProc) {
		pEntry->filename = _T("N/A (security restriction)");
		return TRUE;
	}
	if (bCmdLine) {
		try {
			pEntry->command_line = GetCommandLine(hProc);
		} catch (EnumProcException &e) {
			pEntry->command_line = _T("ERROR: " + e.getMessage(););
		} catch (...) {
			pEntry->command_line = _T("ERROR: Failed to get CommandLine.");
		}
	}
	HMODULE hMod;
	DWORD size;
	// Get the first module (the process itself)
	if( FEnumProcessModules(hProc, &hMod, sizeof(hMod), &size) ) {
		//Get filename
		//GetModuleFileNameEx

		if( !FGetModuleFileNameEx( hProc, hMod, lpString, MAX_FILENAME) ) { 
			pEntry->filename = _T("N/A (error)");
		} else {
			std::wstring path = lpString;
			std::wstring::size_type pos = path.find_last_of(_T("\\"));
			if (pos != std::wstring::npos) {
				path = path.substr(++pos);
			}
			pEntry->filename = path;
		}
	}
	CloseHandle(hProc);
}


BOOL CEnumProcess::FillMStructPSAPI(DWORD dwPID, HMODULE mMod, CEnumProcess::CModuleEntry *pEntry)
{
	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
	if (hProc)
	{
		if( !FGetModuleFileNameEx( hProc, mMod, lpString, MAX_FILENAME) )
		{
			pEntry->sFilename = _T("N/A (error)");
		} else {
			pEntry->sFilename = lpString;
		}
		pEntry->pLoadBase = (PVOID) mMod;
		pEntry->pPreferredBase = GetModulePreferredBase(dwPID, (PVOID)mMod);
		CloseHandle(hProc);
		return TRUE;
	}
	return FALSE;
}



PVOID CEnumProcess::GetModulePreferredBase(DWORD dwPID, PVOID pModBase)
{
	if (ENUM_METHOD::NONE == m_method) return NULL; 
	HANDLE hProc = OpenProcess(PROCESS_VM_READ, FALSE, dwPID);
	if (hProc)
	{
		IMAGE_DOS_HEADER idh;
		IMAGE_NT_HEADERS inh;
		//Read DOS header
		ReadProcessMemory(hProc, pModBase, &idh, sizeof(idh), NULL);

		if (IMAGE_DOS_SIGNATURE == idh.e_magic) // DOS header OK?
			// Read NT headers at offset e_lfanew 
			ReadProcessMemory(hProc, (PBYTE)pModBase + idh.e_lfanew, &inh, sizeof(inh), NULL);

		CloseHandle(hProc); 

		if (IMAGE_NT_SIGNATURE == inh.Signature) //NT signature OK?
			// Get the preferred base...
			return (PVOID) inh.OptionalHeader.ImageBase; 

	}

	return NULL; //didn't find anything useful..
}


