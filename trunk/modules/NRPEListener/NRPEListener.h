NSC_WRAPPERS_MAIN();


#include "NRPESocket.h"
#include <Socket.h>
#include <map>

class NRPEListener {
private:
	NRPESocket socket;
	typedef std::map<std::string, std::string> commandList;
	commandList commands;
	unsigned int timeout;

public:
	NRPEListener();
	virtual ~NRPEListener();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);

private:
	int executeNRPECommand(std::string command, std::string &msg, std::string &perf);
	void addCommand(std::string key, std::string args) {
		commands[key] = args;
	}
};

