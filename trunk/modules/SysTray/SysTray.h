#pragma once

#include "TrayIcon.h"

NSC_WRAPPERS_MAIN();

class SysTray {
private:
	IconWidget icon;

public:
	SysTray();
	virtual ~SysTray();
	// Module calls
	bool loadModule();
	bool unloadModule();

	std::string getModuleName() {
		return "System Tray icon";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}

	std::string getModuleDescription() {
		return "A simple module that only displays a system tray icon when NSClient++ is running.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
};