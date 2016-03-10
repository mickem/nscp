#include "check_nrpe.hpp"
#include <boost/filesystem.hpp>
#include <config.h>
#include <common.hpp>

#include <types.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>

std::string gLog = "";

int main(int argc, char* argv[]) {
	Plugin::QueryResponseMessage response_message;
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++) {
		args.push_back(argv[i]);
	}
	Plugin::QueryRequestMessage request_message;
	Plugin::QueryRequestMessage::Request *request = request_message.add_payload();
	request->set_command("check_nrpe");
	for (int i = 1; i < argc; i++) {
		request->add_arguments(argv[i]);
	}
	int response_return_code;
	response_return_code = 0;
	check_nrpe client;
	client.query(request_message, response_message);
	BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &response, response_message.payload()) {
		BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response_Line &line, response.lines()) {
			std::cout << line.message();
			if(response.result() > response_return_code){
				response_return_code = response.result();
			}
			std::string tmp = nscapi::protobuf::functions::build_performance_data(line);
			if (!tmp.empty())
				std::cout << '|' << tmp;
		}
	}
	return response_return_code;
}

#ifdef WIN32
boost::filesystem::path get_selfpath() {
	wchar_t buff[4096];
	if (GetModuleFileName(NULL, buff, sizeof(buff) - 1)) {
		boost::filesystem::path p = std::wstring(buff);
		return p.parent_path();
	}
	return boost::filesystem::initial_path();
}
#else
boost::filesystem::path get_selfpath() {
	char buff[1024];
	ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
	if (len != -1) {
		buff[len] = '\0';
		boost::filesystem::path p = std::string(buff);
		return p.parent_path();
	}
	return boost::filesystem::initial_path();
}
#endif
boost::filesystem::path getBasePath(void) {
	return get_selfpath();
}

#ifdef WIN32
typedef DWORD(WINAPI *PFGetTempPath)(__in DWORD nBufferLength, __out  LPTSTR lpBuffer);
#endif
boost::filesystem::path getTempPath() {
	std::string tempPath;
#ifdef WIN32
	unsigned int buf_len = 4096;
	HMODULE hKernel = ::LoadLibrary(L"kernel32");
	if (hKernel) {
		// Find PSAPI functions
		PFGetTempPath FGetTempPath = (PFGetTempPath)::GetProcAddress(hKernel, "GetTempPathW");
		if (FGetTempPath) {
			wchar_t* buffer = new wchar_t[buf_len + 1];
			if (FGetTempPath(buf_len, buffer)) {
				std::wstring s = buffer;
				tempPath = utf8::cvt<std::string>(s);
			}
			delete[] buffer;
		}
	}
#else
	tempPath = "/tmp";
#endif
	return tempPath;
}
#ifdef WIN32
#ifndef CSIDL_COMMON_APPDATA
#define CSIDL_COMMON_APPDATA 0x0023
#endif
typedef BOOL(WINAPI *fnSHGetSpecialFolderPath)(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate);

__inline BOOL WINAPI _SHGetSpecialFolderPath(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate) {
	static fnSHGetSpecialFolderPath __SHGetSpecialFolderPath = NULL;
	if (!__SHGetSpecialFolderPath) {
		HMODULE hDLL = LoadLibrary(L"shfolder.dll");
		if (hDLL != NULL)
			__SHGetSpecialFolderPath = (fnSHGetSpecialFolderPath)GetProcAddress(hDLL, "SHGetSpecialFolderPathW");
	}
	if (__SHGetSpecialFolderPath)
		return __SHGetSpecialFolderPath(hwndOwner, lpszPath, nFolder, fCreate);
	return FALSE;
}
#endif

struct stdout_client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string, int, std::string msg) const {
		if (gLog == "debug")
			std::cout << msg << std::endl;
	}
	void log_error(std::string, int, std::string msg) const {
		if (gLog == "debug" || gLog == "error")
			std::cout << msg << std::endl;
	}

	std::string getFolder(std::string key) {
		std::string default_value = getBasePath().string();
		if (key == "certificate-path") {
			default_value = CERT_FOLDER;
		} else if (key == "module-path") {
			default_value = MODULE_FOLDER;
		} else if (key == "web-path") {
			default_value = WEB_FOLDER;
		} else if (key == "scripts") {
			default_value = SCRIPTS_FOLDER;
		} else if (key == CACHE_FOLDER_KEY) {
			default_value = DEFAULT_CACHE_PATH;
		} else if (key == CRASH_ARCHIVE_FOLDER_KEY) {
			default_value = CRASH_ARCHIVE_FOLDER;
		} else if (key == "base-path") {
			default_value = getBasePath().string();
		} else if (key == "temp") {
			default_value = getTempPath().string();
		} else if (key == "shared-path" || key == "base-path" || key == "exe-path") {
			default_value = getBasePath().string();
		}
#ifdef WIN32
		else if (key == "common-appdata") {
			wchar_t buf[MAX_PATH + 1];
			if (_SHGetSpecialFolderPath(NULL, buf, CSIDL_COMMON_APPDATA, FALSE))
				default_value = utf8::cvt<std::string>(buf);
			else
				default_value = getBasePath().string();
		}
#else
		else if (key == "etc") {
			default_value = "/etc";
		}
#endif
		return default_value;
	}

	std::string expand_path(std::string file) {
		std::string::size_type pos = file.find('$');
		while (pos != std::string::npos) {
			std::string::size_type pstart = file.find('{', pos);
			std::string::size_type pend = file.find('}', pstart);
			std::string key = file.substr(pstart + 1, pend - 2);

			std::string tmp = file;
			strEx::replace(file, "${" + key + "}", getFolder(key));
			if (file == tmp)
				pos = file.find_first_of('$', pos + 1);
			else
				pos = file.find_first_of('$');
		}
		return file;
	}
};

bool test(client::destination_container &source, client::destination_container &destination) {
	if (source.has_data("log"))
		gLog = source.get_string_data("log");
	return true;
}

boost::program_options::options_description add_client_options(client::destination_container &source, client::destination_container &destination) {
	po::options_description desc("Client options");
	desc.add_options()
		("log", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "log", _1)),
			"Set log level")
		;
	return desc;
}

typedef nrpe_client::nrpe_client_handler<stdout_client_handler> nrpe_client_handler;
check_nrpe::check_nrpe() : client_("nrpe", boost::make_shared<nrpe_client_handler>(), boost::make_shared<nrpe_handler::options_reader_impl>()) {
	client_.client_desc = &add_client_options;
	client_.client_pre = &test;
}

void check_nrpe::query(const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage &response) {
	client_.do_query(request, response);
}
