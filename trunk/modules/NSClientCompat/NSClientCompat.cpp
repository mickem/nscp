// NSClientCompat.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "NSClientCompat.h"

#include "NSCommands.h"

NSClientCompat gNSClientCompat;

/**
 * DLL Entry point
 * @param hModule 
 * @param ul_reason_for_call 
 * @param lpReserved 
 * @return 
 */
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

/**
 * Default c-tor
 * @return 
 */
NSClientCompat::NSClientCompat() : pdhCollector(NULL) {}
/**
 * Default d-tor
 * @return 
 */
NSClientCompat::~NSClientCompat() {}
/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NSClientCompat::loadModule() {
	pdhCollector = pdhThread.createThread();
	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NSClientCompat::unloadModule() {
	if (!pdhThread.exitThread())
		NSC_LOG_ERROR("Could not exit the thread, memory leak and potential corruption may be the result...");
	return true;
}
/**
 * Return the module name.
 * @return The module name
 */
std::string NSClientCompat::getModuleName() {
	return "NSClient compatibility Module.";
}
/**
 * Module version
 * @return module version
 */
NSCModuleWrapper::module_version NSClientCompat::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}
/**
 * Check if we have a command handler.
 * @return true (as we have a command handler)
 */
bool NSClientCompat::hasCommandHandler() {
	return true;
}
/**
 * Check if we have a message handler.
 * @return false as we have no message handler
 */
bool NSClientCompat::hasMessageHandler() {
	return false;
}
/*
*/
/**
 * Main command parser and delegator.
 * This also handles a lot of the simpler responses (though some are deferred to other helper functions)
 *
 #define REQ_CLIENTVERSION	1	// x
 #define REQ_CPULOAD		2	// x
 #define REQ_UPTIME			3	// x
 #define REQ_USEDDISKSPACE	4	// x
 #define REQ_SERVICESTATE	5	// x
 #define REQ_PROCSTATE		6	// x
 #define REQ_MEMUSE			7	// x
 //#define REQ_COUNTER		8	// ! - not implemented
 //#define REQ_FILEAGE		9	// ! - not implemented
 //#define REQ_INSTANCES	10	// ! - not implemented
 *
 * @param command 
 * @param argLen 
 * @param **args 
 * @return 
 */
std::string NSClientCompat::handleCommand(const std::string command, const unsigned int argLen, char **args) {
	std::list<std::string> stl_args;
	int id = atoi(command.c_str());
	if (id == 0)
		return "";
	switch (id) {
		case REQ_CLIENTVERSION:
			return NSCModuleHelper::getApplicationName() + " " + NSCModuleHelper::getApplicationVersionString();
		
		case REQ_UPTIME:
			return strEx::itos(pdhCollector->getUptime());

		case REQ_CPULOAD:
			stl_args = NSCHelper::makelist(argLen, args);
			if (stl_args.empty())
				return "ERROR: Missing argument exception.";
			return strEx::itos(pdhCollector->getCPUAvrage(strEx::stoi(stl_args.front())*(60/CHECK_INTERVAL)));

		case REQ_SERVICESTATE:
			return NSCommands::serviceState(NSCHelper::makelist(argLen, args));

		case REQ_PROCSTATE:
			return NSCommands::procState(NSCHelper::makelist(argLen, args));

		case REQ_MEMUSE:
			return strEx::itos(pdhCollector->getMemCommitLimit()) + "&" + 
				strEx::itos(pdhCollector->getMemCommit());

		case REQ_USEDDISKSPACE:
			return NSCommands::usedDiskSpace(NSCHelper::makelist(argLen, args));
	}
	return "";
}


NSC_WRAPPERS_MAIN_DEF(gNSClientCompat);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNSClientCompat);
