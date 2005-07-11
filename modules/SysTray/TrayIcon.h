#pragma once

#include <thread.h>

#define WM_ICON_NOTIFY	WM_USER+1
#define WM_MY_CLOSE	WM_USER+2

class IconWidget_ {
public:
	void threadProc(LPVOID lpParameter);
	void exitThread(void);

private:
	void createDialog(void);
	void destroyDialog(void);

private:
	HWND hDlgWnd;

};

typedef Thread<IconWidget_> IconWidget;


namespace TrayIcon
{
	INT_PTR CALLBACK DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	BOOL CALLBACK InjectDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	void removeIcon(HWND hWnd);
	void addIcon(HWND hWnd);
}