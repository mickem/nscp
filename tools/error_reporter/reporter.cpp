/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include <str/xtos.hpp>

#ifdef USE_BREAKPAD
#include <client/windows/sender/crash_report_sender.cc>
#endif

bool SendMinidump(std::string file, std::string product, std::string version, std::string date, std::string url, std::string &err);

int archive_dump(std::string file, std::string application, std::string version, std::string date, std::string target);
int send_dump_ui(std::string file, std::string application, std::string version, std::string date, std::string url);
int send_dump(std::string file, std::string application, std::string version, std::string date, std::string url);
int send_dump(std::string file, std::string url);
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
		} else if (command == "send" && argc > 3) {
			return send_dump(argv[2], argv[3]);
		} else if (command == "send" && argc > 2) {
			return send_dump(argv[2], "");
		} else if (command == "send-gui" && argc > 6) {
			return send_dump_ui(argv[2], argv[3], argv[4], argv[5], argv[6]);
		}
	}
	std::cout << "Usage: " << argv[0] << "archive|send|send-gui [options]" << std::endl;
	std::cout << "    archive <file> <archive path>" << std::endl;
	std::cout << "    send <file> <product> <version> <date> <url>" << std::endl;
	std::cout << "    send <file> [url] (This requires the .txt to accompany the file)" << std::endl;
	std::cout << "         <file> is the crashdump file usually called GUID.dmp but we also need the GUID.dmp.txt" << std::endl;
	std::cout << "         [url]  is the crash server: http://crash.nsclient.org/crash_upload" << std::endl;
	std::cout << "    send-gui <file> <product> <version> <date> <url>" << std::endl;
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
		while (MessageBox(NULL, (std::wstring(L"Failed to send crash report to report server: ") + utf8::cvt<std::wstring>(err)).c_str(), L"NSClient++ Crash report", MB_RETRYCANCEL | MB_ICONERROR) == IDRETRY) {
			if (SendMinidump(file, application, version, date, url, err))
				break;
		}
#else
		std::cerr << msg << std::endl;
#endif
	}
	return 0;
}

int send_dump(std::string file, std::string url) {
	try {
		std::string desc_file = file + ".txt";
		std::ifstream infile(desc_file.c_str());

		std::string line, app, ver, date;
		while (std::getline(infile, line)) {
			std::string::size_type pos = line.find('=');
			if (pos == std::string::npos) {
				std::cout << "Failed to read: " << line << std::endl;
				return -1;
			}
			if (line.substr(0, pos) == "application") {
				app = line.substr(pos + 1);
			} else if (line.substr(0, pos) == "build-version") {
				ver = line.substr(pos + 1);
			} else if (line.substr(0, pos) == "build-date") {
				date = line.substr(pos + 1);
			}
		}

		std::string err;
		if (url.empty())
			url = "https://crash.nsclient.org/post";

		if (!SendMinidump(file, app, ver, date, url, err)) {
			std::cout << "Failed sending report to server: " << err << std::endl;
			return -1;
		}
	} catch (const std::exception &e) {
		std::cout << "Failed to process: " << e.what() << std::endl;
		return -1;
	} catch (...) {
		std::cout << "Failed to process: " << std::endl;
		return -1;
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
	std::map<std::wstring, std::wstring> params;
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