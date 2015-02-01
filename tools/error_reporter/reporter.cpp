// systray_helper.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <fstream>

#include <boost/filesystem.hpp>

#include "reporter.h"
#ifdef WIN32
#include <ServiceCmd.h>
#include <tchar.h>
#endif
#include <iostream>
#include <file_helpers.hpp>
#include <strEx.h>

#ifdef USE_BREAKPAD
#include <client/windows/sender/crash_report_sender.cc>
#endif


bool SendMinidump(std::string file, std::string product, std::string version, std::string date, std::string url, std::string &err);

int archive_dump(std::string file, std::string application, std::string version, std::string date, std::string target);
int send_dump_ui(std::string file, std::string application, std::string version, std::string date, std::string url);
int send_dump(std::string file, std::string application, std::string version, std::string date, std::string url);
int restart(std::string service);


int nscp_main(int argc, wchar_t* argv[]);

int main(int argc, char* argv[]) { 
	if (argc > 1) {
		std::string command = argv[1];
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
	std::cout << "Usage: " << argv[0] << L"archive|send|send-gui [options]" << std::endl;
	std::cout << "    archive <file> <archive path>" << std::endl;
	std::cout << "    send <file> <product> <version> <date>" << std::endl;
	std::cout << "    send-gui <file> <product> <version> <date>" << std::endl;
	std::cout << "    restart <service>" << std::endl;
	return -1;
}

int restart(std::string service) {
#ifdef WIN32
	try {
		serviceControll::Stop(utf8::cvt<std::wstring>(service));
	} catch (...) {}
	Sleep(1000);
	try {
		serviceControll::Start(utf8::cvt<std::wstring>(service));
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

int archive_dump(std::string file, std::string application, std::string version, std::string date, std::string target) {
	try {
		if (!boost::filesystem::exists(target)) {
			if (!boost::filesystem::create_directories(target)) {
				std::cout << "Failed to create directory: " << target << std::endl;
				return -1;
			}
			std::cout << "Created folder: " << target << std::endl;
		}
		if (!boost::filesystem::is_directory(target)) {
			std::cout << L"Target is not a folder: " << target << std::endl;
			return -1;
		}

		std::string fname = file.substr(file.find_last_of("/\\"));
		boost::filesystem::copy_file(file, target + fname);
		std::string descfile = target + fname + ".txt";
		if (!write_desc(utf8::cvt<std::string>(descfile), utf8::cvt<std::string>(application), utf8::cvt<std::string>(version), utf8::cvt<std::string>(date))) {
			std::cout << L"Failed to write description: " << target << fname << L".txt" << std::endl;
			return -1;
		}
		return 0;
	} catch (const std::exception &e) {
		std::cout << e.what() << std::endl;
		return -1;
	}
}
int send_dump_ui(std::string file, std::string application, std::string version, std::string date, std::string url) {
	std::string err;
	if (!SendMinidump(file, application, version, date, url, err)) {
		std::string msg = "Failed sending report to server: " + application + ", " + version + "\nFile: " + file + "\nUrl: " + url;
#ifdef WIN32
		MessageBox(NULL, utf8::cvt<std::wstring>(msg).c_str(), L"NSClient++ Crash report", MB_OK);
		while (MessageBox(NULL, (std::wstring(L"Failed to send crash report to report server: ") + utf8::cvt<std::wstring>(err)).c_str(), L"NSClient++ Crash report", MB_RETRYCANCEL|MB_ICONERROR) == IDRETRY) {
			if (SendMinidump(file, application, version, date, url, err))
				break;
		}
#else
		std::cerr << msg << std::endl;
#endif
	}
	return 0;
}

int send_dump(std::string file, std::string application, std::string version, std::string date, std::string url) {
	std::string err;
	if (!SendMinidump(file, application, version, date, url, err)) {
		std::cout << "Failed sending report to server: " << err << std::endl;
		return -1;
	}
	return 0;
}



bool SendMinidump(std::string file, std::string product, std::string version, std::string date, std::string url, std::string &err) {
#ifdef USE_BREAKPAD
	google_breakpad::CrashReportSender sender(L"");
	std::map<std::wstring,std::wstring> params;
	std::wstring ret;
	params[L"prod"] = utf8::cvt<std::wstring>(product);
	params[L"ver"] = utf8::cvt<std::wstring>(version);
	params[L"Date"] = utf8::cvt<std::wstring>(date);
	params[L"user-agent"] = L"NSClient++ crash reporter";


	google_breakpad::ReportResult result = sender.SendCrashReport(utf8::cvt<std::wstring>(url), params, utf8::cvt<std::wstring>(file), &ret);
	err = utf8::cvt<std::string>(ret);
	return result == google_breakpad::RESULT_SUCCEEDED;
#else
	std::cerr << "Not compiled with protocol buffer support...\n";
	return false;
#endif
}
