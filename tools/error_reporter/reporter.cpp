// systray_helper.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

#include <boost/filesystem.hpp>

#include "reporter.h"
#ifdef WIN32
#include <ServiceCmd.h>
#include <tchar.h>
#endif
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


int nscp_main(int argc, wchar_t* argv[]);

#ifdef WIN32
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) { return nscp_main(argc, argv); }
#else
int main(int argc, char* argv[]) { 
	wchar_t **wargv = new wchar_t*[argc];
	for (int i=0;i<argc;i++) {
		std::wstring s = utf8::cvt<std::wstring>(argv[i]);
		wargv[i] = new wchar_t[s.length()+10];
		wcscpy(wargv[i], s.c_str());
	}
	int ret = nscp_main(argc, wargv); 
	for (int i=0;i<argc;i++) {
		delete [] wargv[i];
	}
	delete [] wargv;
	return ret;
}
#endif

int nscp_main(int argc, wchar_t* argv[]) {
	if (argc > 1) {
		std::string command = utf8::cvt<std::string>(argv[1]);
		if (command == "restart" && argc > 2) {
			return restart(argv[2]);
		} else if (command == "archive" && argc > 6) {
			return archive_dump(argv[2], argv[3], argv[4], argv[5], argv[6]);
		} else if (command == "send" && argc > 6) {
			return send_dump(argv[2], argv[3], argv[4], argv[5], argv[6]);
		} else if (command == "send-gui" && argc > 6) {
			return send_dump_ui(argv[2], argv[3], argv[4], argv[5], argv[6]);
		}
	}
	std::wcout << L"Usage: " << argv[0] << L"archive|send|send-gui [options]" << std::endl;
	std::wcout << L"    archive <file> <archive path>" << std::endl;
	std::wcout << L"    send <file> <product> <version> <date>" << std::endl;
	std::wcout << L"    send-gui <file> <product> <version> <date>" << std::endl;
	std::wcout << L"    restart <service>" << std::endl;
	return -1;
}

int restart(std::wstring service) {
#ifdef WIN32
	try {
		serviceControll::Stop(service);
	} catch (...) {}
	Sleep(1000);
	try {
		serviceControll::Start(service);
	} catch (...) {
		return -1;
	}
#else
	std::cerr << "service restart not supported on linux" << std::endl;
#endif
	return 0;
}

bool write_desc(std::string file, std::string application, std::string version, std::string date) {

	try {
		std::ofstream descfile;
		descfile.open(file.c_str());
		descfile << "application=" + application + "\n";
		descfile << "build-version=" + version + "\n";
		descfile << "build-date=" + date + "\n";
		descfile.close();
	} catch (const std::exception &e) {
		std::cout << e.what() << std::endl;
		return false;
	}
	return true;
} 

int archive_dump(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring target) {
	try {
		if (!file_helpers::checks::exists(target)) {
			if (!boost::filesystem::create_directories(target)) {
				std::wcout << L"Failed to create directory: " << target << std::endl;
				return -1;
			}
			std::wcout << L"Created folder: " << target << std::endl;
		}
		if (!file_helpers::checks::is_directory(target)) {
			std::wcout << L"Target is not a folder: " << target << std::endl;
			return -1;
		}

		std::wstring fname = file.substr(file.find_last_of(L"/\\"));
		boost::filesystem::copy_file(file, target + fname);
		std::wstring descfile = target + fname + L".txt";
		if (!write_desc(utf8::cvt<std::string>(descfile), utf8::cvt<std::string>(application), utf8::cvt<std::string>(version), utf8::cvt<std::string>(date))) {
			std::wcout << L"Failed to write description: " << target << fname << L".txt" << std::endl;
			return -1;
		}
		return 0;
	} catch (const std::exception &e) {
		std::cout << e.what() << std::endl;
		return -1;
	}
}
int send_dump_ui(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring url) {
	std::wstring err;
	if (!SendMinidump(file, application, version, date, url, err)) {
		std::wstring msg = L"Failed sending report to server: " + application + L", " + version + L"\nFile: " + file + L"\nUrl: " + url;
#ifdef WIN32
		MessageBox(NULL, msg.c_str(), L"NSClient++ Crash report", MB_OK);
		while (MessageBox(NULL, (std::wstring(L"Failed to send crash report to report server: ") + err).c_str(), L"NSClient++ Crash report", MB_RETRYCANCEL|MB_ICONERROR) == IDRETRY) {
			if (SendMinidump(file, application, version, date, url, err))
				break;
		}
#else
		std::wcerr << msg << std::endl;
#endif
	}
	return 0;
}

int send_dump(std::wstring file, std::wstring application, std::wstring version, std::wstring date, std::wstring url) {
	std::wstring err;
	if (!SendMinidump(file, application, version, date, url, err)) {
		std::wcout << L"Failed sending report to server: " << err << std::endl;
		return -1;
	}
	return 0;
}



bool SendMinidump(std::wstring file, std::wstring product, std::wstring version, std::wstring date, std::wstring url, std::wstring &err) {
#ifdef HAVE_BREAKPAD
	google_breakpad::CrashReportSender sender(L"");
	//std::wstring url = L"http://crash.nsclient.org/submit";
	std::map<std::wstring,std::wstring> params;
	std::wstring ret;
	params[L"ProductName"] = product;
	params[L"Version"] = version;
	params[L"Date"] = date;


	google_breakpad::ReportResult result = sender.SendCrashReport(url, params, file, &ret);
	err = ret;
	return result == google_breakpad::RESULT_SUCCEEDED;
#else
	std::wcerr << L"Not compiled with protocol buffer support...\n";
	return false;
#endif
}
