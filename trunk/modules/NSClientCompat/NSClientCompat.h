#pragma once
#include "PDHCollector.h"

NSC_WRAPPERS_MAIN();

class NSClientCompat {
private:
	PDHCollectorThread pdhThread;
	PDHCollector *pdhCollector;

public:
	typedef struct rB {
		NSCAPI::nagiosReturn code_;
		std::string msg_;
		std::string perf_;
		rB(NSCAPI::nagiosReturn code, std::string msg) : code_(code), msg_(msg) {}
		rB() : code_(NSCAPI::returnUNKNOWN) {}
	} returnBundle;

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