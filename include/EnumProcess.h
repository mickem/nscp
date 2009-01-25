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
//typedef BOOL ( WINAPI *PROCESSENUMPROC )(DWORD dwProcessId,	DWORD dwAttributes,	LPARAM lpUserDefined	);
typedef BOOL ( WINAPI *TASKENUMPROCEX )(DWORD dwThreadId, WORD hMod16, WORD hTask16, PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined );
typedef INT (WINAPI *PFVDMEnumTaskWOWEx)(DWORD dwProcessId, TASKENUMPROCEX fp, LPARAM lparam); 
#else
// Functions loaded from PSAPI
typedef BOOL (WINAPI *PFEnumProcesses)(DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL (WINAPI *PFEnumProcessModules)(HANDLE hProcess, HMODULE * lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *PFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize);
typedef BOOL ( WINAPI *TASKENUMPROCEX )(DWORD dwThreadId, WORD hMod16, WORD hTask16, PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined );
typedef INT (WINAPI *PFVDMEnumTaskWOWEx)(DWORD dwProcessId, TASKENUMPROCEX fp, LPARAM lparam); 
#endif

#define DEFAULT_BUFFER_SIZE 1024

class CEnumProcess  
{
public:

	class error_reporter {
	public:
		virtual void report_error(std::wstring error) = 0;
		virtual void report_warning(std::wstring error) = 0;
	};
	class process_enumeration_exception {
		std::wstring what_;
	public:
		process_enumeration_exception(std::wstring what) : what_(what) {}
		std::wstring what() {
			return what_;
		}
	};

	struct CProcessEntry {
		std::wstring filename;
		std::wstring command_line;
		DWORD  dwPID;
	};

	typedef std::list<CProcessEntry> process_list;
	process_list enumerate_processes(bool expand_command_line, bool find_16bit = false, CEnumProcess::error_reporter *error_interface = NULL, unsigned int buffer_size = DEFAULT_BUFFER_SIZE);
	CProcessEntry describe_pid(DWORD pid, bool expand_command_line);

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

	std::wstring GetCommandLine(HANDLE hProcess);
	bool has_PSAPI() {
		return PSAPI != NULL;
	}

private:

	// PSAPI related members
	HMODULE PSAPI;   //Handle to the module
	HMODULE VDMDBG;
	// PSAPI related functions
	PFEnumProcesses       FEnumProcesses;           // Pointer to EnumProcess
	PFEnumProcessModules  FEnumProcessModules; // Pointer to EnumProcessModules
	PFGetModuleFileNameEx FGetModuleFileNameEx;// Pointer to GetModuleFileNameEx
	PFVDMEnumTaskWOWEx FVDMEnumTaskWOWEx;
};

