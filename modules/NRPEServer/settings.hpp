#pragma once


#include <settings/macros.h>

namespace setting_keys {
	// NSClient Setting headlines
	namespace nrpe {
		DEFINE_PATH(SECTION, NRPE_SECTION_PROTOCOL);
		//DESCRIBE_SETTING(SECTION, "NRPE SECTION", "Section for NRPE (NRPEListener.dll) (check_nrpe) protocol options.");


		DEFINE_PATH(CH_SECTION, NRPE_CLIENT_HANDLER_SECTION);
		//DESCRIBE_SETTING(CH_SECTION, "CLIENT HANDLER SECTION", "...");

		DEFINE_SETTING_S(ALLOWED_HOSTS, NRPE_SECTION_PROTOCOL, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to NSClient deamon. If you leave this blank the global version will be used instead.");

		DEFINE_SETTING_I(PORT, NRPE_SECTION_PROTOCOL, "port", 5666);
		//DESCRIBE_SETTING(PORT, "NSCLIENT PORT NUMBER", "This is the port the NSClientListener.dll will listen to.");

		DEFINE_SETTING_S(BINDADDR, NRPE_SECTION_PROTOCOL, GENERIC_KEY_BIND_TO, "");
		//DESCRIBE_SETTING(BINDADDR, "BIND TO ADDRESS", "Allows you to bind server to a specific local address. This has to be a dotted ip adress not a hostname. Leaving this blank will bind to all avalible IP adresses.");

		DEFINE_SETTING_I(READ_TIMEOUT, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_READ_TIMEOUT, 30);
		//DESCRIBE_SETTING(READ_TIMEOUT, "SOCKET TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out.");

		DEFINE_SETTING_I(LISTENQUE, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_LISTENQUE, 0);
		//DESCRIBE_SETTING_ADVANCED(LISTENQUE, "LISTEN QUEUE", "Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.");

		DEFINE_SETTING_I(THREAD_POOL, NRPE_SECTION_PROTOCOL, "thread pool", 10);
		//DESCRIBE_SETTING_ADVANCED(THREAD_POOL, "THREAD POOL", "");



		DEFINE_SETTING_B(CACHE_ALLOWED, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		//DEFINE_SETTING_B(KEYUSE_SSL, NRPE_SECTION_PROTOCOL, GENERIC_KEY_USE_SSL, true);
		//DESCRIBE_SETTING(KEYUSE_SSL, "USE SSL SOCKET", "This option controls if SSL should be used on the socket.");

		//DEFINE_SETTING_I(PAYLOAD_LENGTH, NRPE_SECTION_PROTOCOL, "payload length", 1024);
		//DESCRIBE_SETTING_ADVANCED(PAYLOAD_LENGTH, "PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.");

		//DEFINE_SETTING_B(ALLOW_PERFDATA, NRPE_SECTION, "performance data", true);
		//DESCRIBE_SETTING_ADVANCED(ALLOW_PERFDATA, "PERFORMANCE DATA", "Send performance data back to nagios (set this to 0 to remove all performance data).");

		//DEFINE_SETTING_I(CMD_TIMEOUT, NRPE_SECTION, "command timeout", 60);
		//DESCRIBE_SETTING(CMD_TIMEOUT, "COMMAND TIMEOUT", "This specifies the maximum number of seconds that the NRPE daemon will allow plug-ins to finish executing before killing them off.");

		DEFINE_SETTING_B(ALLOW_ARGS, NRPE_SECTION, "allow arguments", false);
		//DESCRIBE_SETTING(ALLOW_ARGS, "COMMAND ARGUMENT PROCESSING", "This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.");

		DEFINE_SETTING_B(ALLOW_NASTY, NRPE_SECTION, "allow nasy characters", false);
		//DESCRIBE_SETTING(ALLOW_NASTY, "COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the NRPE daemon will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.");

	}
	namespace protocol_def {
		DEFINE_SETTING_S(ALLOWED_HOSTS, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to the all daemons. If leave this blank anyone can access the deamon remotly (NSClient still requires a valid password). The syntax is host or ip/mask so 192.168.0.0/24 will allow anyone on that subnet access");

		DEFINE_SETTING_B(CACHE_ALLOWED, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		//DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		DEFINE_SETTING_S(MASTER_KEY, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_PWD_MASTER_KEY, "This is a secret key that you should change");
		//DESCRIBE_SETTING(MASTER_KEY, "MASTER KEY", "The secret \"key\" used when (de)obfuscating passwords.");

		DEFINE_SETTING_S(PWD, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_PWD, "");
		//DESCRIBE_SETTING(PWD, "PASSWORD", "This is the password (-s) that is required to access NSClient remotely. If you leave this blank everyone will be able to access the daemon remotly.");

		DEFINE_SETTING_S(OBFUSCATED_PWD, DEFAULT_PROTOCOL_SECTION, GENERIC_KEY_OBFUSCATED_PWD, "");
		//DESCRIBE_SETTING(OBFUSCATED_PWD, "OBFUSCATED PASSWORD", "This is the same as the password option but here you can store the password in an obfuscated manner. *NOTICE* obfuscation is *NOT* the same as encryption, someone with access to this file can still figure out the password. Its just a bit harder to do it at first glance.");
	}

}