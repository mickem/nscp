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
	typedef std::map<strEx::blindstr, std::string> commandList;
	commandList commands;
	unsigned int timeout;
	socketHelpers::allowedHosts allowedHosts;

public:
	NRPEListener();
	virtual ~NRPEListener();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::string getModuleName() {
		return "NRPE server";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::string getModuleDescription() {
		return "A simple server that listens for incoming NRPE connection and handles them.\nNRPE is preferred over NSClient as it is more flexible. You can of cource use both NSClient and NRPE.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
	std::string getConfigurationMeta();

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
	void addCommand(strEx::blindstr key, std::string args) {
		commands[key] = args;
	}

};

