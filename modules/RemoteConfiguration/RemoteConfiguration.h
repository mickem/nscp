NSC_WRAPPERS_MAIN();
#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>

class RemoteConfiguration {
private:

public:
	RemoteConfiguration();
	virtual ~RemoteConfiguration();
	// Module calls
	bool loadModule();
	bool unloadModule();

	std::string getModuleName() {
		return "RemoteConfiguration";
	}
	std::string getModuleDescription() {
		return "RemoteConfiguration Allows remote configuration and administration of NSCP.";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
	int commandLineExec(const char* command,const unsigned int argLen,char** args);

	// Check commands
	NSCAPI::nagiosReturn writeConf(const unsigned int argLen, char **char_args, std::string &message);
	NSCAPI::nagiosReturn readConf(const unsigned int argLen, char **char_args, std::string &message);
	NSCAPI::nagiosReturn setVariable(const unsigned int argLen, char **char_args, std::string &message);
	NSCAPI::nagiosReturn getVariable(const unsigned int argLen, char **char_args, std::string &message);

private:
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsDiscSize> PathConatiner;
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinPercentageBoundsDiskSize> DriveConatiner;
};