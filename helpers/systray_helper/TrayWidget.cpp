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
#include <config.h>
#include "TrayWidget.h"
#include "resource.h"
#include <commctrl.h>
#include <strEx.h>
#include <ShellAPI.h>
#include "TrayWidget.h"
#include <error.hpp>
#include <file_logger.hpp>
#include <ServiceCmd.h>


logging::file_logger g_log_instance(_T("nsclient++"),_T("systray.log"));
HINSTANCE ghInstance = NULL;
TrayWidget *gTrayInstance = NULL;

#define LOG_ERROR_FILE(x) g_log_instance.log(_T("error"), __FILEW__, __LINE__, std::wstring(x).c_str());
#define LOG_MESSAGE_FILE(x) g_log_instance.log(_T("message"), __FILEW__, __LINE__, std::wstring(x).c_str());

#define LOG_ERROR_TRAY(x) log(_T("error"), __FILEW__, __LINE__, std::wstring(x).c_str());
#define LOG_MESSAGE_TRAY(x) log(_T("message"), __FILEW__, __LINE__, std::wstring(x).c_str());

#define LOG_ERROR_TO_TRAY(x) gTrayInstance->log(_T("error"), __FILEW__, __LINE__, std::wstring(x).c_str());
#define LOG_MESSAGE_TO_TRAY(x) gTrayInstance->log(_T("message"), __FILEW__, __LINE__, std::wstring(x).c_str());

#if WINVER < 0x0600
#define MSGFLT_ADD 1
#define MSGFLT_REMOVE 2
typedef BOOL (WINAPI *LPFN_CHANGEWINDOWMESSAGEFILTER) (UINT, DWORD);
LPFN_CHANGEWINDOWMESSAGEFILTER fnChangeWindowMessageFilter = NULL;
BOOL ChangeWindowMessageFilter(UINT message, DWORD what)
{
	if (fnChangeWindowMessageFilter == NULL) {
		HMODULE hMod = GetModuleHandle(TEXT("user32"));
		if (hMod == NULL)
			return FALSE;
		fnChangeWindowMessageFilter = (LPFN_CHANGEWINDOWMESSAGEFILTER)GetProcAddress(hMod,"ChangeWindowMessageFilter");
	}
	if (fnChangeWindowMessageFilter == NULL) {
		LOG_ERROR_FILE(_T("Could not find ChangeWindowMessageFilter: ") + error::lookup::last_error());
		return TRUE;
	}
	LOG_ERROR_FILE(_T("registred windows thingy..."));
	return fnChangeWindowMessageFilter(message,what);
}
#endif

std::wstring getArgumentValue(std::wstring key, strEx::splitList list) {
	for (strEx::splitList::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
		if ((*cit) == key) {
			if (++cit == list.end())
				return _T("");
			return *cit;
		}
	}
	return _T("");
}
TrayWidget::TrayWidget(std::wstring cmdLine) {
	strEx::splitList list = strEx::splitEx(cmdLine, _T(" "));
	channel_id_ = getArgumentValue(_T("-channel"), list);
	if (channel_id_.empty()) {
		channel_id_ = _T("_solo_") + strEx::itos(GetCurrentProcessId()) + _T("_");
	}
	LOG_MESSAGE_TRAY(_T("Attempting to launch system tray module for ") + channel_id_);
	gTrayInstance = this;
	try {
		shared_client_.reset(new nsclient_session::shared_client_session(channel_id_, this));
		if (shared_client_.get() != NULL)
			shared_client_->attach_to_session(channel_id_);
	} catch (nsclient_session::session_exception e) {
		LOG_ERROR_TRAY(_T("Failed to attach to shared session: ") + e.what());
	}
}
TrayWidget::~TrayWidget() {
	gTrayInstance = NULL;
	try {
		if (shared_client_.get() != NULL) {
			LOG_ERROR_FILE(_T("Destroying shared session..."));
			shared_client_->close_session();
		}
	} catch (nsclient_session::session_exception e) {
		LOG_ERROR_FILE(_T("Failed to close shared session: ") + e.what());
	} catch (...) {
		LOG_ERROR_FILE(_T("Failed to close shared session: Unknown Exception"));
	}
	shared_client_.reset();
}
void TrayWidget::connectService() {
	LOG_MESSAGE_TRAY(_T("Reconnecting to the service..."));
	try {
		shared_client_.reset(new nsclient_session::shared_client_session(channel_id_, this));
		if (shared_client_.get() != NULL)
			shared_client_->attach_to_session(channel_id_);
	} catch (nsclient_session::session_exception e) {
		LOG_ERROR_TRAY(_T("Failed to attach to shared session: ") + e.what());
	}
}
int TrayWidget::inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf) {
	if (shared_client_.get() == NULL) {
		LOG_ERROR_TRAY(_T("No active shared instance!"));
		return -1;
	}
	return shared_client_->inject(command, arguments, splitter, escape, msg, perf);
}




