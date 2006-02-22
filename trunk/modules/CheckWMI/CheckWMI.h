NSC_WRAPPERS_MAIN();
#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>
#include "WMIQuery.h"

class CheckWMI {
private:
	WMIQuery wmiQuery;

public:
	CheckWMI();
	virtual ~CheckWMI();
	// Module calls
	bool loadModule();
	bool unloadModule();

	std::string getModuleName() {
		return "CheckWMI Various Disk related checks.";
	}
	std::string getModuleDescription() {
		return "CheckWMI can check various file and disk related things.\nThe current version has commands to check Size of hard drives and directories.";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);

	// Check commands
	NSCAPI::nagiosReturn CheckSimpleWMI(const unsigned int argLen, char **char_args, std::string &message, std::string &perf);


private:
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsDiscSize> PathConatiner;
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinPercentageBoundsDiskSize> DriveConatiner;
};