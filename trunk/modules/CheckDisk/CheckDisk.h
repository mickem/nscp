NSC_WRAPPERS_MAIN();

class CheckDisk {
private:

public:
	CheckDisk();
	virtual ~CheckDisk();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	std::string handleCommand(const std::string command, const unsigned int argLen, char **args);

	// Check commands
	std::string CheckFileSize(const unsigned int argLen, char **char_args);
};