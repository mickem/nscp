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

#include <thread.h>

#define WM_ICON_NOTIFY	WM_USER+1
#define WM_MY_CLOSE	WM_USER+2

class IconWidget_ {
public:
	unsigned threadProc(LPVOID lpParameter);
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
	INT_PTR CALLBACK InjectDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	INT_PTR CALLBACK LogDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	void setupUI();
	
	void removeIcon(HWND hWnd);
	void addIcon(HWND hWnd);
}