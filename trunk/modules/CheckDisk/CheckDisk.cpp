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

NSCAPI::nagiosReturn CheckDisk::CheckFileSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	// CheckFileSize
	// request: CheckFileSize&<option>&<option>...
	// <option>			MaxWarn=<size gmkb>
	//					MaxCrit=<size gmkb>
	//					MinWarn=<size gmkb>
	//					MinCrit=<size gmkb>
	//					ShowAll
	//					File=<path>
	//					File:<shortname>=<path>
	//
	// Return: <return state>&<return string>...
	// <return state>	0 - No errors
	//					1 - Unknown
	//					2 - Errors
	// <size gmkb> is a size with a possible modifier letter (such as G for gigabyte, M for Megabyte, K for kilobyte etc)
	// Examples:
	// <return string>	<directory> <size gmkb> ... |<shortname>=<size>:<warn>:<crit>
	// test: CheckFileSize&ShowAll&MaxWarn=1024M&MaxCrit=4096M&File:WIN=c:\WINDOWS\*.*
	//       CheckFileSize
	//
	// check_nscp -H <ip> -p <port> -s <passwd> -c <commandstring>
	//
	// ./check_nscp -H 192.168.0.167 -p 1234 -s pwd -c 'CheckFileSize&ShowAll&MaxWarn=1024M&MaxCrit=4096M&File:WIN=c:\WINDOWS\*.*'
	// WIN: 1G (2110962363B)|WIN:2110962363:1073741824:4294967296
	NSC_DEBUG_MSG("CheckFileSize");
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = NSCHelper::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}
	long long maxWarn = 0;
	long long maxCrit = 0;
	long long minWarn = 0;
	long long minCrit = 0;
	bool bShowAll = false;
	std::list<std::pair<std::string,std::string> > paths;

	std::list<std::string>::const_iterator cit;
	for (cit=args.begin();cit!=args.end();++cit) {
		std::string arg = *cit;
		std::pair<std::string,std::string> p = strEx::split(arg,"=");
		if (p.first == "File") {
			paths.push_back(std::pair<std::string,std::string>("",p.second));
		} else if (p.first == "MaxWarn") {
			maxWarn = strEx::stoi64_as_BKMG(p.second);
		} else if (p.first == "MinWarn") {
			minWarn = strEx::stoi64_as_BKMG(p.second);
		} else if (p.first == "MaxCrit") {
			maxCrit = strEx::stoi64_as_BKMG(p.second);
		} else if (p.first == "MinCrit") {
			minCrit = strEx::stoi64_as_BKMG(p.second);
		} else if (p.first == "ShowAll") {
			bShowAll = true;
		} else if (p.first.find(":") != std::string::npos) {
			std::pair<std::string,std::string> p2 = strEx::split(p.first,":");
			if (p2.first == "File") {
				paths.push_back(std::pair<std::string,std::string>(p2.second,p.second));
			} else {
				message = "Unknown command: " + p.first;
				return NSCAPI::returnCRIT;
			}
		} else {
			message = "Unknown command: " + p.first;
			return NSCAPI::returnCRIT;
		}
	}
	NSC_DEBUG_MSG_STD("Bounds: critical " + strEx::itos(minCrit) + " > siez > " + strEx::itos(maxCrit));
	NSC_DEBUG_MSG_STD("Bounds: warning " + strEx::itos(minWarn) + " > size > " + strEx::itos(maxWarn));
	NSC_DEBUG_MSG_STD("Showall: " + ((bShowAll)?"yeap":"noop"));

	std::list<std::pair<std::string,std::string> >::const_iterator pit;
	for (pit = paths.begin(); pit != paths.end(); ++pit) {
		std::string tstr;
		GetSize sizeFinder;
		std::string sName = (*pit).first;
		if (sName.empty())
			sName = (*pit).second;
		RecursiveScanDirectory((*pit).second, sizeFinder);

		if ((maxCrit!=0) && (sizeFinder.getSize() > maxCrit)) {
			tstr = sName + ": " + strEx::itos_as_BKMG(sizeFinder.getSize());
			returnCode = NSCAPI::returnCRIT;
		} else if (sizeFinder.getSize() < minCrit) {
			tstr = sName + ": " + strEx::itos_as_BKMG(sizeFinder.getSize());
			NSCHelper::escalteReturnCodeToCRIT(returnCode);
		} else if ((maxWarn!=0)&&(sizeFinder.getSize() > maxWarn)) {
			tstr = sName +  ": " + strEx::itos_as_BKMG(sizeFinder.getSize());
			NSCHelper::escalteReturnCodeToWARN(returnCode);
		} else if (sizeFinder.getSize() < minWarn) {
			tstr = sName +  ": " + strEx::itos_as_BKMG(sizeFinder.getSize());
			NSCHelper::escalteReturnCodeToWARN(returnCode);
		} else if (bShowAll) {
			tstr = sName +  ": " + strEx::itos_as_BKMG(sizeFinder.getSize());
		}
		if (!(*pit).first.empty())
			perf += (*pit).first + "=" + strEx::itos(sizeFinder.getSize()) + ";" + strEx::itos(maxWarn) + ";" + strEx::itos(maxCrit) + " ";
		if (!message.empty() && !tstr.empty())
			message += ", ";
		if (!tstr.empty())
			message += tstr;
	}
	if (message.empty())
		message = "OK all file sizes are within bounds.";
	return returnCode;
}


#define BUFFER_SIZE 1024*64

NSCAPI::nagiosReturn CheckDisk::handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	if (command == "CheckFileSize") {
		return CheckFileSize(argLen, char_args, msg, perf);
//	} else if (command == "CheckFileDate") {
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gCheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckDisk);