void TrayWidget::createDialog(HINSTANCE hInstance) {
	LOG_MESSAGE_TRAY(_T("Creating dialog..."));
	ghInstance = hInstance;
	//hDlgWnd = ::CreateDialog(hInstance,MAKEINTRESOURCE(IDD_NSTRAYDLG),NULL,TrayIcon::DialogProc);
	//if ((hDlgWnd == NULL)||!IsWindow(hDlgWnd)) {
//		LOG_ERROR_TRAY(_T("Failed to create windows: ") + error::lookup::last_error());
//	}

	WNDCLASSEX wndclass;
	wndclass.lpszMenuName=NULL;
	wndclass.cbSize=sizeof(wndclass);
	wndclass.lpfnWndProc=TrayIcon::DialogProc;
	wndclass.cbClsExtra=0;
	wndclass.cbWndExtra=0;
	wndclass.hInstance=hInstance;
	wndclass.hIcon=NULL;
	wndclass.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.hCursor=LoadCursor(NULL,IDC_ARROW);
	wndclass.hIconSm=NULL;
	wndclass.lpszClassName=_T("NSClient_pp_TrayClass");
	wndclass.style=0;
	// register task bar restore event after crash
	//WM_TASKBARCREATED=RegisterWindowMessage(TEXT("TaskbarCreated"));
	//MyChangeWindowMessageFilter(WM_TASKBARCREATED, MSGFLT_ADD); 

	UINT UDM_TASKBARCREATED = RegisterWindowMessage(_T("TaskbarCreated"));
	if (UDM_TASKBARCREATED == 0) {
		LOG_ERROR_TRAY(_T("Failed to register 'TaskbarCreated': ") + error::lookup::last_error());
	}
	if (!ChangeWindowMessageFilter(UDM_TASKBARCREATED, MSGFLT_ADD)) {
		LOG_ERROR_TRAY(_T("Failed to cchange window filter: ") + error::lookup::last_error());
	}

	if (!RegisterClassEx(&wndclass)) {
		LOG_ERROR_TRAY(_T("Failed to register window class: ") + error::lookup::last_error());
	}

	MSG msg;
	hDlgWnd=CreateWindow(_T("NSClient_pp_TrayClass"),NULL,0,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,NULL,NULL,hInstance,NULL);
	if(hDlgWnd==NULL)
	{
		LOG_ERROR_TRAY(_T("Failed to create window: ") + error::lookup::last_error());
		return;
	}
	while(GetMessage(&msg,NULL,0,0))
	{
		if (msg.message == WM_MY_CLOSE) {
			::DestroyWindow(hDlgWnd);
		} else if (msg.message == UDM_TASKBARCREATED) {
			LOG_MESSAGE_TRAY(_T("Recreating systray icon..."));
			TrayIcon::addIcon(msg.hwnd);
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return;
	
	/*
	MSG Msg;
	BOOL bRet;
	while((bRet = ::GetMessage(&Msg, NULL, 0, 0)) != 0)
	{
		if (Msg.message == WM_MY_CLOSE) {
			::DestroyWindow(hDlgWnd);
		} else if (Msg.message == UDM_TASKBARCREATED) {
			LOG_MESSAGE_TRAY(_T("Recreating systray icon..."));
			TrayIcon::addIcon(Msg.hwnd);
		} else if (bRet == -1) {
			// handle the error and possibly exit
			LOG_ERROR_TRAY(_T("Wonder what this is... please let me know..."));
			return;
		} else {
		//} else if (!::IsWindow(hDlgWnd) || !::IsDialogMessage(hDlgWnd, &Msg)) {
			::TranslateMessage(&Msg); 
			::DispatchMessage(&Msg); 
		} 
	}
	*/
}

void TrayWidget::session_error(std::wstring file, unsigned int line, std::wstring msg) {
	log(_T("error"), file.c_str(), line, msg);
}

void TrayWidget::session_log_message(int msgType, const TCHAR* file, const int line, std::wstring message) {
	log(_T("message"), file, line, message);
}
void TrayWidget::log(std::wstring category, const TCHAR* file, const int line, std::wstring message) {
	log_entry record(category, file, line, message);
	g_log_instance.log(_T("error"), record.file.c_str(), record.line, record.message.c_str());
	HWND hWnd = NULL;
	{
		try {
			MutexLock lock(logLock);
			if (lock.hasMutex()) {
				log_.push_back(record);
				if (log_.size() > 50)
					log_.pop_front();
				hWnd = hLogWnd;
			} else {
				g_log_instance.log(_T("error"), __FILEW__, __LINE__, _T("Failed to get mutex in logger, message discarded (follows)"));
				g_log_instance.log(_T("discarded"), record.file.c_str(), record.line, record.message.c_str());
			}
		} catch (...) {
			g_log_instance.log(_T("error"), __FILEW__, __LINE__, _T("Failed to get mutex in logger, message discarded (follows)"));
			g_log_instance.log(_T("discarded"), record.file.c_str(), record.line, record.message.c_str());
		}
	}
	if (hWnd) {
		SendMessage(hWnd, WM_USER+1, reinterpret_cast<WPARAM>(&record), NULL);
	}
}

TrayWidget::log_type TrayWidget::getLog() {
	log_type ret;
	for (log_type::const_iterator cit = log_.begin(); cit != log_.end(); ++cit)
		ret.push_back(*cit);
	return ret;
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
		result = _T("TODO: re-implement this!"); //NSCModuleHelper::describeCommand(cmd);
		//} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		//	result = _T("Error: ") + e.msg_;
	} catch (...) {
		result = _T("Unknown error!");
	}
	SetDlgItemText(hDlg, IDC_DESCRIPTION, result.c_str());
}


class worker_thread {
public:
	struct start_block {
		std::wstring cmd;
		HMENU hMenu;
		HWND hWnd;
	};

	void update(HMENU hMenu, UINT pos, LPTSTR text) {
		if (!ModifyMenu(hMenu, pos, MF_BYCOMMAND|MF_STRING|MF_GRAYED, pos, text))
			LOG_ERROR_TO_TRAY(_T("Failed to update menu: ") + error::lookup::last_error());
	}

	DWORD threadProc(LPVOID lpParameter) {
		start_block* param = static_cast<start_block*>(lpParameter);
		std::wstring cmd = param->cmd;
		HMENU hMenu = param->hMenu;
		HWND hWnd = param->hWnd;
		delete param;
		if (cmd == _T("update-status")) {
			update(hMenu, ID_SERVICE_STATUS, _T("getting..."));
			try {
				if (serviceControll::isStarted(SZSERVICENAME)) {
					update(hMenu, ID_SERVICE_STATUS, _T("Started"));
				} else {
					update(hMenu, ID_SERVICE_STATUS, _T("Stopped"));
				}
			} catch (serviceControll::SCException &e) {
				LOG_ERROR_TO_TRAY(_T("Failed to get service information: ") + e.error_);
			} catch (...) {
				LOG_ERROR_TO_TRAY(_T("Failed to get service information: Unknown error"));
			}
		} else if (cmd == _T("start-service")) {
			update(hMenu, ID_SERVICE_STATUS, _T("Starting"));
			try {
				serviceControll::Start(SZSERVICENAME);
				update(hMenu, ID_SERVICE_STATUS, _T("Started"));
			} catch (serviceControll::SCException &e) {
				update(hMenu, ID_SERVICE_STATUS, _T("Unknown (failed to start)"));
				LOG_ERROR_TO_TRAY(_T("Failed to get service information: ") + e.error_);
			} catch (...) {
				update(hMenu, ID_SERVICE_STATUS, _T("Unknown (failed to start)"));
				LOG_ERROR_TO_TRAY(_T("Failed to get service information: Unknown error"));
			}
		} else if (cmd == _T("stop-service")) {
			update(hMenu, ID_SERVICE_STATUS, _T("Stopped"));
			try {
				serviceControll::Stop(SZSERVICENAME);
				update(hMenu, ID_SERVICE_STATUS, _T("Stopped"));
			} catch (serviceControll::SCException &e) {
				update(hMenu, ID_SERVICE_STATUS, _T("Unknown (failed to stop)"));
				LOG_ERROR_TO_TRAY(_T("Failed to get service information: ") + e.error_);
			} catch (...) {
				update(hMenu, ID_SERVICE_STATUS, _T("Unknown (failed to stop)"));
				LOG_ERROR_TO_TRAY(_T("Failed to get service information: Unknown error"));
			}
		} else if (cmd == _T("inject-button-pressed")) {
			std::wstring result = _T("");
			std::wstring cmd = getDlgItemText(hWnd, IDC_CMD_BOX);
			std::wstring args = getDlgItemText(hWnd, IDC_ARG_BOX);
			std::wstring msg;
			std::wstring perf;
			try {
				int ret = gTrayInstance->inject(cmd, args, L' ', true, msg, perf);
				//NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd, args, ' ', msg, perf);
				/*
				if (ret == NSCAPI::returnIgnored) {
				result = _T("Command not found!");
				} else {
				result = NSCHelper::translateReturn(ret);
				}
				*/
				result = _T("TODO: re-implement this: ") + strEx::itos(ret);
				//} catch (NSCModuleHelper::NSCMHExcpetion &e) {
				//	result = _T("Error: ") + e.msg_;
			} catch (...) {
				result = _T("Unknown error!");
			}
			SetDlgItemText(hWnd, IDC_DESCRIPTION, result.c_str());
			SetDlgItemText(hWnd, IDC_MSG, msg.c_str());
			SetDlgItemText(hWnd, IDC_PERF, perf.c_str());
		}
		return 0;
	}
	static LPVOID init(std::wstring command, HMENU hMenu, HWND hWnd = NULL) {
		start_block *sb = new start_block;
		sb->cmd = command;
		sb->hMenu = hMenu;
		sb->hWnd = hWnd;
		return sb;
	}

	static void updateServiceStatus(HMENU hMenu) {
		Thread<worker_thread> *pThread = new Thread<worker_thread>(_T("tray-worker-thread"));
		pThread->createThread(worker_thread::init(_T("update-status"), hMenu));
	}
	static void startService(HMENU hMenu) {
		Thread<worker_thread> *pThread = new Thread<worker_thread>(_T("tray-worker-thread"));
		pThread->createThread(worker_thread::init(_T("start-service"), hMenu));
	}
	static void stopService(HMENU hMenu) {
		Thread<worker_thread> *pThread = new Thread<worker_thread>(_T("tray-worker-thread"));
		pThread->createThread(worker_thread::init(_T("stop-service"), hMenu));
	}
	static void injectDialog(HWND hWnd) {
		Thread<worker_thread> *pThread = new Thread<worker_thread>(_T("tray-worker-thread"));
		pThread->createThread(worker_thread::init(_T("inject-button-pressed"), NULL, hWnd));
	}
};



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
		{
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_NSCP)));
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_NSCP)));
			SetDlgItemText(hwndDlg, IDC_CMD_BOX, TrayIcon::defaultCommand.c_str());
			SetDlgItemText(hwndDlg, IDC_DESCRIPTION, _T("Loading commands, please wait..."));

			SendDlgItemMessage(hwndDlg, IDC_CMD_BOX, CB_RESETCONTENT, 0, 0); 
			std::wstring result = _T("");
			/*
			TODO! re-implement this!
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
			*/
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
			worker_thread::injectDialog(hwndDlg);
			/*
			{
				std::wstring result = _T("");
				std::wstring cmd = getDlgItemText(hwndDlg, IDC_CMD_BOX);
				std::wstring args = getDlgItemText(hwndDlg, IDC_ARG_BOX);
				std::wstring msg;
				std::wstring perf;
				try {
					int ret = gTrayInstance->inject(cmd, args, _T(" "), true, msg, perf);
					//NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd, args, ' ', msg, perf);
					/ *
					if (ret == NSCAPI::returnIgnored) {
						result = _T("Command not found!");
					} else {
						result = NSCHelper::translateReturn(ret);
					}
					* /
					result = _T("TODO: re-implement this: ") + strEx::itos(ret);
				//} catch (NSCModuleHelper::NSCMHExcpetion &e) {
				//	result = _T("Error: ") + e.msg_;
				} catch (...) {
					result = _T("Unknown error!");
				}
				SetDlgItemText(hwndDlg, IDC_DESCRIPTION, result.c_str());
				SetDlgItemText(hwndDlg, IDC_MSG, msg.c_str());
				SetDlgItemText(hwndDlg, IDC_PERF, perf.c_str());
			}
	*/
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



