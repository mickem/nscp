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
#include "stdafx.h"
#include "trayicon.h"
#include "resource.h"
#include <commctrl.h>
#include <strEx.h>
#include <ShellAPI.h>
#include "SysTray.h"
#include <error.hpp>


extern SysTray gSysTray;

//BOOL ChangeWindowMessageFilter(UINT message,DWORD dwFlag);

unsigned IconWidget_::threadProc(LPVOID lpParameter)
{
	createDialog();
	return 0;
}

#ifdef WINVER < 0x0600
#define MSGFLT_ADD 1
#define MSGFLT_REMOVE 2
typedef BOOL (WINAPI *LPFN_CHANGEWINDOWMESSAGEFILTER) (UINT, DWORD);
#endif

LPFN_CHANGEWINDOWMESSAGEFILTER fnChangeWindowMessageFilter = NULL;
BOOL ChangeWindowMessageFilter(UINT message, DWORD what)
{
	if (fnChangeWindowMessageFilter == NULL)
		fnChangeWindowMessageFilter = (LPFN_CHANGEWINDOWMESSAGEFILTER)GetProcAddress(GetModuleHandle(TEXT("user32")),"ChangeWindowMessageFilter");
	if (fnChangeWindowMessageFilter == NULL) {
		NSC_DEBUG_MSG(_T("Failed to load: ChangeWindowMessageFilter aparently we are not on Vista..."));
		return true;
	}
	NSC_DEBUG_MSG(_T("Chaning window message filters..."));
	return fnChangeWindowMessageFilter(message,what);
}

void test() {
/* error 
WTSQueryUserToken(dwSessionId, &hToken);
DuplicateTokenEx(hTokenNew,MAXIMUM_ALLOWED,NULL,SecurityIdentification,TokenPrimary,&hTokenDup);
*/
/*
	HANDLE hToken = NULL, hTokenDup = NULL;
	HMODULE  hmod = LoadLibrary("kernel32.dll");
	WTSGETACTIVECONSOLESESSIONID lpfnWTSGetActiveConsoleSessionId = (WTSGETACTIVECONSOLESESSIONID)GetProcAddress(hmod,"WTSGetActiveConsoleSessionId"); 
	DWORD dwSessionId = lpfnWTSGetActiveConsoleSessionId();
	WTSQueryUserToken(dwSessionId, &hToken);
	//DuplicateTokenEx(hTokenNew,MAXIMUM_ALLOWED,NULL,SecurityIdentification,TokenPrimary,&hTokenDup);
	//
	WriteToLog("Calling lpfnCreateEnvironmentBlock");
	ZeroMemory( &si, sizeof( STARTUPINFO ) );
	si.cb = sizeof( STARTUPINFO );
	si.lpDesktop = "winsta0\\default";


	LPVOID  pEnv = NULL;
	DWORD dwCreationFlag = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE;
	HMODULE hModule = LoadLibrary("Userenv.dll");
	if(hModule )
	{
		LPFN_CreateEnvironmentBlock lpfnCreateEnvironmentBlock = (LPFN_CreateEnvironmentBlock)GetProcAddress( hModule, "CreateEnvironmentBlock" );
		if( lpfnCreateEnvironmentBlock != NULL )
		{
			if(lpfnCreateEnvironmentBlock(&pEnv, hTokenDup, FALSE))
			{
				WriteToLog("CreateEnvironmentBlock Ok");
				dwCreationFlag |= CREATE_UNICODE_ENVIRONMENT;    
			}
			else
			{
				pEnv = NULL;
			}
		}
	}
	//
	ZeroMemory( &pi,sizeof(pi));

	if ( !CreateProcessAsUser(
		hTokenDup,
		NULL,
		( char * )pszCmd,  
		NULL,
		NULL,
		FALSE,
		dwCreationFlag,
		pEnv,
		NULL,
		&si,
		&pi
		) )
	{

		goto RESTORE;
	} 
*/

}

