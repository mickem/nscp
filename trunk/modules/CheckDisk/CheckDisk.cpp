// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "CheckDisk.h"
#include <strEx.h>
#include <time.h>

CheckDisk gCheckDisk;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckDisk::CheckDisk() {
}
CheckDisk::~CheckDisk() {
}


bool CheckDisk::loadModule() {
	return true;
}
bool CheckDisk::unloadModule() {
	return true;
}

std::string CheckDisk::getModuleName() {
	return "CheckDisk Various Disk related checks.";
}
NSCModuleWrapper::module_version CheckDisk::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}

bool CheckDisk::hasCommandHandler() {
	return true;
}
bool CheckDisk::hasMessageHandler() {
	return false;
}

typedef std::unary_function<const WIN32_FIND_DATA&, bool> baseClass;
struct GetSize : public baseClass
{
	GetSize() : size(0) { }
	result_type operator()(argument_type wfd) {
		if (!(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
			size += (wfd.nFileSizeHigh * ((long long)MAXDWORD+1)) + (long long)wfd.nFileSizeLow;
			NSC_LOG_ERROR_STD(wfd.cFileName + ": " + strEx::itos((wfd.nFileSizeHigh * ((long long)MAXDWORD+1)) + (long long)wfd.nFileSizeLow) + " --- " + strEx::itos(size));
		}
		return true;
	}
	inline long long getSize() {
		return size;
	}
private:  
	long long size;
};

void RecursiveScanDirectory(std::string dir, GetSize & f) {
	std::string baseDir;
	std::string::size_type pos = dir.find_last_of('\\');
	if (pos == std::string::npos)
		return;
	baseDir = dir.substr(0, pos);

	WIN32_FIND_DATA wfd;
	HANDLE hFind = FindFirstFile(dir.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!f(wfd))
				break;
			if ((wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
				if ( (strcmp(wfd.cFileName, ".") != 0) && (strcmp(wfd.cFileName, "..") != 0) )
					RecursiveScanDirectory(baseDir + "\\" + wfd.cFileName + "\\*.*", f);
			}
		} while (FindNextFile(hFind, &wfd));
	}
	FindClose(hFind);
}


// CheckDirectorySize&<Directory>&<Directory>...
// request: CheckDirectorySize&<warning>&<max>&<Directory>&<Directory>...
// Return: <return state>&<return string>...
// <return state>	0 - No errors
//					1 - Unknown
//					2 - Errors
// <return string>	<directory> <size mb> ...
// test: CheckDirectorySize&1024M&4096M&c:\WINDOWS\*.*
//       CheckDirectorySize
#define BUFFER_SIZE 1024*64

std::string CheckDisk::handleCommand(const std::string command, const unsigned int argLen, char **char_args) {
	std::string ret;
	if (command == "CheckDirectorySize") {
		std::list<std::string> args = NSCHelper::makelist(argLen, char_args);
		if (args.size() < 3)
			return "Missing argument(s)." + strEx::itos((int)args.size());
		int iWarn = strEx::stoi64_as_BKMG(args.front()); args.pop_front();
		int iMax = strEx::stoi64_as_BKMG(args.front()); args.pop_front();

		std::list<std::string>::const_iterator cit;
		for (cit=args.begin();cit!=args.end();++cit) {
			GetSize size;
			RecursiveScanDirectory(*cit, size);
			if (!ret.empty())
				ret += ", ";
			ret += (*cit) + ": " + strEx::itos_as_BKMG(size.getSize());
		}
	}
	return ret;
}


NSC_WRAPPERS_MAIN_DEF(gCheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckDisk);
