NSC_WRAPPERS_MAIN();

#include <strEx.h>
#include <config.h>
#include <checkHelpers.hpp>
#include <filter_framework.hpp>


class CheckEventLog {
private:

public:
	CheckEventLog();
	virtual ~CheckEventLog();
	// Module calls
	bool loadModule();
	bool unloadModule();

	std::string getModuleName() {
		return "Event log Checker.";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::string getModuleDescription() {
		return "Check for errors and warnings in the event log.\nThis is only supported through NRPE so if you plan to use only NSClient this wont help you at all.";
	}


	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
};