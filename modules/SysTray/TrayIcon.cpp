#include "stdafx.h"
#include ".\trayicon.h"
#include "resource.h"
#include <strEx.h>
#include <ShellAPI.h>

namespace TrayIcon
{
	HWND ghDlgWnd = NULL;
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

INT_PTR CALLBACK TrayIcon::DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	DWORD tmp = 0;
	switch (uMsg) 
	{
	case WM_INITDIALOG:
		addIcon(hwndDlg);
//		return TRUE;
		break;

	case WM_ICON_NOTIFY:
		if (lParam==WM_RBUTTONDOWN) {
			HMENU hMenu = LoadMenu(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDR_POPUP));
			HMENU hSubMenu = GetSubMenu(hMenu, 0);
			const RECT r = {0, 0, 0, 0};
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(ghDlgWnd);
			int cmd = TrackPopupMenu(hSubMenu, TPM_LEFTALIGN|TPM_RETURNCMD, pt.x, pt.y, 0, ghDlgWnd, &r);
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
				if ((tmp = (INT)ShellExecute(ghDlgWnd, "open", (NSCModuleHelper::getBasePath() + NSCModuleHelper::getSettingsString("log", "file", "")).c_str(), NULL, NULL, SW_SHOWNORMAL))<=32) {
					NSC_LOG_ERROR("ShellExecute failed : " + strEx::itos((INT)tmp));
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

	ghDlgWnd = ::CreateDialog(NSCModuleWrapper::getModule(),MAKEINTRESOURCE(IDD_NSTRAYDLG),NULL,DialogProc);

	MSG Msg;
	while(::GetMessage(&Msg, ghDlgWnd, 0, 0))
	{
		if (Msg.message == WM_MY_CLOSE)
			break;
		if (!::IsWindow(ghDlgWnd) || !::IsDialogMessage(ghDlgWnd, &Msg)) {
			::TranslateMessage(&Msg); 
			::DispatchMessage(&Msg); 
		} 
	}

	::DestroyWindow(ghDlgWnd);
	::ReleaseMutex(ghMutex);
}
void TrayIcon::destroyDialog(void) {
	::PostMessage(ghDlgWnd, WM_MY_CLOSE, NULL, NULL);
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
