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
	NSCAPI::nagiosReturn handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);

	// Check commands
	NSCAPI::nagiosReturn CheckFileSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
	NSCAPI::nagiosReturn CheckDriveSize(const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
};