#pragma once
#include "PDHCollector.h"
#include <CheckMemory.h>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();

class CheckSystem {
private:
	CheckMemory memoryChecker;
	int processMethod_;
	PDHCollectorThread pdhThread;

public:
	typedef enum { started, stopped } states;
	typedef struct rB {
		NSCAPI::nagiosReturn code_;
		std::string msg_;
		std::string perf_;
		rB(NSCAPI::nagiosReturn code, std::string msg) : code_(code), msg_(msg) {}
		rB() : code_(NSCAPI::returnUNKNOWN) {}
	} returnBundle;

public:
	CheckSystem();
	virtual ~CheckSystem();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getConfigurationMeta();

	/**
	* Return the module name.
	* @return The module name
	*/
	std::string getModuleName() {
		return "CheckSystem";
	}
	/**
	* Module version
	* @return module version
	*/
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 3, 0 };
		return version;
	}
	std::string getModuleDescription() {
		return "Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	int commandLineExec(const char* command,const unsigned int argLen,char** args);

	NSCAPI::nagiosReturn checkCPU(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkUpTime(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkServiceState(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkMem(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkProcState(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkCounter(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);

};