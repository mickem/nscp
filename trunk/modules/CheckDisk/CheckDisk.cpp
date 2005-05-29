// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "CheckDisk.h"
#include <strEx.h>
#include <time.h>
#include <utils.h>

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

struct DriveConatiner {
	checkHolders::SizeMaxMinPercentage<> warn_;
	checkHolders::SizeMaxMinPercentage<> crit_;
	std::string drive_;
	DriveConatiner(std::string drive, checkHolders::SizeMaxMinPercentage<> warn, checkHolders::SizeMaxMinPercentage<> crit) 
		: drive_(drive), warn_(warn), crit_(crit) 
	{}
};

NSCAPI::nagiosReturn CheckDisk::CheckDriveSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}

	checkHolders::SizeMaxMinPercentage<> warn;
	checkHolders::SizeMaxMinPercentage<> crit;
	bool bShowAll = false;
	bool bNSClient = false;
	bool bCheckAll = false;

	bool bFilter = false;
	bool bFilterRemote = false;
	bool bFilterRemovable = false;
	bool bFilterFixed = false;
	bool bFilterCDROM = false;
	std::list<DriveConatiner> drives;

	std::list<std::string>::const_iterator cit;
	for (cit=args.begin();cit!=args.end();++cit) {
		std::string arg = *cit;
		std::pair<std::string,std::string> p = strEx::split(arg,"=");
		if (p.first == "Drive") {
			drives.push_back(DriveConatiner(p.second, warn, crit));
		} else if (p.first == "MaxWarn") {
			warn.max.set(p.second);
		} else if (p.first == "MinWarn") {
			warn.min.set(p.second);
		} else if (p.first == "MaxCrit") {
			crit.max.set(p.second);
		} else if (p.first == "MinCrit") {
			crit.min.set(p.second);
		} else if (p.first == SHOW_ALL) {
			bShowAll = true;
		} else if (p.first == "nsclient") {
			bNSClient = true;
		} else if (p.first == "FilterType") {
			bFilter = true;
			if (p.second == "FIXED") {
				bFilterFixed = true;
			} else if (p.second == "CDROM") {
				bFilterCDROM= true;
			} else if (p.second == "REMOVABLE") {
				bFilterRemovable = true;
			} else if (p.second == "REMOTE") {
				bFilterRemote= true;
			}
		} else if (p.first == "CheckAll") {
			bCheckAll = true;
		} else {
			drives.push_back(DriveConatiner(p.first, warn, crit));
		}
	}

	if (bCheckAll) {
		DWORD dwDrives = GetLogicalDrives();
		int idx = 0;
		while (dwDrives != 0) {
			if (dwDrives & 0x1) {
				std::string drv;
				drv += static_cast<char>('A' + idx); drv += ":\\";
				UINT drvType = GetDriveType(drv.c_str());
				if ((!bFilter)&&(drvType == DRIVE_FIXED)) {
					drives.push_back(DriveConatiner(drv, warn, crit));
				} else if ((bFilter)&&(bFilterFixed)&&(drvType==DRIVE_FIXED)) {
					drives.push_back(DriveConatiner(drv, warn, crit));
				} else if ((bFilter)&&(bFilterCDROM)&&(drvType==DRIVE_CDROM)) {
					drives.push_back(DriveConatiner(drv, warn, crit));
				} else if ((bFilter)&&(bFilterRemote)&&(drvType==DRIVE_REMOTE)) {
					drives.push_back(DriveConatiner(drv, warn, crit));
				} else if ((bFilter)&&(bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) {
					drives.push_back(DriveConatiner(drv, warn, crit));
				}
			}
			idx++;
			dwDrives >>= 1;
		}
	}

	for (std::list<DriveConatiner>::iterator it = drives.begin();it!=drives.end();it++) {
		DriveConatiner drive = (*it);
		if (drive.drive_.length() == 1)
			drive.drive_ += ":";
		UINT drvType = GetDriveType(drive.drive_.c_str());

		if ((!bFilter)&&(drvType != DRIVE_FIXED)) {
			message = "UNKNOWN: Drive is not a fixed drive: " + drive.drive_ + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		} else if ((bFilter)&&(!bFilterFixed)&&(drvType==DRIVE_FIXED)) {
			message = "UNKNOWN: Drive does not match the current filter: " + drive.drive_ + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		} else if ((bFilter)&&(!bFilterCDROM)&&(drvType==DRIVE_CDROM)) {
			message = "UNKNOWN: Drive does not match the current filter: " + drive.drive_ + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		} else if ((bFilter)&&(!bFilterRemote)&&(drvType==DRIVE_REMOTE)) {
			message = "UNKNOWN: Drive does not match the current filter: " + drive.drive_ + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		} else if ((bFilter)&&(!bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) {
			message = "UNKNOWN: Drive does not match the current filter: " + drive.drive_ + " (it is a: " + strEx::itos(drvType) + ")";
			return NSCAPI::returnUNKNOWN;
		}

		ULARGE_INTEGER freeBytesAvailableToCaller;
		ULARGE_INTEGER totalNumberOfBytes;
		ULARGE_INTEGER totalNumberOfFreeBytes;
		if (!GetDiskFreeSpaceEx(drive.drive_.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
			message = "UNKNOWN: Could not get free space for: " + drive.drive_;
			return NSCAPI::returnUNKNOWN;
		}
		//10597515264&80015491072

		if (bNSClient) {
			if (!message.empty())
				message += "&";
			message += strEx::itos(totalNumberOfFreeBytes.QuadPart);
			message += "&";
			message += strEx::itos(totalNumberOfBytes.QuadPart);
		} else {
			std::string tStr;
			checkHolders::drive_size usedSpace = totalNumberOfBytes.QuadPart-totalNumberOfFreeBytes.QuadPart;
			checkHolders::drive_size totalSpace = totalNumberOfBytes.QuadPart;
			if (drive.crit_.max.hasBounds() && drive.crit_.max.checkMAX(usedSpace, totalSpace)) {
				tStr += drive.crit_.max.prettyPrint(drive.drive_, usedSpace, totalSpace) + " > critical";
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			} else if (drive.crit_.min.hasBounds() && drive.crit_.min.checkMIN(usedSpace, totalSpace)) {
				tStr = drive.crit_.min.prettyPrint(drive.drive_, usedSpace, totalSpace) + " < critical";
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			} else if (drive.warn_.max.hasBounds() && drive.warn_.max.checkMAX(usedSpace, totalSpace)) {
				tStr = drive.warn_.max.prettyPrint(drive.drive_, usedSpace, totalSpace) + " > warning";
				NSCHelper::escalteReturnCodeToWARN(returnCode);
			} else if (drive.warn_.min.hasBounds() && drive.warn_.min.checkMIN(usedSpace, totalSpace)) {
				tStr = drive.warn_.min.prettyPrint(drive.drive_, usedSpace, totalSpace) + " < warning";
				NSCHelper::escalteReturnCodeToWARN(returnCode);
			} else if (bShowAll) {
				tStr = drive.drive_ + ": " + strEx::itos_as_BKMG(usedSpace);
			}
			perf += checkHolders::SizeMaxMinPercentage<>::printPerf(drive.drive_, usedSpace, totalSpace, drive.warn_, drive.crit_);
			if (!message.empty() && !tStr.empty())
				message += ", ";
			if (!tStr.empty())
				message += tStr;
		}
	}
	if (message.empty())
		message = "All drive sizes are within bounds.";
	else if (!bNSClient)
		message = NSCHelper::translateReturn(returnCode) + ": " + message;
	return returnCode;
}

struct PathConatiner {
	checkHolders::SizeMaxMin<> warn_;
	checkHolders::SizeMaxMin<> crit_;
	std::string drive_;
	std::string alias_;
	PathConatiner(std::string drive, checkHolders::SizeMaxMin<> warn, checkHolders::SizeMaxMin<> crit) 
		: drive_(drive), warn_(warn), crit_(crit) 
	{}
	PathConatiner(std::string drive, std::string alias, checkHolders::SizeMaxMin<> warn, checkHolders::SizeMaxMin<> crit) 
		: drive_(drive), alias_(alias), warn_(warn), crit_(crit) 
	{}
	std::string getPath() {
		return drive_;
	}
	std::string getName() {
		if (alias_.empty())
			return drive_;
		return alias_;
	}
};

NSCAPI::nagiosReturn CheckDisk::CheckFileSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}
	checkHolders::SizeMaxMin<> crit;
	checkHolders::SizeMaxMin<> warn;
	bool bShowAll = false;
	std::list<PathConatiner> paths;

	std::list<std::string>::const_iterator cit;
	for (cit=args.begin();cit!=args.end();++cit) {
		std::string arg = *cit;
		std::pair<std::string,std::string> p = strEx::split(arg,"=");
		if (p.first == "File") {
			paths.push_back(PathConatiner(p.second, warn, crit));
		} else if (p.first == "MaxWarn") {
			warn.max.set(p.second);
		} else if (p.first == "MinWarn") {
			warn.min.set(p.second);
		} else if (p.first == "MaxCrit") {
			crit.max.set(p.second);
		} else if (p.first == "MinCrit") {
			crit.min.set(p.second);
		} else if (p.first == SHOW_ALL) {
			bShowAll = true;
		} else if (p.first.find(":") != std::string::npos) {
			std::pair<std::string,std::string> p2 = strEx::split(p.first,":");
			if (p2.first == "File") {
				paths.push_back(PathConatiner(p.second, p2.second, warn, crit));
			} else {
				message = "Unknown command: " + p.first;
				return NSCAPI::returnCRIT;
			}
		} else {
			message = "Unknown command: " + p.first;
			return NSCAPI::returnCRIT;
		}
	}

	std::list<PathConatiner>::const_iterator pit;
	for (pit = paths.begin(); pit != paths.end(); ++pit) {
		PathConatiner path = (*pit);
		std::string tstr;
		GetSize sizeFinder;
		std::string sName = path.getName();
		RecursiveScanDirectory(path.getPath(), sizeFinder);

		if (path.crit_.max.hasBounds() && path.crit_.max.checkMAX(sizeFinder.getSize())) {
			tstr += path.crit_.max.prettyPrint(sName, sizeFinder.getSize()) + " > critical";
			NSCHelper::escalteReturnCodeToCRIT(returnCode);
		} else if (path.crit_.min.hasBounds() && path.crit_.min.checkMIN(sizeFinder.getSize())) {
			tstr += path.crit_.min.prettyPrint(sName, sizeFinder.getSize()) + " < critical";
			NSCHelper::escalteReturnCodeToCRIT(returnCode);
		} else if (path.warn_.max.hasBounds() && path.warn_.max.checkMAX(sizeFinder.getSize())) {
			tstr += path.warn_.max.prettyPrint(sName, sizeFinder.getSize()) + " > warning";
			NSCHelper::escalteReturnCodeToWARN(returnCode);
		} else if (path.warn_.min.hasBounds() && path.warn_.min.checkMIN(sizeFinder.getSize())) {
			tstr += path.warn_.min.prettyPrint(sName, sizeFinder.getSize()) + " < warning";
			NSCHelper::escalteReturnCodeToWARN(returnCode);
		} else if (bShowAll) {
			tstr = sName +  ": " + strEx::itos_as_BKMG(sizeFinder.getSize());
		}
		perf += checkHolders::SizeMaxMin<>::printPerf(sName, sizeFinder.getSize(), path.warn_, path.crit_);
		if (!message.empty() && !tstr.empty())
			message += ", ";
		if (!tstr.empty())
			message = tstr;
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

//	} else if (command == "CheckFileDate") {
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gCheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckDisk);
