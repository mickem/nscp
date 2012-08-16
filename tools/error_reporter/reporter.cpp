// systray_helper.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "reporter.h"
#include <ServiceCmd.h>
#include <iostream>
#include <file_helpers.hpp>
#include <strEx.h>

#ifdef HAVE_BREAKPAD
#include <client/windows/sender/crash_report_sender.cc>
#endif


bool SendMinidump(std::wstring file, std::wstring product, std::wstring version, std::wstring date, std::wstring url, std::wstring &err);

int archive_dump(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring target);
int send_dump_ui(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring url);
int send_dump(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring url);
int restart(std::wstring service);

int _tmain(int argc, wchar_t* argv[]) {
	if (argc > 1) {
		if (wcsicmp(argv[1], _T("restart"))==0 && argc > 2) {
			return restart(argv[2]);
		} else if (wcsicmp(argv[1], _T("archive"))==0 && argc > 6) {
			return archive_dump(argv[2], argv[3], argv[4], argv[5], argv[6]);
		} else if (wcsicmp(argv[1], _T("send"))==0 && argc > 6) {
			return send_dump(argv[2], argv[3], argv[4], argv[5], argv[6]);
		} else if (wcsicmp(argv[1], _T("send-gui"))==0 && argc > 6) {
			return send_dump_ui(argv[2], argv[3], argv[4], argv[5], argv[6]);
		}
	}
	std::wcout << _T("Usage: ") << argv[0] << _T("archive|send|send-gui [options]") << std::endl;
	std::wcout << _T("    archive <file> <archive path>") << std::endl;
	std::wcout << _T("    send <file> <product> <version> <date>") << std::endl;
	std::wcout << _T("    send-gui <file> <product> <version> <date>") << std::endl;
	std::wcout << _T("    restart <service>") << std::endl;
	return -1;
}

int restart(std::wstring service) {
	try {
		serviceControll::Stop(service);
	} catch (...) {}
	Sleep(1000);
	try {
		serviceControll::Start(service);
	} catch (...) {
		return -1;
	}
	return 0;
}

bool write_desc(std::wstring file, std::string application, std::string version, std::string date) {
	HANDLE hFile = ::CreateFile(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	DWORD numberOfBytesWritten;
	std::string l1 = "application=" + application + "\n";
	std::string l2 = "build-version=" + version + "\n";
	std::string l3 = "build-date=" + date + "\n";
	if (!::WriteFile(hFile, l1.c_str(), (static_cast<DWORD>(l1.length()))*(sizeof(char)), &numberOfBytesWritten, NULL))
		return false;
	if (!::WriteFile(hFile, l2.c_str(), (static_cast<DWORD>(l2.length()))*(sizeof(char)), &numberOfBytesWritten, NULL))
		return false;
	if (!::WriteFile(hFile, l3.c_str(), (static_cast<DWORD>(l3.length()))*(sizeof(char)), &numberOfBytesWritten, NULL))
		return false;
	::CloseHandle(hFile);
	return true;
} 

int archive_dump(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring target) {
	if (!file_helpers::checks::exists(target)) {
 		if (_wmkdir(target.c_str()) != 0) {
 			std::wcout << _T("Failed to create directory: ") << target << std::endl;
 			return -1;
 		}
 		std::wcout << _T("Created folder: ") << target << std::endl;
	}
	if (!file_helpers::checks::is_directory(target)) {
		std::wcout << _T("Target is not a folder: ") << target << std::endl;
		return -1;
	}

	std::wstring fname = file.substr(file.find_last_of(_T("/\\")));
	if (!CopyFile(file.c_str(), (target + fname).c_str(), TRUE)) {
		std::wcout << _T("Failed to copy file to: ") << target << std::endl;
		return -1;
	}
	if (!write_desc(target + fname + _T(".txt"), strEx::wstring_to_string(application), strEx::wstring_to_string(version), strEx::wstring_to_string(date))) {
		std::wcout << _T("Failed to write description: ") << target << fname << _T(".txt") << std::endl;
		return -1;
	}
	return 0;
}
int send_dump_ui(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring url) {
	std::wstring err;
	if (!SendMinidump(file, application, version, date, url, err)) {
		std::wstring msg = _T("Failed sending report to server: ") + application + _T(", ") + version + _T("\nFile: ") + file + _T("\nUrl: ") + url;
		MessageBox(NULL, msg.c_str(), _T("NSClient++ Crash report"), MB_OK);
		while (MessageBox(NULL, (std::wstring(_T("Failed to send crash report to report server: ")) + err).c_str(), _T("NSClient++ Crash report"), MB_RETRYCANCEL|MB_ICONERROR) == IDRETRY) {
			if (SendMinidump(file, application, version, date, url, err))
				break;
		}
	}
	return 0;
}

int send_dump(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring url) {
	std::wstring err;
	if (!SendMinidump(file, application, version, date, url, err)) {
		std::wcout << _T("Failed sending report to server: ") << err << std::endl;
		return -1;
	}
	return 0;
}



bool SendMinidump(std::wstring file, std::wstring product, std::wstring version, std::wstring date, std::wstring url, std::wstring &err) {
#ifdef HAVE_BREAKPAD
	google_breakpad::CrashReportSender sender(_T(""));
	//std::wstring url = _T("http://crash.nsclient.org/submit");
	std::map<std::wstring,std::wstring> params;
	std::wstring ret;
	params[_T("ProductName")] = product;
	params[_T("Version")] = version;
	params[_T("Date")] = date;


	google_breakpad::ReportResult result = sender.SendCrashReport(url, params, file, &ret);
	err = ret;
	return result == google_breakpad::RESULT_SUCCEEDED;
#else
	std::wcerr << _T("Not compiled with protocol buffer support...\n");
	return false;
#endif
}
