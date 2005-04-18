#pragma once

// Application Name
#define SZAPPNAME "NSClient++"

// Version
#define SZVERSION "0.0.9 rc1 2005-04-19"

// internal name of the service
#define SZSERVICENAME        "NSClient++"

// displayed name of the service
#define SZSERVICEDISPLAYNAME SZSERVICENAME " (Nagios) " SZVERSION

// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       ""

// Buffer size of incoming data (noteice this is the maximum request length!)
#define RECV_BUFFER_LEN		1024

#define NASTY_METACHARS         "|`&><'\"\\[]{}"        /* This may need to be modified for windows directory seperator */


// Default Argumentstring (for consistency)
#define SHOW_ALL "ShowAll"
#define SHOW_FAIL "ShowFail"
#define NSCLIENT "nsclient"

// NSClient Setting headlines
#define NSCLIENT_SECTION_TITLE "NSClient"
#define NSCLIENT_SETTINGS_PORT "port"
#define NSCLIENT_SETTINGS_PORT_DEFAULT 12489
#define NSCLIENT_SETTINGS_ALLOWED "allowed_hosts"
#define NSCLIENT_SETTINGS_ALLOWED_DEFAULT ""
#define NSCLIENT_SETTINGS_PWD "password"
#define NSCLIENT_SETTINGS_PWD_DEFAULT ""

// NRPE Settings headlines
#define NRPE_SECTION_TITLE "NRPE"
#define NRPE_HANDLER_SECTION_TITLE "NRPE Handlers"
#define NRPE_SETTINGS_TIMEOUT "command_timeout"
#define NRPE_SETTINGS_TIMEOUT_DEFAULT 60
#define NRPE_SETTINGS_ALLOWED "allowed_hosts"
#define NRPE_SETTINGS_ALLOWED_DEFAULT ""
#define NRPE_SETTINGS_PORT "port"
#define NRPE_SETTINGS_PORT_DEFAULT 5666
#define NRPE_SETTINGS_ALLOW_ARGUMENTS "allow_arguments"
#define NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT 0
#define NRPE_SETTINGS_ALLOW_NASTY_META "allow_nasty_meta_chars"
#define NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT 0
#define NRPE_SETTINGS_USE_SSL "use_ssl"
#define NRPE_SETTINGS_USE_SSL_DEFAULT 1

// Check System Settings
#define C_SYSTEM_SECTION_TITLE "Check System"
#define C_SYSTEM_CPU_BUFFER_TIME "CPUBufferSize" 
#define C_SYSTEM_CPU_BUFFER_TIME_DEFAULT "1h"
#define C_SYSTEM_CHECK_RESOLUTION "CheckResolution"
#define C_SYSTEM_CHECK_RESOLUTION_DEFAULT 10 /* unit: second/10 */
#define C_SYSTEM_MEM_PAGE_LIMIT "CounterPageLimit"
#define C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT "\\\\.\\Memory\\Commit Limit"
#define C_SYSTEM_MEM_PAGE "CounterPage"
#define C_SYSTEM_MEM_PAGE_DEFAULT "\\\\.\\Memory\\Committed Bytes"
#define C_SYSTEM_UPTIME "CounterUptime"
#define C_SYSTEM_UPTIME_DEFAULT "\\\\.\\System\\System Up Time"
#define C_SYSTEM_CPU "CounterCPU"
#define C_SYSTEM_MEM_CPU_DEFAULT "\\\\.\\Processor(_total)\\% Processor Time"

// Log to File Settings
#define LOG_SECTION_TITLE "log"
#define LOG_FILENAME "file" 
#define LOG_FILENAME_DEFAULT "nsclient.log" 

