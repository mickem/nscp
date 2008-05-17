#pragma once

#include <EnumNtSrv.h>
#include "NSClientCompat.h"


namespace NSCommands {
	typedef enum ServiceState { ok = 0, unknown = 1, error = 2 } ServiceState;
	std::string cmdServiceStateCheckItem(bool bShowAll, int &nState, TNtServiceInfo &info);
	NSClientCompat::returnBundle serviceState(std::list<std::string> args);
	NSClientCompat::returnBundle usedDiskSpace(std::list<std::string> args);
	NSClientCompat::returnBundle procState(std::list<std::string> args);
};