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
			size += (wfd.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)wfd.nFileSizeLow;
		}
		return true;
	}
	inline unsigned long long getSize() {
		return size;
	}
private:  
	unsigned long long size;
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


NSCAPI::nagiosReturn CheckDisk::CheckDriveSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}

	DriveConatiner tmpObject;
	bool bShowAll = false;
	bool bFilter = false;
	bool bFilterRemote = false;
	bool bFilterRemovable = false;
	bool bFilterFixed = false;
	bool bFilterCDROM = false;
	bool bCheckAll = false;
	bool bNSClient = false;
	std::list<DriveConatiner> drives;

	MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR_AND("Drive", tmpObject.data, drives.push_back(tmpObject))
		MAP_OPTIONS_STR("MaxWarn", tmpObject.warn.max)
		MAP_OPTIONS_STR("MinWarn", tmpObject.warn.min)
		MAP_OPTIONS_STR("MaxCrit", tmpObject.crit.max)
		MAP_OPTIONS_STR("MinCrit", tmpObject.crit.min)
		else if (p__.first == "FilterType") {
			bFilter = true;
			if (p__.second == "FIXED") {
				bFilterFixed = true;
			} else if (p__.second == "CDROM") {
				bFilterCDROM = true;
			} else if (p__.second == "REMOVABLE") {
				bFilterRemovable = true;
			} else if (p__.second == "REMOTE") {
				bFilterRemote= true;
			}
		}
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_TRUE(CHECK_ALL, bCheckAll)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Drive") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				drives.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, message, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_MISSING(message, "Unknown argument: ")
	MAP_OPTIONS_END()

	if (bCheckAll) {
		DWORD dwDrives = GetLogicalDrives();
		int idx = 0;
		while (dwDrives != 0) {
			if (dwDrives & 0x1) {
				std::string drv;
				drv += static_cast<char>('A' + idx); drv += ":\\";
				UINT drvType = GetDriveType(drv.c_str());
				if ( ((!bFilter)&&(drvType == DRIVE_FIXED))  ||
					((bFilter)&&(bFilterFixed)&&(drvType==DRIVE_FIXED)) ||
					((bFilter)&&(bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
					((bFilter)&&(bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
					((bFilter)&&(bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) )
					drives.push_back(DriveConatiner(drv, tmpObject.warn, tmpObject.crit));
			}
			idx++;
			dwDrives >>= 1;
		}
	}


	for (std::list<DriveConatiner>::const_iterator pit = drives.begin();pit!=drives.end();++pit) {
		DriveConatiner drive = (*pit);
		if (drive.data.length() == 1)
			drive.data += ":";
		UINT drvType = GetDriveType(drive.data.c_str());

		if ((!bFilter)&&!((drvType == DRIVE_FIXED)||(drvType == DRIVE_NO_ROOT_DIR))) {
			message = "UNKNOWN: Drive is not a fixed drive: " + drive.getAlias() + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		} else if ( (bFilter)&&( (!bFilterFixed)&&((drvType==DRIVE_FIXED)||(drvType==DRIVE_NO_ROOT_DIR))) ||
			((!bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
			((!bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
			((!bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) ) {
			message = "UNKNOWN: Drive does not match the current filter: " + drive.getAlias() + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		}

		ULARGE_INTEGER freeBytesAvailableToCaller;
		ULARGE_INTEGER totalNumberOfBytes;
		ULARGE_INTEGER totalNumberOfFreeBytes;
		if (!GetDiskFreeSpaceEx(drive.data.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
			message = "UNKNOWN: Could not get free space for: " + drive.getAlias() + + " \"" + drive.data + "\" reason: " + strEx::itos(GetLastError());
			return NSCAPI::returnUNKNOWN;
		}

		if (bNSClient) {
			if (!message.empty())
				message += "&";
			message += strEx::itos(totalNumberOfFreeBytes.QuadPart);
			message += "&";
			message += strEx::itos(totalNumberOfBytes.QuadPart);
		} else {
			std::string tstr;
			checkHolders::disk_size_type size = totalNumberOfBytes.QuadPart-totalNumberOfFreeBytes.QuadPart;
			drive.setDefault(tmpObject);
			drive.warn.max.setMax(totalNumberOfBytes.QuadPart);
			drive.warn.min.setMax(totalNumberOfBytes.QuadPart);
			drive.crit.max.setMax(totalNumberOfBytes.QuadPart);
			drive.crit.min.setMax(totalNumberOfBytes.QuadPart);
			drive.runCheck(size, returnCode, message, perf, bShowAll);
		}
	}
	if (message.empty())
		message = "OK: All drives within bounds.";
	else if (!bNSClient)
		message = NSCHelper::translateReturn(returnCode) + ": " + message;
	return returnCode;
}



NSCAPI::nagiosReturn CheckDisk::CheckFileSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnUNKNOWN;
	}
	PathConatiner tmpObject;
	bool bShowAll = false;
	std::list<PathConatiner> paths;

	MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR_AND("File", tmpObject.data, paths.push_back(tmpObject))
		MAP_OPTIONS_STR("MaxWarn", tmpObject.warn.max)
		MAP_OPTIONS_STR("MinWarn", tmpObject.warn.min)
		MAP_OPTIONS_STR("MaxCrit", tmpObject.crit.max)
		MAP_OPTIONS_STR("MinCrit", tmpObject.crit.min)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
		else if (p2.first == "File") {
			tmpObject.data = p__.second;
			tmpObject.alias = p2.second;
			paths.push_back(tmpObject);
		}
		MAP_OPTIONS_MISSING_EX(p2, message, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_MISSING(message, "Unknown argument: ")
	MAP_OPTIONS_END()

	for (std::list<PathConatiner>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		PathConatiner path = (*pit);
		std::string tstr;
		std::string sName = path.getAlias();
		GetSize sizeFinder;
		RecursiveScanDirectory(path.data, sizeFinder);
		path.setDefault(tmpObject);

		checkHolders::disk_size_type size = sizeFinder.getSize();
		path.runCheck(size, returnCode, message, perf, bShowAll);
	}
	if (message.empty())
		message = "OK all file sizes are within bounds.";
	else
		message = NSCHelper::translateReturn(returnCode) + ": " + message;
	return returnCode;
}

NSCAPI::nagiosReturn CheckDisk::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	if (command == "CheckFileSize") {
		return CheckFileSize(argLen, char_args, msg, perf);
	} else if (command == "CheckDriveSize") {
		return CheckDriveSize(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gCheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckDisk);
