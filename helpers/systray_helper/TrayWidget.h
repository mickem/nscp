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
#include <string>
#include <list>
#include <nsclient_session.hpp>
#include <file_logger.hpp>

#define WM_ICON_NOTIFY	WM_USER+1
#define WM_MY_CLOSE	WM_USER+2

class TrayWidget : public nsclient_session::session_handler_interface {
public:
	struct log_entry {
		log_entry(std::wstring category_, std::wstring file_, int line_, std::wstring message_) : category(category_), file(file_), line(line_), message(message_) {

		}
		std::wstring category;
		std::wstring file;
		int line;
		std::wstring message;
	};
	typedef std::list<log_entry> log_type;

private:
	std::auto_ptr<nsclient_session::shared_client_session> shared_client_;
	log_type log_;
	MutexHandler logLock;
	HWND hDlgWnd;
	HWND hLogWnd;
	std::wstring channel_id_;

public:
	TrayWidget(std::wstring cmdLine);
	~TrayWidget();
	void createDialog(HINSTANCE hInstance);
	log_type getLog();
	void connectService();


public: // Shared session interface:
	void log(std::wstring category, const TCHAR* file, const int line, std::wstring message);
	void session_error(std::wstring file, unsigned int line, std::wstring msg);
	void session_log_message(int msgType, const TCHAR* file, const int line, std::wstring message);
	int session_inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf) {
		return -1;
	}


	void setLogWindow(HWND hWnd) { hLogWnd = hWnd; }
	HWND getLogWindow() const { return hLogWnd; }
	int inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf);


};

namespace TrayIcon
{
	INT_PTR CALLBACK DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	INT_PTR CALLBACK InjectDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	INT_PTR CALLBACK LogDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	void setupUI();
	
	void removeIcon(HWND hWnd);
	void addIcon(HWND hWnd);
}