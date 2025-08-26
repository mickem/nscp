

# Modules


| Type    | Module                                             | Description                                                                                                                                                                                        |
|---------|----------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| misc    | [CauseCrashes](misc/CauseCrashes)                  | *DO NOT USE* This module is useful except for debugging purposes and outright dangerous as it allows someone remotely to crash your client!                                                        |
| windows | [CheckDisk](windows/CheckDisk)                     | CheckDisk can check various file and disk related things.                                                                                                                                          |
| misc    | [CheckDocker](misc/CheckDocker)                    | Use this module to detect and monitor docker containers                                                                                                                                            |
| windows | [CheckEventLog](windows/CheckEventLog)             | Check for errors and warnings in the event log.                                                                                                                                                    |
| check   | [CheckExternalScripts](check/CheckExternalScripts) | Module used to execute external scripts                                                                                                                                                            |
| check   | [CheckHelpers](check/CheckHelpers)                 | Various helper function to extend other checks.                                                                                                                                                    |
| check   | [CheckLogFile](check/CheckLogFile)                 | File for checking log files and various other forms of updating text files                                                                                                                         |
| check   | [CheckMKClient](check/CheckMKClient)               | check_mk client can be used both from command line and from queries to check remote systems via check_mk                                                                                           |
| check   | [CheckMKServer](check/CheckMKServer)               | A server that listens for incoming check_mk connection and processes incoming requests.                                                                                                            |
| misc    | [CheckNet](misc/CheckNet)                          | Network related check such as check_ping.                                                                                                                                                          |
| check   | [CheckNSCP](check/CheckNSCP)                       | Use this module to check the health and status of NSClient++ it self                                                                                                                               |
| windows | [CheckSystem](windows/CheckSystem)                 | Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.                                                                                       |
| windows | [CheckTaskSched](windows/CheckTaskSched)           | Check status of your scheduled jobs.                                                                                                                                                               |
| windows | [CheckWMI](windows/CheckWMI)                       | Check status via WMI                                                                                                                                                                               |
| misc    | [CollectdClient](misc/CollectdClient)              | CollectD client can be used to submit metrics to a collectd server                                                                                                                                 |
| generic | [CommandClient](generic/CommandClient)             | A command line client, generally not used except with "nscp test".                                                                                                                                 |
| misc    | [ElasticClient](misc/ElasticClient)                | Elastic sends metrics, events and logs to elastic search                                                                                                                                           |
| client  | [GraphiteClient](client/GraphiteClient)            | Graphite client can be used to submit graph data to a graphite graphing system                                                                                                                     |
| generic | [LUAScript](generic/LUAScript)                     | Loads and processes internal Lua scripts                                                                                                                                                           |
| client  | [NRDPClient](client/NRDPClient)                    | NRDP client can be used both from command line and from queries to check remote systems via NRDP                                                                                                   |
| client  | [NRPEClient](client/NRPEClient)                    | NRPE client can be used both from command line and from queries to check remote systems via NRPE as well as configure the NRPE server                                                              |
| client  | [NRPEServer](client/NRPEServer)                    | A server that listens for incoming NRPE connection and processes incoming requests.                                                                                                                |
| client  | [NSCAClient](client/NSCAClient)                    | NSCA client can be used both from command line and from queries to submit passive checks via NSCA                                                                                                  |
| client  | [NSCAServer](client/NSCAServer)                    | A server that listens for incoming NSCA connection and processes incoming requests.                                                                                                                |
| windows | [NSClientServer](windows/NSClientServer)           | A server that listens for incoming check_nt connection and processes incoming requests.                                                                                                            |
| misc    | [NSCPClient](misc/NSCPClient)                      | NSCP client can be used both from command line and from queries to check remote systems via NSCP (REST)                                                                                            |
| misc    | [Op5Client](misc/Op5Client)                        | Client for connecting nativly to the Op5 Nortbound API                                                                                                                                             |
| generic | [PythonScript](generic/PythonScript)               | Loads and processes internal Python scripts                                                                                                                                                        |
| misc    | [SamplePluginSimple](misc/SamplePluginSimple)      | This is a sample plugin used to demonstrate how to make plugins                                                                                                                                    |
| generic | [Scheduler](generic/Scheduler)                     | Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA                                                                                      |
| generic | [SimpleCache](generic/SimpleCache)                 | Stores status updates and allows for active checks to retrieve them                                                                                                                                |
| generic | [SimpleFileWriter](generic/SimpleFileWriter)       | Write status updates to a text file (A bit like the NSCA server does)                                                                                                                              |
| client  | [SMTPClient](client/SMTPClient)                    | SMTP client can be used both from command line and from queries to check remote systems via SMTP                                                                                                   |
| client  | [SyslogClient](client/SyslogClient)                | Forward information as syslog messages to a syslog server                                                                                                                                          |
| generic | [WEBServer](generic/WEBServer)                     | A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module. |


