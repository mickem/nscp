#pragma once

NSC_WRAPPERS_MAIN();

class FileLogger {
private:
	std::string file_;
	std::string format_;

public:
	FileLogger();
	virtual ~FileLogger();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getConfigurationMeta();


	std::string getModuleName() {
		return "File logger";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::string getModuleDescription() {
		return "Writes errors and (if configured) debug info to a text file.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	void handleMessage(int msgType, char* file, int line, char* message);
	int handleCommand(char* command, char **argument, char *returnBuffer, int returnBufferLen);


	std::string getFileName();
};