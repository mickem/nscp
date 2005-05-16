#pragma once
#include "PDHCollector.h"

NSC_WRAPPERS_MAIN();

class CheckSystem {
private:
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
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);


	NSCAPI::nagiosReturn checkCPU(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkUpTime(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkServiceState(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkMem(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkProcState(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);
	NSCAPI::nagiosReturn checkCounter(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf);


};