#pragma once

#include "TrayIcon.h"

NSC_WRAPPERS_MAIN();

class SysTray {
private:
	DWORD dwThreadID_;
	HANDLE hThread_;

public:
	SysTray();
	virtual ~SysTray();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
};