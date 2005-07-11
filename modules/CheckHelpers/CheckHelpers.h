NSC_WRAPPERS_MAIN();
#include <config.h>
#include <strEx.h>

class CheckHelpers {
private:

public:
	CheckHelpers();
	virtual ~CheckHelpers();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::string getModuleName() {
		return "Helper function";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 3, 0 };
		return version;
	}
	std::string getModuleDescription() {
		return "Various helper function to extend other checks.\nThis is also only supported through NRPE.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);

	// Check commands
	NSCAPI::nagiosReturn checkMultiple(const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
};