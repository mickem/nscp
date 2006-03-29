#pragma once

// Application Name
#define SZAPPNAME "NSClient++"

// Version
#define SZVERSION "0.2.5h 2006-03-01"

// internal name of the service
#define SZSERVICENAME        "NSClientpp"

// Description of service
#define SZSERVICEDESCRIPTION "Nagios Windows Agent (Provides performance data for Nagios server)"

// displayed name of the service
#define SZSERVICEDISPLAYNAME SZSERVICENAME " (Nagios) " SZVERSION

// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       ""

// Buffer size of incoming data (notice this is the maximum request length!)
#define RECV_BUFFER_LEN		1024

#define NASTY_METACHARS         "|`&><'\"\\[]{}"        /* This may need to be modified for windows directory seperator */

#define DATE_FORMAT "%#c"
 

// Default Argument string (for consistency)
#define SHOW_ALL "ShowAll"
#define SHOW_FAIL "ShowFail"
#define NSCLIENT "nsclient"
#define CHECK_ALL "CheckAll"

// NSClient Setting headlines
#define NSCLIENT_SECTION_TITLE "NSClient"
#define NSCLIENT_SETTINGS_PORT "port"
#define NSCLIENT_SETTINGS_PORT_DEFAULT 12489
#define NSCLIENT_SETTINGS_VERSION "version"
#define NSCLIENT_SETTINGS_VERSION_DEFAULT "auto"
#define NSCLIENT_SETTINGS_BINDADDR "bind_to_address"
#define NSCLIENT_SETTINGS_BINDADDR_DEFAULT ""
#define NSCLIENT_SETTINGS_LISTENQUE "socket_back_log"
#define NSCLIENT_SETTINGS_LISTENQUE_DEFAULT 0

// NRPE Settings headlines
#define NRPE_SECTION_TITLE "NRPE"
#define NRPE_HANDLER_SECTION_TITLE "NRPE Handlers"
#define NRPE_SETTINGS_TIMEOUT "command_timeout"
#define NRPE_SETTINGS_TIMEOUT_DEFAULT 60
#define NRPE_SETTINGS_PORT "port"
#define NRPE_SETTINGS_PORT_DEFAULT 5666
#define NRPE_SETTINGS_BINDADDR "bind_to_address"
#define NRPE_SETTINGS_BINDADDR_DEFAULT ""
#define NRPE_SETTINGS_ALLOW_ARGUMENTS "allow_arguments"
#define NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT 0
#define NRPE_SETTINGS_ALLOW_NASTY_META "allow_nasty_meta_chars"
#define NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT 0
#define NRPE_SETTINGS_USE_SSL "use_ssl"
#define NRPE_SETTINGS_USE_SSL_DEFAULT 1
#define NRPE_SETTINGS_LISTENQUE "socket_back_log"
#define NRPE_SETTINGS_LISTENQUE_DEFAULT 0

// Check System Settings
#define C_SYSTEM_SECTION_TITLE "Check System"
#define C_SYSTEM_CPU_BUFFER_TIME "CPUBufferSize" 
#define C_SYSTEM_CPU_BUFFER_TIME_DEFAULT "1h"
#define C_SYSTEM_CHECK_RESOLUTION "CheckResolution"
#define C_SYSTEM_CHECK_RESOLUTION_DEFAULT 10 /* unit: second/10 */

#define C_SYSTEM_AUTODETECT_PDH "auto_detect_pdh"
#define C_SYSTEM_AUTODETECT_PDH_DEFAULT 1

#define C_SYSTEM_MEM_PAGE_LIMIT "MemoryCommitLimit"
#define C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT "\\Memory\\Commit Limit"
#define C_SYSTEM_MEM_PAGE "MemoryCommitByte"
#define C_SYSTEM_MEM_PAGE_DEFAULT "\\Memory\\Committed Bytes"
#define C_SYSTEM_UPTIME "SystemSystemUpTime"
#define C_SYSTEM_UPTIME_DEFAULT "\\System\\System Up Time"
#define C_SYSTEM_CPU "SystemTotalProcessorTime"
#define C_SYSTEM_MEM_CPU_DEFAULT "\\Processor(_total)\\% Processor Time"
#define C_SYSTEM_ENUMPROC_METHOD_PSAPI "PSAPI"
#define C_SYSTEM_ENUMPROC_METHOD_THELP "TOOLHELP"
#define C_SYSTEM_ENUMPROC_METHOD_AUTO "auto"
#define C_SYSTEM_ENUMPROC_METHOD "ProcessEnumerationMethod"
#define C_SYSTEM_ENUMPROC_METHOD_DEFAULT C_SYSTEM_ENUMPROC_METHOD_AUTO

// Log to File Settings
#define LOG_SECTION_TITLE "log"
#define LOG_FILENAME "file" 
#define LOG_FILENAME_DEFAULT "nsclient.log"
#define LOG_DATEMASK "date_mask"
#define LOG_DATEMASK_DEFAULT "%Y-%m-%d %H:%M:%S"

// Main Settings
#define MAIN_SECTION_TITLE "Settings"
#define MAIN_USEFILE "use_file"
#define MAIN_USEFILE_DEFAULT 0
#define MAIN_MASTERKEY "master_key" 
#define MAIN_MASTERKEY_DEFAULT "This is a secret key that you should change"
#define MAIN_OBFUSCATED_PASWD "obfuscated_password"
#define MAIN_OBFUSCATED_PASWD_DEFAULT ""
#define MAIN_SETTINGS_PWD "password"
#define MAIN_SETTINGS_PWD_DEFAULT ""
#define MAIN_ALLOWED_HOSTS "allowed_hosts"
#define MAIN_ALLOWED_HOSTS_DEFAULT "127.0.0.1"


// Main Registry ROOT
#define NS_HKEY_ROOT HKEY_LOCAL_MACHINE
#define NS_REG_ROOT "SOFTWARE\\NSClient++"
