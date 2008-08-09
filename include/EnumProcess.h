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
#include <string>
#include <error.hpp>


namespace ENUM_METHOD 
{
	const int NONE    = 0x0;
	const int PSAPI   = 0x1;
} 

const int MAX_FILENAME = 256;

#ifdef UNICODE
// Functions loaded from PSAPI
typedef BOOL (WINAPI *PFEnumProcesses)(DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL (WINAPI *PFEnumProcessModules)(HANDLE hProcess, HMODULE * lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *PFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize);
#else
// Functions loaded from PSAPI
typedef BOOL (WINAPI *PFEnumProcesses)(DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL (WINAPI *PFEnumProcessModules)(HANDLE hProcess, HMODULE * lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *PFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize);
#endif

class CEnumProcess  
{
public:

	class EnumProcException {
		std::wstring error_;
	public:
		EnumProcException(std::wstring error) : error_(error) {}
		EnumProcException(std::wstring error, DWORD code) : error_(error) {
			error_ += _T(":" ) + error::format::from_system(code);
		}
		std::wstring getMessage() const {
			return error_;
		}
	};

	struct CProcessEntry
	{
		static const int fill_filename = 0x1;
		static const int fill_command_line = 0x2;
		DWORD fill;
		std::wstring filename;
		std::wstring command_line;
		DWORD  dwPID;
		// Constructors/Destructor
		CProcessEntry() : dwPID(0), fill(0) {}
		CProcessEntry(DWORD toFill) : dwPID(0), fill(toFill) {}
		CProcessEntry(const CProcessEntry &e) : dwPID(e.dwPID), fill(e.fill), filename(e.filename), command_line(e.command_line) {}
		virtual ~CProcessEntry() {}
		bool getCommandLine() const { return (fill&fill_command_line)!=0; }
		bool getFilename() const { return (fill&fill_filename)!=0; }
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
	BOOL EnableTokenPrivilege(LPTSTR privilege);
	std::wstring GetCommandLine(HANDLE hProcess);

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
	LPTSTR lpString;
};