void IconWidget_::createDialog(void) {
	hDlgWnd = ::CreateDialog(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_NSTRAYDLG),NULL,TrayIcon::DialogProc);

	UINT UDM_TASKBARCREATED = RegisterWindowMessage(_T("TaskbarCreated"));
	if (UDM_TASKBARCREATED == 0) {
		NSC_LOG_ERROR_STD(_T("Failed to register 'TaskbarCreated': ") + error::lookup::last_error());
	}
	if (!ChangeWindowMessageFilter(UDM_TASKBARCREATED, MSGFLT_ADD)) {
		NSC_LOG_ERROR_STD(_T("Failed to cchange window filter: ") + error::lookup::last_error());
	}

	MSG Msg;
	BOOL bRet;
	while((bRet = ::GetMessage(&Msg, NULL, 0, 0)) != 0)
	{
		if (Msg.message == WM_MY_CLOSE) {
			::DestroyWindow(hDlgWnd);
//		} else if (Msg.message == WM_QUERYENDSESSION) {
//			NSC_LOG_ERROR_STD(_T("Got WM_QUERYENDSESSION thingy..."));
		} else if (Msg.message == UDM_TASKBARCREATED) {
			NSC_LOG_MESSAGE_STD(_T("Recreating systray icon..."));
			TrayIcon::addIcon(Msg.hwnd);
		} else if (bRet == -1) {
			// handle the error and possibly exit
			NSC_LOG_ERROR_STD(_T("Wonder what this is... please let me know..."));
			return;
		} else {
		//} else if (!::IsWindow(hDlgWnd) || !::IsDialogMessage(hDlgWnd, &Msg)) {
			::TranslateMessage(&Msg); 
			::DispatchMessage(&Msg); 
		} 
	}
}
void IconWidget_::exitThread(void) {
	::PostMessage(hDlgWnd, WM_MY_CLOSE, NULL, NULL);
}

namespace TrayIcon
{
	std::wstring defaultCommand;
}

