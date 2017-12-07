

# Modules


| Type    | Module                                                       | Description                                                                                                                                                                                        |
|---------|--------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| misc    | [CauseCrashes](reference/misc/CauseCrashes)                  | *DO NOT USE* This module is usefull except for debugging purpouses and outright dangerous as it allows someone remotley to crash your client!                                                      |
| windows | [CheckDisk](reference/windows/CheckDisk)                     | CheckDisk can check various file and disk related things.                                                                                                                                          |
| windows | [CheckEventLog](reference/windows/CheckEventLog)             | Check for errors and warnings in the event log.                                                                                                                                                    |
| check   | [CheckExternalScripts](reference/check/CheckExternalScripts) | Module used to execute external scripts                                                                                                                                                            |
| check   | [CheckHelpers](reference/check/CheckHelpers)                 | Various helper function to extend other checks.                                                                                                                                                    |
| check   | [CheckLogFile](reference/check/CheckLogFile)                 | File for checking log files and various other forms of updating text files                                                                                                                         |
| check   | [CheckMKClient](reference/check/CheckMKClient)               | check_mk client can be used both from command line and from queries to check remote systes via check_mk                                                                                            |
| check   | [CheckMKServer](reference/check/CheckMKServer)               | A server that listens for incoming check_mk connection and processes incoming requests.                                                                                                            |
| misc    | [CheckNet](reference/misc/CheckNet)                          | Network related check such as check_ping.                                                                                                                                                          |
| check   | [CheckNSCP](reference/check/CheckNSCP)                       | Use this module to check the healt and status of NSClient++ it self                                                                                                                                |
| windows | [CheckSystem](reference/windows/CheckSystem)                 | Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.                                                                                       |
| windows | [CheckTaskSched](reference/windows/CheckTaskSched)           | Check status of your scheduled jobs.                                                                                                                                                               |
| windows | [CheckWMI](reference/windows/CheckWMI)                       | Check status via WMI                                                                                                                                                                               |
| misc    | [CollectdClient](reference/misc/CollectdClient)              | CollectD client can be used to submit metrics to a collectd server                                                                                                                                 |
| generic | [CommandClient](reference/generic/CommandClient)             | A command line client, generally not used except with "nscp test".                                                                                                                                 |
| windows | [DotnetPlugins](reference/windows/DotnetPlugins)             | Plugin to load and manage plugins written in dot net.                                                                                                                                              |
| misc    | [ElasticClient](reference/misc/ElasticClient)                | Elastic sends metrics, events and logs to elastic search                                                                                                                                           |
| client  | [GraphiteClient](reference/client/GraphiteClient)            | Graphite client can be used to submit graph data to a graphite graphing system                                                                                                                     |
| generic | [LUAScript](reference/generic/LUAScript)                     | Loads and processes internal Lua scripts                                                                                                                                                           |
| client  | [NRDPClient](reference/client/NRDPClient)                    | NRDP client can be used both from command line and from queries to check remote systes via NRDP                                                                                                    |
| client  | [NRPEClient](reference/client/NRPEClient)                    | NRPE client can be used both from command line and from queries to check remote systes via NRPE as well as configure the NRPE server                                                               |
| client  | [NRPEServer](reference/client/NRPEServer)                    | A server that listens for incoming NRPE connection and processes incoming requests.                                                                                                                |
| client  | [NSCAClient](reference/client/NSCAClient)                    | NSCA client can be used both from command line and from queries to submit passive checks via NSCA                                                                                                  |
| client  | [NSCAServer](reference/client/NSCAServer)                    | A server that listens for incoming NSCA connection and processes incoming requests.                                                                                                                |
| windows | [NSClientServer](reference/windows/NSClientServer)           | A server that listens for incoming check_nt connection and processes incoming requests.                                                                                                            |
| misc    | [NSCPClient](reference/misc/NSCPClient)                      | NSCP client can be used both from command line and from queries to check remote systes via NSCP (REST)                                                                                             |
| misc    | [Op5Client](reference/misc/Op5Client)                        | Client for connecting nativly to the Op5 Nortbound API                                                                                                                                             |
| generic | [PythonScript](reference/generic/PythonScript)               | Loads and processes internal Python scripts                                                                                                                                                        |
| misc    | [SamplePluginSimple](reference/misc/SamplePluginSimple)      | This is a sample plugin used to demonstrate how to make plugins                                                                                                                                    |
| generic | [Scheduler](reference/generic/Scheduler)                     | Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA                                                                                      |
| generic | [SimpleCache](reference/generic/SimpleCache)                 | Stores status updates and allows for active checks to retrieve them                                                                                                                                |
| generic | [SimpleFileWriter](reference/generic/SimpleFileWriter)       | Write status updates to a text file (A bit like the NSCA server does)                                                                                                                              |
| client  | [SMTPClient](reference/client/SMTPClient)                    | SMTP client can be used both from command line and from queries to check remote systes via SMTP                                                                                                    |
| client  | [SyslogClient](reference/client/SyslogClient)                | Forward information as syslog messages to a syslog server                                                                                                                                          |
| generic | [WEBServer](reference/generic/WEBServer)                     | A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module. |


