NSC_WRAPPERS_MAIN();
#include <Socket.h>
#include <SSLSocket.h>
#include <map>
#include "NRPEPacket.h"

class NRPEListener : public simpleSocket::ListenerHandler {
private:
	bool bUseSSL_;
	simpleSSL::Listener socket_ssl_;
	simpleSocket::Listener<> socket_;
	typedef std::map<std::string, std::string> commandList;
	commandList commands;
	unsigned int timeout;
	socketHelpers::allowedHosts allowedHosts;

public:
	NRPEListener();
	virtual ~NRPEListener();
	// Module calls
	bool loadModule();
	bool unloadModule();
	std::string getModuleName();
	NSCModuleWrapper::module_version getModuleVersion();
	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);

private:
	class NRPEException {
		std::string error_;
	public:
/*		NRPESocketException(simpleSSL::SSLException e) {
			error_ = e.getMessage();
		}
		NRPEException(NRPEPacket::NRPEPacketException e) {
			error_ = e.getMessage();
		}
		*/
		NRPEException(std::string s) {
			error_ = s;
		}
		std::string getMessage() {
			return error_;
		}
	};


private:
	void onAccept(simpleSocket::Socket *client);
	void onClose();


	NRPEPacket handlePacket(NRPEPacket p);
	int executeNRPECommand(std::string command, std::string &msg, std::string &perf);
	void addCommand(std::string key, std::string args) {
		commands[key] = args;
	}

};

