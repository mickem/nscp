NSC_WRAPPERS_MAIN();

class CheckEventLog {
private:

public:
	CheckEventLog();
	virtual ~CheckEventLog();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	std::string handleCommand(const std::string command, const unsigned int argLen, char **args);
};