

# Modules

| Type    | Module                                                | Description                                                                                                                                                                                        |
|---------|-------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| windows | [CheckDisk](windows/CheckDisk.md)                     | CheckDisk can check various file and disk related things.                                                                                                                                          |
| misc    | [CheckDocker](misc/CheckDocker.md)                    | Use this module to detect and monitor docker containers                                                                                                                                            |
| windows | [CheckEventLog](windows/CheckEventLog.md)             | Check for errors and warnings in the event log.                                                                                                                                                    |
| check   | [CheckExternalScripts](check/CheckExternalScripts.md) | Module used to execute external scripts                                                                                                                                                            |
| check   | [CheckHelpers](check/CheckHelpers.md)                 | Various helper function to extend other checks.                                                                                                                                                    |
| check   | [CheckLogFile](check/CheckLogFile.md)                 | File for checking log files and various other forms of updating text files                                                                                                                         |
| check   | [CheckMKClient](check/CheckMKClient.md)               | check_mk client can be used both from command line and from queries to check remote systems via check_mk                                                                                           |
| check   | [CheckMKServer](check/CheckMKServer.md)               | A server that listens for incoming check_mk connection and processes incoming requests.                                                                                                            |
| misc    | [CheckNet](misc/CheckNet.md)                          | Network related check such as check_ping.                                                                                                                                                          |
| check   | [CheckNSCP](check/CheckNSCP.md)                       | Use this module to check the health and status of NSClient++ it self                                                                                                                               |
| windows | [CheckSystem](windows/CheckSystem.md)                 | Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.                                                                                       |
| windows | [CheckTaskSched](windows/CheckTaskSched.md)           | Check status of your scheduled jobs.                                                                                                                                                               |
| windows | [CheckWMI](windows/CheckWMI.md)                       | Check status via WMI                                                                                                                                                                               |
| misc    | [CollectdClient](misc/CollectdClient.md)              | CollectD client can be used to submit metrics to a collectd server                                                                                                                                 |
| generic | [CommandClient](generic/CommandClient.md)             | A command line client, generally not used except with "nscp test".                                                                                                                                 |
| misc    | [ElasticClient](misc/ElasticClient.md)                | Elastic sends metrics, events and logs to elastic search                                                                                                                                           |
| client  | [GraphiteClient](client/GraphiteClient.md)            | Graphite client can be used to submit graph data to a graphite graphing system                                                                                                                     |
| generic | [LUAScript](generic/LUAScript.md)                     | Loads and processes internal Lua scripts                                                                                                                                                           |
| client  | [NRDPClient](client/NRDPClient.md)                    | NRDP client can be used both from command line and from queries to check remote systems via NRDP                                                                                                   |
| client  | [NRPEClient](client/NRPEClient.md)                    | NRPE client can be used both from command line and from queries to check remote systems via NRPE as well as configure the NRPE server                                                              |
| client  | [NRPEServer](client/NRPEServer.md)                    | A server that listens for incoming NRPE connection and processes incoming requests.                                                                                                                |
| client  | [NSCAClient](client/NSCAClient.md)                    | NSCA client can be used both from command line and from queries to submit passive checks via NSCA                                                                                                  |
| client  | [NSCAServer](client/NSCAServer.md)                    | A server that listens for incoming NSCA connection and processes incoming requests.                                                                                                                |
| windows | [NSClientServer](windows/NSClientServer.md)           | A server that listens for incoming check_nt connection and processes incoming requests.                                                                                                            |
| misc    | [NSCPClient](misc/NSCPClient.md)                      | NSCP client can be used both from command line and from queries to check remote systems via NSCP (REST)                                                                                            |
| misc    | [Op5Client](misc/Op5Client.md)                        | Client for connecting nativly to the Op5 Nortbound API                                                                                                                                             |
| generic | [PythonScript](generic/PythonScript.md)               | Loads and processes internal Python scripts                                                                                                                                                        |
| misc    | [SamplePluginSimple](misc/SamplePluginSimple.md)      | This is a sample plugin used to demonstrate how to make plugins                                                                                                                                    |
| generic | [Scheduler](generic/Scheduler.md)                     | Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA                                                                                      |
| generic | [SimpleCache](generic/SimpleCache.md)                 | Stores status updates and allows for active checks to retrieve them                                                                                                                                |
| generic | [SimpleFileWriter](generic/SimpleFileWriter.md)       | Write status updates to a text file (A bit like the NSCA server does)                                                                                                                              |
| client  | [SMTPClient](client/SMTPClient.md)                    | SMTP client can be used both from command line and from queries to check remote systems via SMTP                                                                                                   |
| client  | [SyslogClient](client/SyslogClient.md)                | Forward information as syslog messages to a syslog server                                                                                                                                          |
| generic | [WEBServer](generic/WEBServer.md)                     | A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module. |


