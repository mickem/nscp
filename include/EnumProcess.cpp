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

#include "EnumProcess.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CEnumProcess::CEnumProcess() : m_pProcesses(NULL), m_pModules(NULL), m_pCurrentP(NULL), m_pCurrentM(NULL), lpString(NULL)
{
	lpString = new TCHAR[MAX_FILENAME+1];
	m_hProcessSnap = INVALID_HANDLE_VALUE;
	m_hModuleSnap = INVALID_HANDLE_VALUE;

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

	TOOLHELP = ::LoadLibrary(_TEXT("Kernel32"));
	if (TOOLHELP)  
	{
		// Setup variables
		m_pe.dwSize = sizeof(m_pe);
		m_me.dwSize = sizeof(m_me);
		// Find ToolHelp functions

		FCreateToolhelp32Snapshot = (PFCreateToolhelp32Snapshot)::GetProcAddress(TOOLHELP, "CreateToolhelp32Snapshot");
		FProcess32First = (PFProcess32First)::GetProcAddress(TOOLHELP, "Process32First");
		FProcess32Next = (PFProcess32Next)::GetProcAddress(TOOLHELP, "Process32Next");
		FModule32First = (PFModule32First)::GetProcAddress(TOOLHELP, "Module32First");
		FModule32Next = (PFModule32Next)::GetProcAddress(TOOLHELP, "Module32Next");
	}

	// Find the preferred method of enumeration
	m_method = ENUM_METHOD::NONE;
	int method = GetAvailableMethods();
	if (method == (method|ENUM_METHOD::PSAPI))    m_method = ENUM_METHOD::PSAPI;
	if (method == (method|ENUM_METHOD::TOOLHELP)) m_method = ENUM_METHOD::TOOLHELP;
	if (method == (method|ENUM_METHOD::PROC16))   m_method += ENUM_METHOD::PROC16;

}

CEnumProcess::~CEnumProcess()
{
	delete [] lpString;
	if (m_pProcesses) {delete[] m_pProcesses;}
	if (m_pModules)   {delete[] m_pModules;}
	if (PSAPI) FreeLibrary(PSAPI);
	if (TOOLHELP) FreeLibrary(TOOLHELP);
	if (INVALID_HANDLE_VALUE != m_hProcessSnap) ::CloseHandle(m_hProcessSnap);
	if (INVALID_HANDLE_VALUE != m_hModuleSnap)  ::CloseHandle(m_hModuleSnap);
}



int CEnumProcess::GetAvailableMethods()
{
	int res = 0;
	// Does all psapi functions exist?
	if (PSAPI&&FEnumProcesses&&FEnumProcessModules&&FGetModuleFileNameEx) 
		res += ENUM_METHOD::PSAPI;
	// How about Toolhelp?
	if (TOOLHELP&&FCreateToolhelp32Snapshot&&FProcess32Next&&FProcess32Next&&FModule32First&&FModule32Next) 
		res += ENUM_METHOD::TOOLHELP;

	return res;
}

int CEnumProcess::SetMethod(int method)
{
	int avail = GetAvailableMethods();

	if (method != ENUM_METHOD::PROC16 && avail == (method|avail)) 
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
	if (ENUM_METHOD::NONE == m_method) return FALSE; 


	if ((ENUM_METHOD::TOOLHELP|m_method) == m_method)
		// Use ToolHelp functions
		// ----------------------
	{
		m_hProcessSnap = FCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (INVALID_HANDLE_VALUE == m_hProcessSnap) return FALSE;
		if (!FProcess32First(m_hProcessSnap, &m_pe)) return FALSE;
		pEntry->dwPID = m_pe.th32ProcessID;
		pEntry->sFilename, m_pe.szExeFile;
	}
	else
		// Use PSAPI functions
		// ----------------------
	{
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
	}

	return TRUE;
}

// Returns the following process
////////////////////////////////////////////////////////////////
BOOL CEnumProcess::GetProcessNext(CEnumProcess::CProcessEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) return FALSE; 
	pEntry->hTask16 = 0;


	// Use ToolHelp functions
	// ----------------------
	if ((ENUM_METHOD::TOOLHELP|m_method) == m_method)
	{
		if (!FProcess32Next(m_hProcessSnap, &m_pe)) return FALSE;
		pEntry->dwPID = m_pe.th32ProcessID;
		pEntry->sFilename = m_pe.szExeFile;
	}
	else
		// Use PSAPI functions
		// ----------------------
	{
		if (--m_cProcesses <= 0) return FALSE;
		FillPStructPSAPI(*++m_pCurrentP, pEntry);
	}

	return TRUE;
}


BOOL CEnumProcess::GetModuleFirst(DWORD dwPID, CEnumProcess::CModuleEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) return FALSE; 
	// Use ToolHelp functions
	// ----------------------
	if ((ENUM_METHOD::TOOLHELP|m_method) == m_method)
	{
		if (INVALID_HANDLE_VALUE != m_hModuleSnap)  ::CloseHandle(m_hModuleSnap);
		m_hModuleSnap = FCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);

		if(!FModule32First(m_hModuleSnap, &m_me)) return FALSE;

		pEntry->pLoadBase = m_me.modBaseAddr;
		pEntry->sFilename = m_me.szExePath;
		pEntry->pPreferredBase = GetModulePreferredBase(dwPID, m_me.modBaseAddr);
		return TRUE;
	}
	else
		// Use PSAPI functions
		// ----------------------
	{
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
	}
}


BOOL CEnumProcess::GetModuleNext(DWORD dwPID, CEnumProcess::CModuleEntry *pEntry)
{
	if (ENUM_METHOD::NONE == m_method) return FALSE; 

	// Use ToolHelp functions
	// ----------------------
	if ((ENUM_METHOD::TOOLHELP|m_method) == m_method)
	{
		if(!FModule32Next(m_hModuleSnap, &m_me)) return FALSE;

		pEntry->pLoadBase = m_me.modBaseAddr;
		pEntry->sFilename = m_me.szExePath;
		pEntry->pPreferredBase = GetModulePreferredBase(dwPID, m_me.modBaseAddr);
		return TRUE;
	}
	else
		// Use PSAPI functions
		// ----------------------
	{
		if (--m_cModules <= 0) return FALSE;
		return FillMStructPSAPI(dwPID, *++m_pCurrentM, pEntry);
	}

}



BOOL CEnumProcess::FillPStructPSAPI(DWORD dwPID, CEnumProcess::CProcessEntry* pEntry)
{
	pEntry->dwPID = dwPID;

	// Open process to get filename
	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
	if (hProc)
	{
		HMODULE hMod;
		DWORD size;
		// Get the first module (the process itself)
		if( FEnumProcessModules(hProc, &hMod, sizeof(hMod), &size) )
		{
			//Get filename

			if( !FGetModuleFileNameEx( hProc, hMod, lpString, MAX_FILENAME) ) { 
				pEntry->sFilename = _T("N/A (error)");
			} else {
				pEntry->sFilename = lpString;
			}
		}
		CloseHandle(hProc);
	}
	else
		pEntry->sFilename = _T("N/A (security restriction)");

	return TRUE;
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