std::wstring getDlgItemText(HWND hDlg, int nIDDlgItem) {
#define BUFF_LEN 4096
	std::wstring ret;
	TCHAR *buffer = new TCHAR[BUFF_LEN+1];
	if (!GetDlgItemText(hDlg, nIDDlgItem, buffer, BUFF_LEN))
		buffer[0]=0;
	ret = buffer;
	delete [] buffer;
	return ret;
}
void updateDescFromCmd(HWND hDlg, std::wstring cmd) {
	std::wstring result = _T("");
	try {
		result = NSCModuleHelper::describeCommand(cmd);
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		result = _T("Error: ") + e.msg_;
	} catch (...) {
		result = _T("Unknown error!");
	}
	SetDlgItemText(hDlg, IDC_DESCRIPTION, result.c_str());
}
/*
INT_PTR CALLBACK DialogProc(          HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
);
*/
INT_PTR CALLBACK TrayIcon::InjectDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam) {
	switch (uMsg) 
	{
	case WM_INITDIALOG:
		{
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(NSCModuleWrapper::getModule(), MAKEINTRESOURCE(IDI_NSCP)));
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(NSCModuleWrapper::getModule(), MAKEINTRESOURCE(IDI_NSCP)));
			SetDlgItemText(hwndDlg, IDC_CMD_BOX, TrayIcon::defaultCommand.c_str());
			SetDlgItemText(hwndDlg, IDC_DESCRIPTION, _T("Loading commands, please wait..."));

			SendDlgItemMessage(hwndDlg, IDC_CMD_BOX, CB_RESETCONTENT, 0, 0); 
			std::wstring result = _T("");
			try {
				std::list<std::wstring> lst = NSCModuleHelper::getAllCommandNames();
				for (std::list<std::wstring>::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
					SendDlgItemMessage(hwndDlg, IDC_CMD_BOX, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>( (*cit).c_str() ));
				}
			} catch (NSCModuleHelper::NSCMHExcpetion &e) {
				result = _T("Error: ") + e.msg_;
			} catch (...) {
				result = _T("Unknown error!");
			}
			SetDlgItemText(hwndDlg, IDC_DESCRIPTION, result.c_str());
		}
		break;
	case WM_COMMAND: 
		switch (LOWORD(wParam))
		{
		case IDC_CMD_BOX: 
			switch(HIWORD(wParam)) 
			{
			case CBN_SELCHANGE:
				{
					std::wstring cmd;
					unsigned int id = SendDlgItemMessage(hwndDlg, IDC_CMD_BOX, CB_GETCURSEL, 0, 0); 
					unsigned int len = SendDlgItemMessage(hwndDlg, IDC_CMD_BOX, CB_GETLBTEXTLEN, id, 0);
					TCHAR *buf = new TCHAR[len+2];
					LRESULT ret;
					ret = SendDlgItemMessage(hwndDlg, IDC_CMD_BOX, CB_GETLBTEXT, id, reinterpret_cast<LPARAM>(buf));
					if (ret > 0 && ret <= len) {
						cmd = buf;
						updateDescFromCmd(hwndDlg, cmd);
					}
					delete [] buf;
				}
				break;
			case CBN_KILLFOCUS:
				updateDescFromCmd(hwndDlg, getDlgItemText(hwndDlg, IDC_CMD_BOX));
				break;
			}
			break;
		case IDC_INJECT:
			{
				std::wstring result = _T("");
				std::wstring cmd = getDlgItemText(hwndDlg, IDC_CMD_BOX);
				std::wstring args = getDlgItemText(hwndDlg, IDC_ARG_BOX);
				std::wstring msg;
				std::wstring perf;
				try {
					NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd, args, ' ', msg, perf);
					if (ret == NSCAPI::returnIgnored) {
						result = _T("Command not found!");
					} else {
						result = NSCHelper::translateReturn(ret);
					}
				} catch (NSCModuleHelper::NSCMHExcpetion &e) {
					result = _T("Error: ") + e.msg_;
				} catch (...) {
					result = _T("Unknown error!");
				}
				SetDlgItemText(hwndDlg, IDC_DESCRIPTION, result.c_str());
				SetDlgItemText(hwndDlg, IDC_MSG, msg.c_str());
				SetDlgItemText(hwndDlg, IDC_PERF, perf.c_str());
			}
			break;
		case IDOK: 
			{
				TCHAR *c=new TCHAR[1024];
				if (GetDlgItemText(hwndDlg, IDC_COMMAND, c, 1023))
					TrayIcon::defaultCommand = c;
				delete [] c;
			}

		case IDCANCEL: 
			EndDialog(hwndDlg, wParam); 
			return TRUE; 
		} 
	}
	return FALSE;
}



void insert_logrecord(HWND hwndLV, const SysTray::log_entry &entry) {
	LVITEM item;
	item.mask = LVIF_TEXT;
	std::wstring msg = NSCHelper::translateMessageType(entry.type);
	item.pszText = const_cast<TCHAR*>(msg.c_str());
	item.iSubItem = 0;
	item.iItem = 0; //ListView_GetItemCount(hwndLV);
	int id = ListView_InsertItem(hwndLV, &item);
	msg = entry.file;
	ListView_SetItemText(hwndLV, id, 1, const_cast<TCHAR*>(msg.c_str()));
	msg = strEx::itos(entry.line);
	ListView_SetItemText(hwndLV, id, 2, const_cast<TCHAR*>(msg.c_str()));
	msg = entry.message;
	ListView_SetItemText(hwndLV, id, 3, const_cast<TCHAR*>(msg.c_str()));
}

