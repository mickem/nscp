#include "stdafx.h"
#include ".\trayicon.h"
#include "resource.h"
#include <ShellAPI.h>

namespace TrayIcon
{
	HWND ghDlgWnd;
	HANDLE ghMutex;
	std::string defaultCommand;
}

BOOL CALLBACK TrayIcon::InjectDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam) {
	switch (uMsg) 
	{
	case WM_INITDIALOG:
		SetDlgItemText(hwndDlg, IDC_COMMAND, TrayIcon::defaultCommand.c_str());
	case WM_COMMAND: 
		switch (LOWORD(wParam))
		{
		case IDOK: 
			{
				char *c=new char[1024];
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

BOOL CALLBACK TrayIcon::DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg) 
	{
	case WM_INITDIALOG:
		ShowWindow(hwndDlg, SW_HIDE);
		SetWindowPos(hwndDlg,NULL,-10,-10,0,0,SWP_NOZORDER|SWP_NOMOVE);
		ghDlgWnd = hwndDlg;
		addIcon();
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
					TrayIcon::defaultCommand = NSCModuleHelper::getSettingsString("systray", "defaultCommand", "");
				if (DialogBox(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_INJECTDIALOG),NULL,InjectDialogProc) == IDOK) {
					NSCModuleHelper::InjectCommand(TrayIcon::defaultCommand);
				}
				break;
			case ID_POPUP_SHOWLOG:
				ShellExecute(hwndDlg, "notepad", NSCModuleHelper::getSettingsString("log", "file", "").c_str(), NULL, NULL, SW_SHOWNORMAL);
			}
		}
		break;
	case WM_MY_CLOSE:
		EndDialog(ghDlgWnd, 0);
		break;
	}
	return FALSE;
}
void TrayIcon::addIcon(void) {
	NOTIFYICONDATA ndata;
	ndata.cbSize=sizeof(NOTIFYICONDATA);
	ndata.hWnd=ghDlgWnd;
	ndata.uID=2000;
	ndata.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	ndata.uCallbackMessage=WM_ICON_NOTIFY;
	ndata.hIcon=::LoadIcon(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDI_STANDBY));
	strncpy(ndata.szTip,(NSCModuleHelper::getApplicationName() + " - " + NSCModuleHelper::getApplicationVersionString()).c_str(), 63);
	Shell_NotifyIcon(NIM_ADD,&ndata);
}

void TrayIcon::removeIcon(void) {
	NOTIFYICONDATA ndata;
	ndata.hWnd=ghDlgWnd;
	ndata.uID=2000;
	Shell_NotifyIcon(NIM_DELETE,&ndata);
}

void TrayIcon::createDialog(void) {
	ghMutex = ::CreateMutex(NULL, TRUE, NULL);
	if (!ghMutex)
		throw std::string("Could not create mutex.");
	DialogBox(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_NSTRAYDLG),NULL,DialogProc);
	::ReleaseMutex(ghMutex);
}
void TrayIcon::destroyDialog(void) {
	::SendMessage(ghDlgWnd, WM_MY_CLOSE, NULL, NULL);
}
bool TrayIcon::waitForTermination(DWORD timeout /* = 5000L */) {
	DWORD dwWaitResult = WaitForSingleObject(ghMutex, timeout);
	switch (dwWaitResult) {
		// The thread got mutex ownership.
	case WAIT_OBJECT_0: 
		ReleaseMutex(ghMutex);
		CloseHandle(ghMutex);
		return true;
		// Cannot get mutex ownership due to time-out.
	case WAIT_TIMEOUT: 
		return false; 

		// Got ownership of the abandoned mutex object.
	case WAIT_ABANDONED: 
		return false; 
	}
	return false;
}
