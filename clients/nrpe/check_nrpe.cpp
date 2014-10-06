#include "check_nrpe.hpp"
#include <boost/filesystem.hpp>
#include <config.h>
#include <common.hpp>

#include <types.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>


int main(int argc, char* argv[]) { 
	Plugin::QueryResponseMessage::Response response;
	std::vector<std::string> args;
	for (int i=1;i<argc; i++) {
		args.push_back(argv[i]);
	}

	check_nrpe client;
	client.query(args, response);
	std::cout << response.message();
	std::string tmp = nscapi::protobuf::functions::build_performance_data(response);
	if (!tmp.empty())
		std::cout << '|' << tmp;
}


check_nrpe::check_nrpe() {
	targets.ensure_default("/foo/bar");
}


#ifdef WIN32
boost::filesystem::path get_selfpath() {
	wchar_t buff[4096];
	if (GetModuleFileName(NULL, buff, sizeof(buff)-1)) {
		boost::filesystem::path p = std::wstring(buff);
		return p.parent_path();
	}
	return boost::filesystem::initial_path();
}
#else
boost::filesystem::path get_selfpath() {
	char buff[1024];
	ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
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
typedef DWORD (WINAPI *PFGetTempPath)(__in DWORD nBufferLength, __out  LPTSTR lpBuffer);
#endif
boost::filesystem::path getTempPath() {
	std::string tempPath ;
#ifdef WIN32
	unsigned int buf_len = 4096;
	HMODULE hKernel = ::LoadLibrary(L"kernel32");
	if (hKernel)  
	{
		// Find PSAPI functions
		PFGetTempPath FGetTempPath = (PFGetTempPath)::GetProcAddress(hKernel, "GetTempPathW");
		if (FGetTempPath) {
			wchar_t* buffer = new wchar_t[buf_len+1];
			if (FGetTempPath(buf_len, buffer)) {
				std::wstring s = buffer;
				tempPath = utf8::cvt<std::string>(s);
			}
			delete [] buffer;
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
typedef BOOL (WINAPI *fnSHGetSpecialFolderPath)(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate);

__inline BOOL WINAPI _SHGetSpecialFolderPath(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate) {
	static fnSHGetSpecialFolderPath __SHGetSpecialFolderPath = NULL;
	if (!__SHGetSpecialFolderPath) {
		HMODULE hDLL = LoadLibrary(L"shfolder.dll");
		if (hDLL != NULL)
			__SHGetSpecialFolderPath = (fnSHGetSpecialFolderPath)GetProcAddress(hDLL,"SHGetSpecialFolderPathW");
	}
	if(__SHGetSpecialFolderPath)
		return __SHGetSpecialFolderPath(hwndOwner, lpszPath, nFolder, fCreate);
	return FALSE;
}
#endif


struct client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string file, int line, std::string msg) const {
		std::cout << msg;
	}
	void log_error(std::string file, int line, std::string msg) const {
		std::cout << msg;
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
			wchar_t buf[MAX_PATH+1];
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
			std::string key = file.substr(pstart+1, pend-2);

			std::string tmp = file;
			strEx::replace(file, "${" + key + "}", getFolder(key));
			if (file == tmp)
				pos = file.find_first_of('$', pos+1);
			else
				pos = file.find_first_of('$');
		}
		return file;
	}
};

void check_nrpe::query(const std::vector<std::string> &args, Plugin::QueryResponseMessage::Response &response) {
	client::configuration config(nrpe_client::command_prefix);
	config.target_lookup = boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)); 
	config.handler = boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler())));
	const ::Plugin::Common::Header header;
	nrpe_client::setup(config, header);
	commands.parse_query(config, args, response);
}
