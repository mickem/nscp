#pragma once


#include <settings/macros.h>

namespace setting_keys {
	// NSClient Setting headlines
	namespace nsclient {
		DEFINE_PATH(SECTION, NSCLIENT_SECTION);
		//DESCRIBE_SETTING(SECTION, "NSCLIENT SECTION", "Section for NSClient (NSClientListsner.dll) (check_nt) protocol options.");

		DEFINE_SETTING_S(ALLOWED_HOSTS, NSCLIENT_SECTION, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to NSClient deamon. If you leave this blank the global version will be used instead.");

		DEFINE_SETTING_I(PORT, NSCLIENT_SECTION, "port", 12489);
		//DESCRIBE_SETTING(PORT, "NSCLIENT PORT NUMBER", "This is the port the NSClientListener.dll will listen to.");

		DEFINE_SETTING_S(BINDADDR, NSCLIENT_SECTION, GENERIC_KEY_BIND_TO, "");
		//DESCRIBE_SETTING(BINDADDR, "BIND TO ADDRESS", "Allows you to bind server to a specific local address. This has to be a dotted ip adress not a hostname. Leaving this blank will bind to all avalible IP adresses.");

		DEFINE_SETTING_I(READ_TIMEOUT, NSCLIENT_SECTION, GENERIC_KEY_SOCK_READ_TIMEOUT, 30);
		//DESCRIBE_SETTING(READ_TIMEOUT, "SOCKET TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out.");

		DEFINE_SETTING_I(LISTENQUE, NSCLIENT_SECTION, GENERIC_KEY_SOCK_LISTENQUE, 0);
		//DESCRIBE_SETTING_ADVANCED(LISTENQUE, "LISTEN QUEUE", "Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.");

		DEFINE_SETTING_S(VERSION, NSCLIENT_SECTION, "version", "auto");
		//DESCRIBE_SETTING(VERSION, "VERSION", "The version number to return for the CLIENTVERSION check (useful to \"simulate\" an old/different version of the client, auto will be generated from the compiled version string inside NSClient++");

		DEFINE_SETTING_B(CACHE_ALLOWED, NSCLIENT_SECTION, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		//DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		DEFINE_SETTING_S(MASTER_KEY, NSCLIENT_SECTION, GENERIC_KEY_PWD_MASTER_KEY, "This is a secret key that you should change");
		//DESCRIBE_SETTING(MASTER_KEY, "MASTER KEY", "The secret \"key\" used when (de)obfuscating passwords.");

		DEFINE_SETTING_S(PWD, NSCLIENT_SECTION, GENERIC_KEY_PWD, "");
		//DESCRIBE_SETTING(PWD, "PASSWORD", "This is the password (-s) that is required to access NSClient remotely. If you leave this blank everyone will be able to access the daemon remotly.");

		DEFINE_SETTING_S(OBFUSCATED_PWD, NSCLIENT_SECTION, GENERIC_KEY_OBFUSCATED_PWD, "");
		//DESCRIBE_SETTING(OBFUSCATED_PWD, "OBFUSCATED PASSWORD", "This is the same as the password option but here you can store the password in an obfuscated manner. *NOTICE* obfuscation is *NOT* the same as encryption, someone with access to this file can still figure out the password. Its just a bit harder to do it at first glance.");

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