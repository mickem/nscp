#include "PDHCollector.h"

NSC_WRAPPERS_MAIN();

class NSClientCompat {
private:
	PDHCollectorThread pdhThread;
	PDHCollector *pdhCollector;

public:
	NSClientCompat();
	virtual ~NSClientCompat();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
};