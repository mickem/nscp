#include "NSClientSocket.h"

NSC_WRAPPERS_MAIN();

class NSClientListener {
private:
	NSClientSocket socket;

public:
	NSClientListener();
	virtual ~NSClientListener();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
};