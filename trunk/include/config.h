

#pragma once

// Application Name
#define SZAPPNAME "NSClient++"

// Version
#define SZVERSION "0.0.2 alfa 2005-03-27"

// internal name of the service
#define SZSERVICENAME        "NSClient++"

// displayed name of the service
#define SZSERVICEDISPLAYNAME SZSERVICENAME " (Nagios) " SZVERSION

// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       ""

// Buffer size of incoming data (noteice this is the maximum request length!)
#define RECV_BUFFER_LEN		1024

// The default NRPE port (used by NSRPListener plugin)
#define DEFAULT_NRPE_PORT 5666

// The default NSClient port (used by NSClientListener plugin)
#define DEFAULT_NSCLIENT_PORT 12489

// NRPE Settings headlines
#define NRPE_SECTION_TITLE "NRPE"
#define NRPE_HANDLER_SECTION_TITLE "NRPE Handlers"
#define NRPE_SETTINGS_TIMEOUT "command_timeout"
#define NRPE_SETTINGS_ALLOWED "allowed_hosts"
#define NRPE_SETTINGS_PORT "port"
#define NRPE_SETTINGS_ALLOW_ARGUMENTS "allow_arguments"
#define NRPE_SETTINGS_ALLOW_NASTY_META "allow_nasty_meta_chars"


