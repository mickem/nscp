/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include "../AutoBuild.h"
// Application Name
#define SZAPPNAME _T("NSClient++")

// Version
#define SZBETATAG _T(" ") // _T(" RC ")  _T(" BETA ") 
#define SZVERSION STRPRODUCTVER SZBETATAG STRPRODUCTDATE
//FILEVER[0]

#if defined(_M_IX86)
#define SZARCH _T("w32")
#elif defined(_M_X64)
#define SZARCH _T("x64")
#elif defined(_M_IA64)
#define SZARCH _T("ia64")
#else
#define SZARCH _T("unknown")
#endif

// internal name of the service
#define SZSERVICENAME        _T("NSClientpp")

// Description of service
#define SZSERVICEDESCRIPTION _T("Nagios Windows Agent (Provides performance data for Nagios server)")

// displayed name of the service
#define SZSERVICEDISPLAYNAME SZSERVICENAME _T(" (Nagios) ") SZVERSION _T(" ") SZARCH

// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       _T("")

// Buffer size of incoming data (notice this is the maximum request length!)
#define RECV_BUFFER_LEN		1024

#define NASTY_METACHARS         _T("|`&><'\"\\[]{}")        /* This may need to be modified for windows directory seperator */

#define DATE_FORMAT _T("%#c")


// Default Argument string (for consistency)
#define SHOW_ALL _T("ShowAll")
#define SHOW_FAIL _T("ShowFail")
#define NSCLIENT _T("nsclient")
#define IGNORE_PERFDATA _T("ignore-perf-data")
#define CHECK_ALL _T("CheckAll")
#define CHECK_ALL_OTHERS _T("CheckAllOthers")

// NSClient Setting headlines
#define NSCLIENT_SECTION_TITLE _T("NSClient")
#define NSCLIENT_SETTINGS_PORT _T("port")
#define NSCLIENT_SETTINGS_PORT_DEFAULT 12489
#define NSCLIENT_SETTINGS_VERSION _T("version")
#define NSCLIENT_SETTINGS_VERSION_DEFAULT _T("auto")
#define NSCLIENT_SETTINGS_BINDADDR _T("bind_to_address")
#define NSCLIENT_SETTINGS_BINDADDR_DEFAULT _T("")
#define NSCLIENT_SETTINGS_LISTENQUE _T("socket_back_log")
#define NSCLIENT_SETTINGS_LISTENQUE_DEFAULT 0
#define NSCLIENT_SETTINGS_READ_TIMEOUT _T("socket_timeout")
#define NSCLIENT_SETTINGS_READ_TIMEOUT_DEFAULT 30
#define NSCLIENT_SETTINGS_SYSTRAY_EXE _T("systray_exe")
#define NSCLIENT_SETTINGS_SYSTRAY_EXE_DEFAULT _T("systray.exe")

// NRPE Settings headlines
#define NRPE_SECTION_TITLE _T("NRPE")
#define NRPE_CLIENT_HANDLER_SECTION_TITLE _T("NRPE Client Handlers")
#define NRPE_HANDLER_SECTION_TITLE _T("NRPE Handlers")
#define NRPE_SETTINGS_TIMEOUT _T("command_timeout")
#define NRPE_SETTINGS_TIMEOUT_DEFAULT 60
#define NRPE_SETTINGS_READ_TIMEOUT _T("socket_timeout")
#define NRPE_SETTINGS_READ_TIMEOUT_DEFAULT 30
#define NRPE_SETTINGS_PORT _T("port")
#define NRPE_SETTINGS_PORT_DEFAULT 5666
#define NRPE_SETTINGS_BINDADDR _T("bind_to_address")
#define NRPE_SETTINGS_BINDADDR_DEFAULT _T("")
#define NRPE_SETTINGS_ALLOW_ARGUMENTS _T("allow_arguments")
#define NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT 0
#define NRPE_SETTINGS_ALLOW_NASTY_META _T("allow_nasty_meta_chars")
#define NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT 0
#define NRPE_SETTINGS_USE_SSL _T("use_ssl")
#define NRPE_SETTINGS_USE_SSL_DEFAULT 1
#define NRPE_SETTINGS_LISTENQUE _T("socket_back_log")
#define NRPE_SETTINGS_LISTENQUE_DEFAULT 0
#define NRPE_SETTINGS_PERFDATA _T("performance_data")
#define NRPE_SETTINGS_PERFDATA_DEFAULT 1
#define NRPE_SETTINGS_SCRIPTDIR _T("script_dir")
#define NRPE_SETTINGS_SCRIPTDIR_DEFAULT _T("")
#define NRPE_SETTINGS_STRLEN _T("string_length")
#define NRPE_SETTINGS_STRLEN_DEFAULT 1024

