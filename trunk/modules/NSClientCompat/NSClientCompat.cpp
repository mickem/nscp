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
 #define REQ_CLIENTVERSION	1	// Works fine!
 #define REQ_CPULOAD		2	// Quirks
 - Needs settings for default buffer size (backlog) and possibly other things.
 - Buffer needs to be synced with the client (don't know the size of that)
 - I think the idea was that the buffer would recursive to make a smaller memory footprint.
	(ie. the first level buffer have samples from every second, next level samples from every minute, next level hours etc...)
	(and I don't know the status of this, doesn't seem to be that way)
 #define REQ_UPTIME			3	// Works fine!
 #define REQ_USEDDISKSPACE	4	// Works fine!
 #define REQ_SERVICESTATE	5	// Works fine!
 #define REQ_PROCSTATE		6	// Works fine!
 #define REQ_MEMUSE			7	// Works fine!
 //#define REQ_COUNTER		8	// ! - not implemented Have to look at this, if anyone has a sample let me know...
 //#define REQ_FILEAGE		9	// ! - not implemented Dont know how to use
 //#define REQ_INSTANCES	10	// ! - not implemented Dont know how to use
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
			{
				std::string version = NSCModuleHelper::getSettingsString("nsclient compat", "version", "modern");
				if (version == "modern")
					return NSCModuleHelper::getApplicationName() + " " + NSCModuleHelper::getApplicationVersionString();
				return version;
			}
		case REQ_UPTIME:
			return strEx::itos(pdhCollector->getUptime());

		case REQ_CPULOAD:
			{
				stl_args = NSCHelper::makelist(argLen, args);
				if (stl_args.empty())
					return "ERROR: Missing argument exception.";
				std::string ret;
				while (!stl_args.empty()) {
					std::string s = stl_args.front(); stl_args.pop_front();
					int v = pdhCollector->getCPUAvrage(strEx::stoi(s)*(60/CHECK_INTERVAL));
					if (v == -1)
						return ret;
					if (!ret.empty())
						ret += "&";
					ret += strEx::itos(v);
				}
				return ret;
			}
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