void insert_logrecord(HWND hwndLV, const TrayWidget::log_entry &entry) {
	LVITEM item;
	item.mask = LVIF_TEXT;
	std::wstring msg = entry.category;
	item.pszText = const_cast<TCHAR*>(msg.c_str());
	item.iSubItem = 0;
	item.iItem = 0; //ListView_GetItemCount(hwndLV);
	int id = ListView_InsertItem(hwndLV, &item);
	msg = entry.file;

	std::wstring::size_type pos = msg.find_last_of(_T("\\"));
	if (pos != std::wstring::npos)
		msg = msg.substr(pos);
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
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_NSCP)));
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_NSCP)));
			gTrayInstance->setLogWindow(hwndDlg);
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
			TrayWidget::log_type log = gTrayInstance->getLog();
			for (TrayWidget::log_type::const_iterator cit = log.begin(); cit != log.end(); ++cit) {
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
			const TrayWidget::log_entry* record = reinterpret_cast<const TrayWidget::log_entry*>(wParam);
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
			gTrayInstance->setLogWindow(NULL);
			EndDialog(hwndDlg, wParam); 
			return TRUE; 
	}
	return FALSE;
}
namespace TrayIcon
{
	UINT UDM_TASKBARCREATED = -1;
	HMENU hPopupMenu_ = NULL;
}
//LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK TrayIcon::DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (uMsg == UDM_TASKBARCREATED) {
		addIcon(hwndDlg);
		LOG_MESSAGE_TO_TRAY(_T("UDM_TASKBARCREATED"));
	}

	switch (uMsg) 
	{
	case WM_DESTROY:
		if (hPopupMenu_ != NULL)
			DestroyMenu(hPopupMenu_);
		TrayIcon::removeIcon(hwndDlg);
		PostQuitMessage(0);
		return 0;

	case WM_CREATE:
	case WM_INITDIALOG:
		LOG_MESSAGE_TO_TRAY(_T("WM_INITDIALOG"));


		UDM_TASKBARCREATED = RegisterWindowMessage(_T("TaskbarCreated"));
		if (UDM_TASKBARCREATED == 0) {
			LOG_MESSAGE_TO_TRAY(_T("Failed to register 'TaskbarCreated': ") + error::lookup::last_error());
		}
		if (!ChangeWindowMessageFilter(UDM_TASKBARCREATED, MSGFLT_ADD)) {
			LOG_MESSAGE_TO_TRAY(_T("Failed to cchange window filter: ") + error::lookup::last_error());
		}

		addIcon(hwndDlg);
		break;

	case WM_COMMAND:
		if (HIWORD(wParam) == 0) {
			switch (wParam) 
			{
			case ID_POPUP_CONNECT:
				gTrayInstance->connectService();
				break;
			case ID_POPUP_CLOSE:
				::PostMessage(hwndDlg, WM_MY_CLOSE, NULL, NULL);
				break;
			case ID_SERVICE_STATUS:
				worker_thread::updateServiceStatus(GetSubMenu(hPopupMenu_, 0));
				break;
			case ID_SERVICE_START:
				worker_thread::startService(GetSubMenu(hPopupMenu_, 0));
				break;
			case ID_SERVICE_STOP:
				worker_thread::stopService(GetSubMenu(hPopupMenu_, 0));
				break;
			case ID_POPUP_SHOWLOG:
				if (IsWindow(gTrayInstance->getLogWindow())) {
					::ShowWindow(gTrayInstance->getLogWindow(), SW_SHOW);
					::SetForegroundWindow(gTrayInstance->getLogWindow());
				}
				else
					CreateDialog(ghInstance,MAKEINTRESOURCE(IDD_LOGWINDOW),hwndDlg,LogDialogProc);
				break;
			case ID_POPUP_INJECTCOMMAND:
				//if (TrayIcon::defaultCommand.empty())
				//	TrayIcon::defaultCommand = NSCModuleHelper::getSettingsString(_T("systray"), _T("defaultCommand"), _T(""));
				CreateDialog(ghInstance,MAKEINTRESOURCE(IDD_INJECTDIALOG),hwndDlg,InjectDialogProc);
				break;
			}
		}
		break;

	case WM_ICON_NOTIFY:
		if (lParam==WM_RBUTTONDOWN) {
			bool created = false;
			if (hPopupMenu_ == NULL) {
				hPopupMenu_ = LoadMenu(ghInstance,MAKEINTRESOURCE(IDR_POPUP));
				created = true;
			}
			HMENU hSubMenu = GetSubMenu(hPopupMenu_, 0);
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hwndDlg);
			TrackPopupMenu(hSubMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hwndDlg, NULL);
			PostMessage(hwndDlg, WM_NULL, 0, 0);
			if (created)
				worker_thread::updateServiceStatus(GetSubMenu(hPopupMenu_, 0));
			return TRUE;
		}
		break;
	}
	return DefWindowProc(hwndDlg,uMsg,wParam,lParam);
	//return FALSE;
}
void TrayIcon::addIcon(HWND hWnd) {
	NOTIFYICONDATA ndata;
	ndata.cbSize=sizeof(NOTIFYICONDATA);
	ndata.hWnd=hWnd;
	ndata.uID=2000;
	ndata.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	ndata.uCallbackMessage=WM_ICON_NOTIFY;
	ndata.hIcon=::LoadIcon(ghInstance,MAKEINTRESOURCE(IDI_NSCP));
	std::wstring title = _T("NSClient++ SystemTray (TODO)"); //NSCModuleHelper::getApplicationName() + _T(" - ") + NSCModuleHelper::getApplicationVersionString();
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