// External Script Settings headlines
#define EXTSCRIPT_SECTION_TITLE _T("External Script")
#define EXTSCRIPT_SCRIPT_SECTION_TITLE _T("External Scripts")
#define EXTSCRIPT_ALIAS_SECTION_TITLE _T("External Alias")
#define EXTSCRIPT_SETTINGS_TIMEOUT _T("command_timeout")
#define EXTSCRIPT_SETTINGS_TIMEOUT_DEFAULT 60
#define EXTSCRIPT_SETTINGS_ALLOW_ARGUMENTS _T("allow_arguments")
#define EXTSCRIPT_SETTINGS_ALLOW_ARGUMENTS_DEFAULT 0
#define EXTSCRIPT_SETTINGS_ALLOW_NASTY_META _T("allow_nasty_meta_chars")
#define EXTSCRIPT_SETTINGS_ALLOW_NASTY_META_DEFAULT 0
#define EXTSCRIPT_SETTINGS_SCRIPTDIR _T("script_dir")
#define EXTSCRIPT_SETTINGS_SCRIPTDIR_DEFAULT _T("")

// Check System Settings
#define C_SYSTEM_SECTION_TITLE _T("Check System")
#define C_SYSTEM_CPU_BUFFER_TIME _T("CPUBufferSize") 
#define C_SYSTEM_CPU_BUFFER_TIME_DEFAULT _T("1h")
#define C_SYSTEM_CHECK_RESOLUTION _T("CheckResolution")
#define C_SYSTEM_CHECK_RESOLUTION_DEFAULT 10 /* unit: second/10 */

#define C_SYSTEM_AUTODETECT_PDH _T("auto_detect_pdh")
#define C_SYSTEM_AUTODETECT_PDH_DEFAULT 1
#define C_SYSTEM_FORCE_LANGUAGE _T("force_language")
#define C_SYSTEM_FORCE_LANGUAGE_DEFAULT _T("")
#define C_SYSTEM_NO_INDEX _T("dont_use_pdh_index")
#define C_SYSTEM_NO_INDEX_DEFAULT 0
#define C_SYSTEM_IGNORE_COLLECTION _T("debug_skip_data_collection")
#define C_SYSTEM_IGNORE_COLLECTION_DEFAULT 0

#define C_SYSTEM_MEM_PAGE_LIMIT _T("MemoryCommitLimit")
#define C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT _T("\\Memory\\Commit Limit")
#define C_SYSTEM_MEM_PAGE _T("MemoryCommitByte")
#define C_SYSTEM_MEM_PAGE_DEFAULT _T("\\Memory\\Committed Bytes")
#define C_SYSTEM_UPTIME _T("SystemSystemUpTime")
#define C_SYSTEM_UPTIME_DEFAULT _T("\\System\\System Up Time")
#define C_SYSTEM_CPU _T("SystemTotalProcessorTime")
#define C_SYSTEM_MEM_CPU_DEFAULT _T("\\Processor(_total)\\% Processor Time")
#define C_SYSTEM_ENUMPROC_METHOD_PSAPI _T("PSAPI")
#define C_SYSTEM_ENUMPROC_METHOD_THELP _T("TOOLHELP")
#define C_SYSTEM_ENUMPROC_METHOD_AUTO _T("auto")
#define C_SYSTEM_ENUMPROC_METHOD _T("ProcessEnumerationMethod")
#define C_SYSTEM_ENUMPROC_METHOD_DEFAULT C_SYSTEM_ENUMPROC_METHOD_AUTO


#define EVENTLOG_SECTION_TITLE _T("Eventlog")
#define EVENTLOG_DEBUG _T("debug")
#define EVENTLOG_DEBUG_DEFAULT 0
#define EVENTLOG_SYNTAX _T("syntax")
#define EVENTLOG_SYNTAX_DEFAULT _T("")