# Queries
| Module             | Command                                                              | Description                                                                         |
|--------------------|----------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| CheckDisk          | [check_drivesize](windows/CheckDisk.md#check_drivesize)              | Check the size (free-space) of a drive or volume.                                   |
| CheckDisk          | [check_files](windows/CheckDisk.md#check_files)                      | Check various aspects of a file and/or folder.                                      |
| CheckDocker        | [check_docker](misc/CheckDocker.md#check_docker)                     | Checks that a given docker container is running.                                    |
| CheckEventLog      | [check_eventlog](windows/CheckEventLog.md#check_eventlog)            | Check for errors in the event log.                                                  |
| CheckHelpers       | [check_always_critical](check/CheckHelpers.md#check_always_critical) | Run another check and regardless of its return code return CRITICAL.                |
| CheckHelpers       | [check_always_ok](check/CheckHelpers.md#check_always_ok)             | Run another check and regardless of its return code return OK.                      |
| CheckHelpers       | [check_always_warning](check/CheckHelpers.md#check_always_warning)   | Run another check and regardless of its return code return WARNING.                 |
| CheckHelpers       | [check_and_forward](check/CheckHelpers.md#check_and_forward)         | Run a check and forward the result as a passive check.                              |
| CheckHelpers       | [check_critical](check/CheckHelpers.md#check_critical)               | Just return CRITICAL (anything passed along will be used as a message).             |
| CheckHelpers       | [check_multi](check/CheckHelpers.md#check_multi)                     | Run more then one check and return the worst state.                                 |
| CheckHelpers       | [check_negate](check/CheckHelpers.md#check_negate)                   | Run a check and alter the return status codes according to arguments.               |
| CheckHelpers       | [check_ok](check/CheckHelpers.md#check_ok)                           | Just return OK (anything passed along will be used as a message).                   |
| CheckHelpers       | [check_timeout](check/CheckHelpers.md#check_timeout)                 | Run a check and timeout after a given amount of time if the check has not returned. |
| CheckHelpers       | [check_version](check/CheckHelpers.md#check_version)                 | Just return the NSClient++ version.                                                 |
| CheckHelpers       | [check_warning](check/CheckHelpers.md#check_warning)                 | Just return WARNING (anything passed along will be used as a message).              |
| CheckHelpers       | [filter_perf](check/CheckHelpers.md#filter_perf)                     | Run a check and filter performance data.                                            |
| CheckHelpers       | [render_perf](check/CheckHelpers.md#render_perf)                     | Run a check and render the performance data as output message.                      |
| CheckHelpers       | [xform_perf](check/CheckHelpers.md#xform_perf)                       | Run a check and transform the performance data in various (currently one) way.      |
| CheckLogFile       | [check_logfile](check/CheckLogFile.md#check_logfile)                 | Check for errors in log file or generic pattern matching in text files.             |
| CheckMKClient      | [check_mk_query](check/CheckMKClient.md#check_mk_query)              | Request remote information via check_mk.                                            |
| CheckNet           | [check_ping](misc/CheckNet.md#check_ping)                            | Ping another host and check the result.                                             |
| CheckNSCP          | [check_nscp](check/CheckNSCP.md#check_nscp)                          | Check the internal health of NSClient++.                                            |
| CheckNSCP          | [check_nscp_version](check/CheckNSCP.md#check_nscp_version)          | Check the version of NSClient++ which is used.                                      |
| CheckSystem        | [check_cpu](windows/CheckSystem.md#check_cpu)                        | Check that the load of the CPU(s) are within bounds.                                |
| CheckSystem        | [check_memory](windows/CheckSystem.md#check_memory)                  | Check free/used memory on the system.                                               |
| CheckSystem        | [check_network](windows/CheckSystem.md#check_network)                | Check network interface status.                                                     |
| CheckSystem        | [check_os_version](windows/CheckSystem.md#check_os_version)          | Check the version of the underlying OS.                                             |
| CheckSystem        | [check_pagefile](windows/CheckSystem.md#check_pagefile)              | Check the size of the system pagefile(s).                                           |
| CheckSystem        | [check_pdh](windows/CheckSystem.md#check_pdh)                        | Check the value of a performance (PDH) counter on the local or remote system.       |
| CheckSystem        | [check_process](windows/CheckSystem.md#check_process)                | Check state/metrics of one or more of the processes running on the computer.        |
| CheckSystem        | [check_service](windows/CheckSystem.md#check_service)                | Check the state of one or more of the computer services.                            |
| CheckSystem        | [check_uptime](windows/CheckSystem.md#check_uptime)                  | Check time since last server re-boot.                                               |
| CheckTaskSched     | [check_tasksched](windows/CheckTaskSched.md#check_tasksched)         | Check status of scheduled jobs.                                                     |
| CheckWMI           | [check_wmi](windows/CheckWMI.md#check_wmi)                           | Check a set of WMI values and return rows which are matching criteria.              |
| GraphiteClient     | [submit_graphite](client/GraphiteClient.md#submit_graphite)          | Submit information to the remote Graphite server.                                   |
| NRDPClient         | [submit_nrdp](client/NRDPClient.md#submit_nrdp)                      | Submit information to the remote NRDP Server.                                       |
| NRPEClient         | [check_nrpe](client/NRPEClient.md#check_nrpe)                        | Request remote information via NRPE.                                                |
| NRPEClient         | [exec_nrpe](client/NRPEClient.md#exec_nrpe)                          | Execute remote script via NRPE. (Most likely you want nrpe_query).                  |
| NRPEClient         | [nrpe_forward](client/NRPEClient.md#nrpe_forward)                    | Forward the request as-is to remote host via NRPE.                                  |
| NRPEClient         | [nrpe_query](client/NRPEClient.md#nrpe_query)                        | Request remote information via NRPE.                                                |
| NRPEClient         | [submit_nrpe](client/NRPEClient.md#submit_nrpe)                      | Submit information to remote host via NRPE. (Most likely you want nrpe_query).      |
| NSCAClient         | [submit_nsca](client/NSCAClient.md#submit_nsca)                      | Submit information to the remote NSCA server.                                       |
| NSCPClient         | [check_remote_nscp](misc/NSCPClient.md#check_remote_nscp)            | Request remote information via NSCP.                                                |
| NSCPClient         | [exec_remote_nscp](misc/NSCPClient.md#exec_remote_nscp)              | Execute remote script via NSCP.                                                     |
| NSCPClient         | [remote_nscp_query](misc/NSCPClient.md#remote_nscp_query)            | Request remote information via NSCP.                                                |
| NSCPClient         | [remote_nscpforward](misc/NSCPClient.md#remote_nscpforward)          | Forward the request as-is to remote host via NSCP.                                  |
| NSCPClient         | [submit_remote_nscp](misc/NSCPClient.md#submit_remote_nscp)          | Submit information to remote host via NSCP.                                         |
| SamplePluginSimple | [sample_raw_command](misc/SamplePluginSimple.md#sample_raw_command)  | This is a sample hello world command.                                               |
| SimpleCache        | [check_cache](generic/SimpleCache.md#check_cache)                    | Fetch results from the cache.                                                       |
| SimpleCache        | [list_cache](generic/SimpleCache.md#list_cache)                      | List all keys in the cache.                                                         |
| SMTPClient         | [submit_smtp](client/SMTPClient.md#submit_smtp)                      | Submit information to the remote SMTP server.                                       |
| SyslogClient       | [submit_syslog](client/SyslogClient.md#submit_syslog)                | Submit information to the remote syslog server.                                     |
