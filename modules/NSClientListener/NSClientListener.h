#include <Socket.h>
#include <string>
#include <utils.h>

NSC_WRAPPERS_MAIN();

class NSClientListener  : public simpleSocket::ListenerHandler {
private:
	simpleSocket::Listener<> socket;
	socketHelpers::allowedHosts allowedHosts;

public:
	NSClientListener();
	virtual ~NSClientListener();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	std::string parseRequest(std::string buffer);

	// simpleSocket::ListenerHandler implementation
	void onAccept(simpleSocket::Socket *client);
	void onClose();

};