#pragma once

#define WM_ICON_NOTIFY	WM_USER+1
#define WM_MY_CLOSE	WM_USER+2

namespace TrayIcon
{
	INT_PTR CALLBACK DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	BOOL CALLBACK InjectDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	void createDialog(void);
	void destroyDialog(void);
	void removeIcon(void);
	void addIcon(HWND hWnd);
	bool waitForTermination(DWORD timeout = 5000L);
}