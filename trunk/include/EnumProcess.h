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
#pragma once

#include <psapi.h>
#include <tlhelp32.h>
#include <string>


namespace ENUM_METHOD 
{
	const int NONE    = 0x0;
	const int PSAPI   = 0x1;
	const int TOOLHELP= 0x2;
	const int PROC16  = 0x4;
} 

const int MAX_FILENAME = 256;

#ifdef UNICODE
// Functions loaded from PSAPI
typedef BOOL (WINAPI *PFEnumProcesses)(DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL (WINAPI *PFEnumProcessModules)(HANDLE hProcess, HMODULE * lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *PFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize);

//Functions loaded from Kernel32
typedef HANDLE (WINAPI *PFCreateToolhelp32Snapshot)(DWORD dwFlags, DWORD th32ProcessID);
typedef BOOL (WINAPI *PFProcess32First)(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
typedef BOOL (WINAPI *PFProcess32Next)(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
typedef BOOL (WINAPI *PFModule32First)(HANDLE hSnapshot, LPMODULEENTRY32W lpme);
typedef BOOL (WINAPI *PFModule32Next)(HANDLE hSnapshot, LPMODULEENTRY32W lpme);
#else
// Functions loaded from PSAPI
typedef BOOL (WINAPI *PFEnumProcesses)(DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL (WINAPI *PFEnumProcessModules)(HANDLE hProcess, HMODULE * lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *PFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize);

//Functions loaded from Kernel32
typedef HANDLE (WINAPI *PFCreateToolhelp32Snapshot)(DWORD dwFlags, DWORD th32ProcessID);
typedef BOOL (WINAPI *PFProcess32First)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef BOOL (WINAPI *PFProcess32Next)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef BOOL (WINAPI *PFModule32First)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
typedef BOOL (WINAPI *PFModule32Next)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
#endif

class CEnumProcess  
{
public:

	struct CProcessEntry
	{
		std::wstring sFilename;
		DWORD  dwPID;
		WORD   hTask16;
		// Constructors/Destructors
		CProcessEntry() : dwPID(0), hTask16(0) {}
		CProcessEntry(CProcessEntry &e) : dwPID(e.dwPID), hTask16(e.hTask16), sFilename(e.sFilename) {}
		virtual ~CProcessEntry() {}
	};

	struct CModuleEntry
	{
		std::wstring sFilename;
		PVOID pLoadBase;
		PVOID pPreferredBase;
		// Constructors/Destructors
		CModuleEntry() : pLoadBase(NULL), pPreferredBase(NULL) {}
		CModuleEntry(CModuleEntry &e) : pLoadBase(e.pLoadBase), pPreferredBase(e.pPreferredBase), sFilename(e.sFilename) {}
		virtual ~CModuleEntry() {}
	};

	CEnumProcess();
	virtual ~CEnumProcess();

	BOOL GetModuleNext(DWORD dwPID, CModuleEntry* pEntry);
	BOOL GetModuleFirst(DWORD dwPID, CModuleEntry* pEntry);
	BOOL GetProcessNext(CProcessEntry *pEntry);    
	BOOL GetProcessFirst(CProcessEntry* pEntry);

	int GetAvailableMethods();
	int GetSuggestedMethod();
	int SetMethod(int method);



protected:

	PVOID GetModulePreferredBase(DWORD dwPID, PVOID pModBase);
	// General members
	int m_method;

	// PSAPI related members
	HMODULE PSAPI;   //Handle to the module
	int m_MAX_COUNT; 
	DWORD* m_pProcesses, *m_pCurrentP; // Process identifiers
	long m_cProcesses, m_cModules;     // Number of Processes/Modules found
	HMODULE* m_pModules, *m_pCurrentM; // Handles to Modules 
	// PSAPI related functions
	PFEnumProcesses       FEnumProcesses;           // Pointer to EnumProcess
	PFEnumProcessModules  FEnumProcessModules; // Pointer to EnumProcessModules
	PFGetModuleFileNameEx FGetModuleFileNameEx;// Pointer to GetModuleFileNameEx
	BOOL FillPStructPSAPI(DWORD pid, CProcessEntry* pEntry);
	BOOL FillMStructPSAPI(DWORD dwPID, HMODULE mMod, CModuleEntry* pEntry);

	// ToolHelp related members
	HANDLE m_hProcessSnap, m_hModuleSnap;
	HMODULE TOOLHELP;   //Handle to the module (Kernel32)
#ifdef UNICODE
	PROCESSENTRY32W m_pe;
	MODULEENTRY32W  m_me;
#else
	PROCESSENTRY32 m_pe;
	MODULEENTRY32  m_me;
#endif
	// ToolHelp related functions
	PFCreateToolhelp32Snapshot FCreateToolhelp32Snapshot;
	PFProcess32First FProcess32First;
	PFProcess32Next  FProcess32Next;
	PFModule32First  FModule32First;
	PFModule32Next   FModule32Next;   
	LPTSTR lpString;

};

