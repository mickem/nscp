

#pragma once

// Application Name
#define SZAPPNAME "NSClient++"

// Version
#define SZVERSION "0.0.1 RC4 2005-03-01"

// internal name of the service
#define SZSERVICENAME        "NSClient++"

// displayed name of the service
#define SZSERVICEDISPLAYNAME SZSERVICENAME " (Nagios) " SZVERSION

// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       ""

// Buffer size of incoming data (noteice this is the maximum request length!)
#define RECV_BUFFER_LEN		1024

#define DEFAULT_TCP_PORT 12489