INT_PTR CALLBACK TrayIcon::LogDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam) {
	switch (uMsg) 
	{
	case WM_INITDIALOG:
		{
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(NSCModuleWrapper::getModule(), MAKEINTRESOURCE(IDI_NSCP)));
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(NSCModuleWrapper::getModule(), MAKEINTRESOURCE(IDI_NSCP)));
			gSysTray.setLogWindow(hwndDlg);
			HWND hwndLV = GetDlgItem(hwndDlg, IDC_LOG);
			LVCOLUMN col;
			col.mask = LVCF_TEXT|LVCF_WIDTH;
			col.cx = 10;
			col.pszText = _T("Type");
			ListView_InsertColumn(hwndLV, 1, &col);
			col.pszText = _T("File");
			ListView_InsertColumn(hwndLV, 2, &col);
			col.pszText = _T("Line");
			ListView_InsertColumn(hwndLV, 3, &col);
			col.pszText = _T("Message");
			ListView_InsertColumn(hwndLV, 4, &col);

			SysTray::log_type log = gSysTray.getLog();
			for (SysTray::log_type::const_iterator cit = log.begin(); cit != log.end(); ++cit) {
				insert_logrecord(hwndLV, *cit);
			}

			ListView_SetColumnWidth(hwndLV, 0, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hwndLV, 1, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hwndLV, 2, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hwndLV, 3, LVSCW_AUTOSIZE_USEHEADER);
		}
		return TRUE; 

	case WM_USER+1: 
		{
			HWND hwndLV = GetDlgItem(hwndDlg, IDC_LOG);
			const SysTray::log_entry* record = reinterpret_cast<const SysTray::log_entry*>(wParam);
			insert_logrecord(hwndLV, *record);

		}
		return TRUE; 

	case WM_SIZE:
		if (wParam == SIZE_RESTORED) {
			HWND hwndLV = GetDlgItem(hwndDlg, IDC_LOG);
			::SetWindowPos(hwndLV, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
		}
		return TRUE; 

	case WM_COMMAND: 
		case IDOK: 
		case IDCANCEL: 
			gSysTray.setLogWindow(NULL);
			EndDialog(hwndDlg, wParam); 
			return TRUE; 
	}
	return FALSE;
}


INT_PTR CALLBACK TrayIcon::DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg) 
	{
	case WM_DESTROY:
		TrayIcon::removeIcon(hwndDlg);
		PostQuitMessage(0);
		return 0;

	case WM_INITDIALOG:
		addIcon(hwndDlg);
		break;

	case WM_ICON_NOTIFY:
		if (lParam==WM_RBUTTONDOWN) {
			HMENU hMenu = LoadMenu(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDR_POPUP));
			HMENU hSubMenu = GetSubMenu(hMenu, 0);
			const RECT r = {0, 0, 0, 0};
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hwndDlg);
			int cmd = TrackPopupMenu(hSubMenu, TPM_LEFTALIGN|TPM_RETURNCMD, pt.x, pt.y, 0, hwndDlg, &r);
			DestroyMenu(hMenu);
			switch (cmd) {
			case ID_POPUP_STOPSERVICE:
				NSCModuleHelper::Exit();
				break;
			case ID_POPUP_INJECTCOMMAND:
				//if (TrayIcon::defaultCommand.empty())
				//	TrayIcon::defaultCommand = NSCModuleHelper::getSettingsString(_T("systray"), _T("defaultCommand"), _T(""));
				CreateDialog(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_INJECTDIALOG),hwndDlg,InjectDialogProc);
				break;
			case ID_POPUP_SHOWLOG:
				CreateDialog(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_LOGWINDOW),hwndDlg,LogDialogProc);
			}
			return TRUE;
		}
		break;
	}
	return FALSE;
}
void TrayIcon::addIcon(HWND hWnd) {
	assert(NSCModuleWrapper::getModule() != NULL);
	assert(hWnd != NULL);

	NOTIFYICONDATA ndata;
	ndata.cbSize=sizeof(NOTIFYICONDATA);
	ndata.hWnd=hWnd;
	ndata.uID=2000;
	ndata.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	ndata.uCallbackMessage=WM_ICON_NOTIFY;
	ndata.hIcon=::LoadIcon(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDI_NSCP));
	std::wstring title = NSCModuleHelper::getApplicationName() + _T(" - ") + NSCModuleHelper::getApplicationVersionString();
	wcsncpy_s(ndata.szTip, 64, title.c_str(), min(64, title.size()));
	Shell_NotifyIcon(NIM_ADD,&ndata);
}

void TrayIcon::removeIcon(HWND hWnd) {
	NOTIFYICONDATA ndata;
	ndata.cbSize=sizeof(NOTIFYICONDATA);
	ndata.hWnd=hWnd;
	ndata.uID=2000;
	Shell_NotifyIcon(NIM_DELETE,&ndata);
}