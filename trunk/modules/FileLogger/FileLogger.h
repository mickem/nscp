#pragma once

NSC_WRAPPERS_MAIN();

class FileLogger {
private:
	std::string file_;

public:
	FileLogger();
	virtual ~FileLogger();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	void handleMessage(int msgType, char* file, int line, char* message);
	int handleCommand(char* command, char **argument, char *returnBuffer, int returnBufferLen);
};