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
		SetDlgItemText(hwndDlg, IDC_COMMAND, TrayIcon::defaultCommand.c_str());
	case WM_COMMAND: 
		switch (LOWORD(wParam))
		{
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
					// @todo NSCModuleHelper::InjectCommand(TrayIcon::defaultCommand);
				}
				break;
			case ID_POPUP_SHOWLOG:
				{
					long long err = reinterpret_cast<long long>(ShellExecute(hwndDlg, _T("open"), 
						(NSCModuleHelper::getBasePath() + NSCModuleHelper::getSettingsString(_T("log"), _T("file"), _T(""))).c_str(), 
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
	wcsncpy_s(ndata.szTip, 128, (NSCModuleHelper::getApplicationName() + _T(" - ") + NSCModuleHelper::getApplicationVersionString()).c_str(), 63);
	Shell_NotifyIcon(NIM_ADD,&ndata);
}

void TrayIcon::removeIcon(HWND hWnd) {
	NOTIFYICONDATA ndata;
	ndata.cbSize=sizeof(NOTIFYICONDATA);
	ndata.hWnd=hWnd;
	ndata.uID=2000;
	Shell_NotifyIcon(NIM_DELETE,&ndata);
}