#define NSCA_AGENT_SECTION_TITLE _T("NSCA Agent")
#define NSCA_CMD_SECTION_TITLE _T("NSCA Commands")
#define NSCA_INTERVAL _T("interval")
#define NSCA_INTERVAL_DEFAULT 60
#define NSCA_HOSTNAME _T("hostname")
#define NSCA_HOSTNAME_DEFAULT _T("")
#define NSCA_SERVER _T("nsca_host")
#define NSCA_SERVER_DEFAULT _T("unknown-host")
#define NSCA_PORT _T("nsca_port")
#define NSCA_PORT_DEFAULT 5667
#define NSCA_ENCRYPTION _T("encryption_method")
#define NSCA_ENCRYPTION_DEFAULT 1
#define NSCA_PASSWORD _T("password")
#define NSCA_PASSWORD_DEFAULT _T("")
#define NSCA_DEBUG_THREADS _T("debug_threads")
#define NSCA_DEBUG_THREADS_DEFAULT 1
#define NSCA_CACHE_HOST _T("cache_hostname")
#define NSCA_CACHE_HOST_DEFAULT 0

#define C_SYSTEM_SVC_ALL_0 _T("check_all_services[SERVICE_BOOT_START]")
#define C_SYSTEM_SVC_ALL_0_DEFAULT _T("ignored")
#define C_SYSTEM_SVC_ALL_1 _T("check_all_services[SERVICE_SYSTEM_START]")
#define C_SYSTEM_SVC_ALL_1_DEFAULT _T("ignored")
#define C_SYSTEM_SVC_ALL_2 _T("check_all_services[SERVICE_AUTO_START]")
#define C_SYSTEM_SVC_ALL_2_DEFAULT _T("started")
#define C_SYSTEM_SVC_ALL_3 _T("check_all_services[SERVICE_DEMAND_START]")
#define C_SYSTEM_SVC_ALL_3_DEFAULT _T("ignored")
#define C_SYSTEM_SVC_ALL_4 _T("check_all_services[SERVICE_DISABLED]")
#define C_SYSTEM_SVC_ALL_4_DEFAULT _T("stopped")

#define C_TASKSCHED_SECTION _T("Task Scheduler")
#define C_TASKSCHED_SYNTAX _T("syntax")
#define C_TASKSCHED_SYNTAX_DEFAULT _T("%title% last run: %most-recent-run-time% (%exit-code%)")

// Log to File Settings
#define LOG_SECTION_TITLE _T("log")
#define LOG_FILENAME _T("file") 
#define LOG_FILENAME_DEFAULT _T("nsclient.log")
#define LOG_DATEMASK _T("date_mask")
#define LOG_DATEMASK_DEFAULT _T("%Y-%m-%d %H:%M:%S")
#define LOG_ROOT _T("root_folder")
#define LOG_ROOT_DEFAULT _T("exe")

// Main Settings
#define MAIN_SECTION_TITLE _T("Settings")
#define MAIN_USEFILE _T("use_file")
#define MAIN_USEREG _T("use_reg")
#define MAIN_USEFILE_DEFAULT 0
#define MAIN_MASTERKEY _T("master_key") 
#define MAIN_MASTERKEY_DEFAULT _T("This is a secret key that you should change")
#define MAIN_OBFUSCATED_PASWD _T("obfuscated_password")
#define MAIN_OBFUSCATED_PASWD_DEFAULT _T("")
#define MAIN_SETTINGS_PWD _T("password")
#define MAIN_SETTINGS_PWD_DEFAULT _T("")
#define MAIN_ALLOWED_HOSTS _T("allowed_hosts")
#define MAIN_ALLOWED_HOSTS_DEFAULT _T("127.0.0.1")
#define MAIN_ALLOWED_HOSTS_CACHE _T("cache_allowed_hosts")
#define MAIN_ALLOWED_HOSTS_CACHE_DEFAULT 1
#define MAIN_STRING_LENGTH _T("string_length")
#define MAIN_STRING_LENGTH_DEFAULT 4096
#define MAIN_SHARED_SESSION _T("shared_session")
#define MAIN_SHARED_SESSION_DEFAULT 0


// LOA Config items
#define LUA_SCRIPT_SECTION_TITLE _T("LUA Scripts")


// Main Registry ROOT
#define NS_HKEY_ROOT HKEY_LOCAL_MACHINE
#define NS_REG_ROOT _T("SOFTWARE\\NSClient++")