# Queries


        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
| Module             | Command                                                           | Description                                                                         |
|--------------------|-------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| CauseCrashes       | [crash_client](misc/CauseCrashes#crash_client)                    | Raise a fatal exception (zero pointer reference) and cause NSClient++ crash.        |
| CheckDisk          | [check_drivesize](windows/CheckDisk#check_drivesize)              | Check the size (free-space) of a drive or volume.                                   |
| CheckDisk          | [check_files](windows/CheckDisk#check_files)                      | Check various aspects of a file and/or folder.                                      |
| CheckDisk          | [checkdrivesize](windows/CheckDisk#checkdrivesize)                | **Deprecated** please use: :query:`check_drivesize`                                 |
| CheckDisk          | [checkfiles](windows/CheckDisk#checkfiles)                        | **Deprecated** please use: :query:`check_drivesize`                                 |
| CheckDocker        | [check_docker](misc/CheckDocker#check_docker)                     | Checks that a given docker container is running.                                    |
| CheckEventLog      | [check_eventlog](windows/CheckEventLog#check_eventlog)            | Check for errors in the event log.                                                  |
| CheckEventLog      | [checkeventlog](windows/CheckEventLog#checkeventlog)              | **Deprecated** please use: :query:`check_eventlog`                                  |
| CheckHelpers       | [check_always_critical](check/CheckHelpers#check_always_critical) | Run another check and regardless of its return code return CRITICAL.                |
| CheckHelpers       | [check_always_ok](check/CheckHelpers#check_always_ok)             | Run another check and regardless of its return code return OK.                      |
| CheckHelpers       | [check_always_warning](check/CheckHelpers#check_always_warning)   | Run another check and regardless of its return code return WARNING.                 |
| CheckHelpers       | [check_and_forward](check/CheckHelpers#check_and_forward)         | Run a check and forward the result as a passive check.                              |
| CheckHelpers       | [check_critical](check/CheckHelpers#check_critical)               | Just return CRITICAL (anything passed along will be used as a message).             |
| CheckHelpers       | [check_multi](check/CheckHelpers#check_multi)                     | Run more then one check and return the worst state.                                 |
| CheckHelpers       | [check_negate](check/CheckHelpers#check_negate)                   | Run a check and alter the return status codes according to arguments.               |
| CheckHelpers       | [check_ok](check/CheckHelpers#check_ok)                           | Just return OK (anything passed along will be used as a message).                   |
| CheckHelpers       | [check_timeout](check/CheckHelpers#check_timeout)                 | Run a check and timeout after a given amount of time if the check has not returned. |
| CheckHelpers       | [check_version](check/CheckHelpers#check_version)                 | Just return the NSClient++ version.                                                 |
| CheckHelpers       | [check_warning](check/CheckHelpers#check_warning)                 | Just return WARNING (anything passed along will be used as a message).              |
| CheckHelpers       | [filter_perf](check/CheckHelpers#filter_perf)                     | Run a check and filter performance data.                                            |
| CheckHelpers       | [render_perf](check/CheckHelpers#render_perf)                     | Run a check and render the performance data as output message.                      |
| CheckHelpers       | [xform_perf](check/CheckHelpers#xform_perf)                       | Run a check and transform the performance data in various (currently one) way.      |
| CheckLogFile       | [check_logfile](check/CheckLogFile#check_logfile)                 | Check for errors in log file or generic pattern matching in text files.             |
| CheckMKClient      | [check_mk_query](check/CheckMKClient#check_mk_query)              | Request remote information via check_mk.                                            |
| CheckNet           | [check_ping](misc/CheckNet#check_ping)                            | Ping another host and check the result.                                             |
| CheckNSCP          | [check_nscp](check/CheckNSCP#check_nscp)                          | Check the internal health of NSClient++.                                            |
| CheckNSCP          | [check_nscp_version](check/CheckNSCP#check_nscp_version)          | Check the version of NSClient++ which is used.                                      |
| CheckSystem        | [check_cpu](windows/CheckSystem#check_cpu)                        | Check that the load of the CPU(s) are within bounds.                                |
| CheckSystem        | [check_memory](windows/CheckSystem#check_memory)                  | Check free/used memory on the system.                                               |
| CheckSystem        | [check_network](windows/CheckSystem#check_network)                | Check network interface status.                                                     |
| CheckSystem        | [check_os_version](windows/CheckSystem#check_os_version)          | Check the version of the underlying OS.                                             |
| CheckSystem        | [check_pagefile](windows/CheckSystem#check_pagefile)              | Check the size of the system pagefile(s).                                           |
| CheckSystem        | [check_pdh](windows/CheckSystem#check_pdh)                        | Check the value of a performance (PDH) counter on the local or remote system.       |
| CheckSystem        | [check_process](windows/CheckSystem#check_process)                | Check state/metrics of one or more of the processes running on the computer.        |
| CheckSystem        | [check_service](windows/CheckSystem#check_service)                | Check the state of one or more of the computer services.                            |
| CheckSystem        | [check_uptime](windows/CheckSystem#check_uptime)                  | Check time since last server re-boot.                                               |
| CheckSystem        | [checkcounter](windows/CheckSystem#checkcounter)                  | **Deprecated** please use: :query:`check_pdh`                                       |
| CheckSystem        | [checkcpu](windows/CheckSystem#checkcpu)                          | **Deprecated** please use: :query:`check_cpu`                                       |
| CheckSystem        | [checkmem](windows/CheckSystem#checkmem)                          | **Deprecated** please use: :query:`check_mem`                                       |
| CheckSystem        | [checkprocstate](windows/CheckSystem#checkprocstate)              | **Deprecated** please use: :query:`check_process`                                   |
| CheckSystem        | [checkservicestate](windows/CheckSystem#checkservicestate)        | **Deprecated** please use: :query:`check_service`                                   |
| CheckSystem        | [checkuptime](windows/CheckSystem#checkuptime)                    | **Deprecated** please use: :query:`check_uptime`                                    |
| CheckTaskSched     | [check_tasksched](windows/CheckTaskSched#check_tasksched)         | Check status of scheduled jobs.                                                     |
| CheckTaskSched     | [checktasksched](windows/CheckTaskSched#checktasksched)           | **Deprecated** please use: :query:`check_tasksched`                                 |
| CheckWMI           | [check_wmi](windows/CheckWMI#check_wmi)                           | Check a set of WMI values and return rows which are matching criteria.              |
| GraphiteClient     | [submit_graphite](client/GraphiteClient#submit_graphite)          | Submit information to the remote Graphite server.                                   |
| NRDPClient         | [submit_nrdp](client/NRDPClient#submit_nrdp)                      | Submit information to the remote NRDP Server.                                       |
| NRPEClient         | [check_nrpe](client/NRPEClient#check_nrpe)                        | Request remote information via NRPE.                                                |
| NRPEClient         | [exec_nrpe](client/NRPEClient#exec_nrpe)                          | Execute remote script via NRPE. (Most likely you want nrpe_query).                  |
| NRPEClient         | [nrpe_forward](client/NRPEClient#nrpe_forward)                    | Forward the request as-is to remote host via NRPE.                                  |
| NRPEClient         | [nrpe_query](client/NRPEClient#nrpe_query)                        | Request remote information via NRPE.                                                |
| NRPEClient         | [submit_nrpe](client/NRPEClient#submit_nrpe)                      | Submit information to remote host via NRPE. (Most likely you want nrpe_query).      |
| NSCAClient         | [submit_nsca](client/NSCAClient#submit_nsca)                      | Submit information to the remote NSCA server.                                       |
| NSCPClient         | [check_remote_nscp](misc/NSCPClient#check_remote_nscp)            | Request remote information via NSCP.                                                |
| NSCPClient         | [exec_remote_nscp](misc/NSCPClient#exec_remote_nscp)              | Execute remote script via NSCP.                                                     |
| NSCPClient         | [remote_nscp_query](misc/NSCPClient#remote_nscp_query)            | Request remote information via NSCP.                                                |
| NSCPClient         | [remote_nscpforward](misc/NSCPClient#remote_nscpforward)          | Forward the request as-is to remote host via NSCP.                                  |
| NSCPClient         | [submit_remote_nscp](misc/NSCPClient#submit_remote_nscp)          | Submit information to remote host via NSCP.                                         |
| SamplePluginSimple | [sample_raw_command](misc/SamplePluginSimple#sample_raw_command)  | This is a sample hello world command.                                               |
| SimpleCache        | [check_cache](generic/SimpleCache#check_cache)                    | Fetch results from the cache.                                                       |
| SimpleCache        | [list_cache](generic/SimpleCache#list_cache)                      | List all keys in the cache.                                                         |
| SMTPClient         | [submit_smtp](client/SMTPClient#submit_smtp)                      | Submit information to the remote SMTP server.                                       |
| SyslogClient       | [submit_syslog](client/SyslogClient#submit_syslog)                | Submit information to the remote syslog server.                                     |
