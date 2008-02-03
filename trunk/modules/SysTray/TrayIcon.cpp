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
#include ".\trayicon.h"
#include "resource.h"
#include <commctrl.h>
#include <strEx.h>
#include <ShellAPI.h>

unsigned IconWidget_::threadProc(LPVOID lpParameter)
{
	createDialog();
	return 0;
}


void IconWidget_::createDialog(void) {
	hDlgWnd = ::CreateDialog(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_NSTRAYDLG),NULL,TrayIcon::DialogProc);

	MSG Msg;
	while(::GetMessage(&Msg, hDlgWnd, 0, 0))
	{
		if (Msg.message == WM_MY_CLOSE)
			break;
		if (!::IsWindow(hDlgWnd) || !::IsDialogMessage(hDlgWnd, &Msg)) {
			::TranslateMessage(&Msg); 
			::DispatchMessage(&Msg); 
		} 
	}
	TrayIcon::removeIcon(hDlgWnd);

	::DestroyWindow(hDlgWnd);
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
#define BUFF_LEN 4096
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

INT_PTR CALLBACK TrayIcon::DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg) 
	{
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
				NSCModuleHelper::StopService();
				break;
			case ID_POPUP_INJECTCOMMAND:
				if (TrayIcon::defaultCommand.empty())
					TrayIcon::defaultCommand = NSCModuleHelper::getSettingsString(_T("systray"), _T("defaultCommand"), _T(""));
				if (DialogBox(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_INJECTDIALOG),NULL,InjectDialogProc) == IDOK) {
					//NSCModuleHelper::InjectSplitAndCommand(TrayIcon::defaultCommand, buffer);
				}
				break;
			case ID_POPUP_SHOWLOG:
				{
					long long err = reinterpret_cast<long long>(ShellExecute(hwndDlg, _T("open"), 
						(NSCModuleHelper::getBasePath() + _T("\\") + NSCModuleHelper::getSettingsString(_T("log"), _T("file"), _T(""))).c_str(), 
						NULL, NULL, SW_SHOWNORMAL));
					if (err <=32) {
							NSC_LOG_ERROR_STD(_T("ShellExecute failed : ") + strEx::itos(err));
						}
				}
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
	ndata.hIcon=::LoadIcon(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDI_STANDBY));
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