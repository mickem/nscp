#pragma once

#include <EnumNtSrv.h>



namespace NSCommands {
	typedef enum ServiceState { ok = 0, unknown = 1, error = 2 } ServiceState;
	std::string cmdServiceStateCheckItem(bool bShowAll, int &nState, TNtServiceInfo &info);
	std::string serviceState(std::list<std::string> args);
	std::string usedDiskSpace(std::list<std::string> args);
	std::string procState(std::list<std::string> args);
}