# Queries


        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
| Module             | Command                                                                     | Description                                                                         |
|--------------------|-----------------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| CauseCrashes       | [crash_client](reference/misc/CauseCrashes#crash_client)                    | Raise a fatal exception (zero pointer reference) and cause NSClient++ crash.        |
| CheckDisk          | [check_drivesize](reference/windows/CheckDisk#check_drivesize)              | Check the size (free-space) of a drive or volume.                                   |
| CheckDisk          | [check_files](reference/windows/CheckDisk#check_files)                      | Check various aspects of a file and/or folder.                                      |
| CheckDisk          | [checkdrivesize](reference/windows/CheckDisk#checkdrivesize)                | **Deprecated** please use: :query:`check_drivesize`                                 |
| CheckDisk          | [checkfiles](reference/windows/CheckDisk#checkfiles)                        | **Deprecated** please use: :query:`check_drivesize`                                 |
| CheckEventLog      | [check_eventlog](reference/windows/CheckEventLog#check_eventlog)            | Check for errors in the event log.                                                  |
| CheckEventLog      | [checkeventlog](reference/windows/CheckEventLog#checkeventlog)              | **Deprecated** please use: :query:`check_eventlog`                                  |
| CheckHelpers       | [check_always_critical](reference/check/CheckHelpers#check_always_critical) | Run another check and regardless of its return code return CRITICAL.                |
| CheckHelpers       | [check_always_ok](reference/check/CheckHelpers#check_always_ok)             | Run another check and regardless of its return code return OK.                      |
| CheckHelpers       | [check_always_warning](reference/check/CheckHelpers#check_always_warning)   | Run another check and regardless of its return code return WARNING.                 |
| CheckHelpers       | [check_and_forward](reference/check/CheckHelpers#check_and_forward)         | Run a check and forward the result as a passive check.                              |
| CheckHelpers       | [check_critical](reference/check/CheckHelpers#check_critical)               | Just return CRITICAL (anything passed along will be used as a message).             |
| CheckHelpers       | [check_multi](reference/check/CheckHelpers#check_multi)                     | Run more then one check and return the worst state.                                 |
| CheckHelpers       | [check_negate](reference/check/CheckHelpers#check_negate)                   | Run a check and alter the return status codes according to arguments.               |
| CheckHelpers       | [check_ok](reference/check/CheckHelpers#check_ok)                           | Just return OK (anything passed along will be used as a message).                   |
| CheckHelpers       | [check_timeout](reference/check/CheckHelpers#check_timeout)                 | Run a check and timeout after a given amount of time if the check has not returned. |
| CheckHelpers       | [check_version](reference/check/CheckHelpers#check_version)                 | Just return the NSClient++ version.                                                 |
| CheckHelpers       | [check_warning](reference/check/CheckHelpers#check_warning)                 | Just return WARNING (anything passed along will be used as a message).              |
| CheckHelpers       | [filter_perf](reference/check/CheckHelpers#filter_perf)                     | Run a check and filter performance data.                                            |
| CheckHelpers       | [render_perf](reference/check/CheckHelpers#render_perf)                     | Run a check and render the performance data as output message.                      |
| CheckHelpers       | [xform_perf](reference/check/CheckHelpers#xform_perf)                       | Run a check and transform the performance data in various (currently one) way.      |
| CheckLogFile       | [check_logfile](reference/check/CheckLogFile#check_logfile)                 | Check for errors in log file or generic pattern matching in text files.             |
| CheckMKClient      | [check_mk_query](reference/check/CheckMKClient#check_mk_query)              | Request remote information via check_mk.                                            |
| CheckNet           | [check_ping](reference/misc/CheckNet#check_ping)                            | Ping another host and check the result.                                             |
| CheckNSCP          | [check_nscp](reference/check/CheckNSCP#check_nscp)                          | Check the internal healt of NSClient++.                                             |
| CheckNSCP          | [check_nscp_version](reference/check/CheckNSCP#check_nscp_version)          | Check the version of NSClient++ which is used.                                      |
| CheckSystem        | [check_cpu](reference/windows/CheckSystem#check_cpu)                        | Check that the load of the CPU(s) are within bounds.                                |
| CheckSystem        | [check_memory](reference/windows/CheckSystem#check_memory)                  | Check free/used memory on the system.                                               |
| CheckSystem        | [check_network](reference/windows/CheckSystem#check_network)                | Check network interface status.                                                     |
| CheckSystem        | [check_os_version](reference/windows/CheckSystem#check_os_version)          | Check the version of the underlaying OS.                                            |
| CheckSystem        | [check_pagefile](reference/windows/CheckSystem#check_pagefile)              | Check the size of the system pagefile(s).                                           |
| CheckSystem        | [check_pdh](reference/windows/CheckSystem#check_pdh)                        | Check the value of a performance (PDH) counter on the local or remote system.       |
| CheckSystem        | [check_process](reference/windows/CheckSystem#check_process)                | Check state/metrics of one or more of the processes running on the computer.        |
| CheckSystem        | [check_service](reference/windows/CheckSystem#check_service)                | Check the state of one or more of the computer services.                            |
| CheckSystem        | [check_uptime](reference/windows/CheckSystem#check_uptime)                  | Check time since last server re-boot.                                               |
| CheckSystem        | [checkcounter](reference/windows/CheckSystem#checkcounter)                  | **Deprecated** please use: :query:`check_pdh`                                       |
| CheckSystem        | [checkcpu](reference/windows/CheckSystem#checkcpu)                          | **Deprecated** please use: :query:`check_cpu`                                       |
| CheckSystem        | [checkmem](reference/windows/CheckSystem#checkmem)                          | **Deprecated** please use: :query:`check_mem`                                       |
| CheckSystem        | [checkprocstate](reference/windows/CheckSystem#checkprocstate)              | **Deprecated** please use: :query:`check_process`                                   |
| CheckSystem        | [checkservicestate](reference/windows/CheckSystem#checkservicestate)        | **Deprecated** please use: :query:`check_service`                                   |
| CheckSystem        | [checkuptime](reference/windows/CheckSystem#checkuptime)                    | **Deprecated** please use: :query:`check_uptime`                                    |
| CheckTaskSched     | [check_tasksched](reference/windows/CheckTaskSched#check_tasksched)         | Check status of scheduled jobs.                                                     |
| CheckTaskSched     | [checktasksched](reference/windows/CheckTaskSched#checktasksched)           | **Deprecated** please use: :query:`check_tasksched`                                 |
| CheckWMI           | [check_wmi](reference/windows/CheckWMI#check_wmi)                           | Check a set of WMI values and return rows which are matching criteria.              |
| GraphiteClient     | [submit_graphite](reference/client/GraphiteClient#submit_graphite)          | Submit information to the remote Graphite server.                                   |
| NRDPClient         | [submit_nrdp](reference/client/NRDPClient#submit_nrdp)                      | Submit information to the remote NRDP Server.                                       |
| NRPEClient         | [check_nrpe](reference/client/NRPEClient#check_nrpe)                        | Request remote information via NRPE.                                                |
| NRPEClient         | [exec_nrpe](reference/client/NRPEClient#exec_nrpe)                          | Execute remote script via NRPE. (Most likely you want nrpe_query).                  |
| NRPEClient         | [nrpe_forward](reference/client/NRPEClient#nrpe_forward)                    | Forward the request as-is to remote host via NRPE.                                  |
| NRPEClient         | [nrpe_query](reference/client/NRPEClient#nrpe_query)                        | Request remote information via NRPE.                                                |
| NRPEClient         | [submit_nrpe](reference/client/NRPEClient#submit_nrpe)                      | Submit information to remote host via NRPE. (Most likely you want nrpe_query).      |
| NSCAClient         | [submit_nsca](reference/client/NSCAClient#submit_nsca)                      | Submit information to the remote NSCA server.                                       |
| NSCPClient         | [check_remote_nscp](reference/misc/NSCPClient#check_remote_nscp)            | Request remote information via NSCP.                                                |
| NSCPClient         | [exec_remote_nscp](reference/misc/NSCPClient#exec_remote_nscp)              | Execute remote script via NSCP.                                                     |
| NSCPClient         | [remote_nscp_query](reference/misc/NSCPClient#remote_nscp_query)            | Request remote information via NSCP.                                                |
| NSCPClient         | [remote_nscpforward](reference/misc/NSCPClient#remote_nscpforward)          | Forward the request as-is to remote host via NSCP.                                  |
| NSCPClient         | [submit_remote_nscp](reference/misc/NSCPClient#submit_remote_nscp)          | Submit information to remote host via NSCP.                                         |
| SamplePluginSimple | [sample_raw_command](reference/misc/SamplePluginSimple#sample_raw_command)  | This is a sample hello world command.                                               |
| SimpleCache        | [check_cache](reference/generic/SimpleCache#check_cache)                    | Fetch results from the cache.                                                       |
| SimpleCache        | [list_cache](reference/generic/SimpleCache#list_cache)                      | List all keys in the cache.                                                         |
| SMTPClient         | [submit_smtp](reference/client/SMTPClient#submit_smtp)                      | Submit information to the remote SMTP server.                                       |
| SyslogClient       | [submit_syslog](reference/client/SyslogClient#submit_syslog)                | Submit information to the remote syslog server.                                     |
