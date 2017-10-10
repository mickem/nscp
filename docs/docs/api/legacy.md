# Legacy API

## Query Endpoint

Queries are mostly based on modules. You can test-drive them through
the API web interface and then add your own calls.

You can use any query available by the check modules.

> **Note**
>
> Some query URL endpoints might not be available. This will be logged and is
> available in the web interface `Log` menu.
>
> Ensure to enable the corresponding module and test-drive the query from the
> web interface.

This documentation compiles a list of the most common query URL endpoints
and their usage examples. Explore and add more from known check commands.

### Arguments

Arguments can be passed to the URL as already known from check\_nt/check\_nrpe command calls.

```
curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_cpu?show-all&warning=load>1&critical=load>20' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_cpu",
            "lines": [
                {
                    "message": "WARNING: warning(5m: 2%, 1m: 2%, 5s: 2%)",
                    "perf": [
                        {
                            "alias": "total 5m",
                            "int_value": {
                                "critical": 20,
                                "unit": "%",
                                "value": 2,
                                "warning": 1
                            }
                        },
                        {
                            "alias": "total 1m",
                            "int_value": {
                                "critical": 20,
                                "unit": "%",
                                "value": 2,
                                "warning": 1
                            }
                        },
                        {
                            "alias": "total 5s",
                            "int_value": {
                                "critical": 20,
                                "unit": "%",
                                "value": 2,
                                "warning": 1
                            }
                        }
                    ]
                }
            ],
            "result": "WARNING"
        }
    ]
}
```

### CheckDisk Module

#### /query/check_drivesize

Check the disk drive on Windows.

```
curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_drivesize?drive=C:' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_drivesize",
            "lines": [
                {
                    "message": "OK All 1 drive(s) are ok",
                    "perf": [
                        {
                            "alias": "C: used",
                            "float_value": {
                                "critical": 49.95878562889993,
                                "maximum": 55.509761810302734,
                                "minimum": 0.0,
                                "unit": "GB",
                                "value": 41.377288818359375,
                                "warning": 44.40780944749713
                            }
                        },
                        {
                            "alias": "C: used %",
                            "float_value": {
                                "critical": 90.0,
                                "maximum": 100.0,
                                "minimum": 0.0,
                                "unit": "%",
                                "value": 75.0,
                                "warning": 80.0
                            }
                        }
                    ]
                }
            ],
            "result": "OK"
        }
    ]
}
```

### CheckEventLog

#### /query/check_eventlog

Check for specific Windows event log entries, e.g. in the last 10 hours. Add the message to the final output. Note: You need to URL encode the parameters. `${message}` becomes `%24%7Bmessage%7D`.

```
curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_eventlog?show-all&file=system&detail-syntax=%24%7Bmessage%7D&scan-range=-600m' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_eventlog",
            "lines": [
                {
                    "message": "WARNING: 5 message(s) warning(The network driver detected that its hardware has stopped responding to commands. ...."
                    "perf": [
                        {
                            "alias": "problem_count",
                            "int_value": {
                                "critical": 0,
                                "value": 5,
                                "warning": 0
                            }
                        }
                    ]
                }
            ],
            "result": "WARNING"
        }
    ]
}
```

### CheckHelpers Module

#### /query/check_version

Print the NSClient++ version.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_version' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_version",
            "lines": [
                {
                    "message": "0.5.0.62 2016-09-14"
                }
            ],
            "result": "OK"
        }
    ]
}
```


### CheckNSCP Module

#### /query/check_nscp

Retrieve the current status and possible errors.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_nscp' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_nscp",
            "lines": [
                {
                    "message": "0 crash(es), 5 error(s), last error: Failed to execute command on log, uptime 01:10:40.901063"
                }
            ],
            "result": "CRITICAL"
        }
    ]
}
```

### CheckSystem Module

#### /query/check_cpu

Check for CPU usage on Windows.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_cpu?show-all&warning=load>1&critical=load>20' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_cpu",
            "lines": [
                {
                    "message": "WARNING: warning(5m: 3%, 1m: 2%, 5s: 2%)",
                    "perf": [
                        {
                            "alias": "total 5m",
                            "int_value": {
                                "critical": 20,
                                "unit": "%",
                                "value": 3,
                                "warning": 1
                            }
                        },
                        {
                            "alias": "total 1m",
                            "int_value": {
                                "critical": 20,
                                "unit": "%",
                                "value": 2,
                                "warning": 1
                            }
                        },
                        {
                            "alias": "total 5s",
                            "int_value": {
                                "critical": 20,
                                "unit": "%",
                                "value": 2,
                                "warning": 1
                            }
                        }
                    ]
                }
            ],
            "result": "WARNING"
        }
    ]
}
```

#### /query/memory

Check memory usage on Windows.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_memory' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_memory",
            "lines": [
                {
                    "message": "WARNING: committed = 2.849GB, physical = 1.669GB",
                    "perf": [
                        {
                            "alias": "committed",
                            "float_value": {
                                "critical": 3.1644229888916016,
                                "maximum": 3.5160255432128906,
                                "minimum": 0.0,
                                "unit": "GB",
                                "value": 2.8489990234375,
                                "warning": 2.8128204345703125
                            }
                        },
                        {
                            "alias": "committed %",
                            "float_value": {
                                "critical": 90.0,
                                "maximum": 100.0,
                                "minimum": 0.0,
                                "unit": "%",
                                "value": 81.0,
                                "warning": 80.0
                            }
                        },
                        {
                            "alias": "physical",
                            "float_value": {
                                "critical": 1.799591445364058,
                                "maximum": 1.9995460510253906,
                                "minimum": 0.0,
                                "unit": "GB",
                                "value": 1.6694755554199219,
                                "warning": 1.599636840634048
                            }
                        },
                        {
                            "alias": "physical %",
                            "float_value": {
                                "critical": 90.0,
                                "maximum": 100.0,
                                "minimum": 0.0,
                                "unit": "%",
                                "value": 83.0,
                                "warning": 80.0
                            }
                        }
                    ]
                }
            ],
            "result": "WARNING"
        }
    ]
}
```

#### /query/check_network

Check network usage.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_network' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_network",
            "lines": [
                {
                    "message": ": VirtualBox Host-Only Ethernet Adapter #2 >790273982464 <790273982464 bps, VirtualBox Host-Only Ethernet Adapter >790273982464 <790273982464 bps, Intel(R) PRO/1000 MT Network Connection >790273982464 <790273982464 bps",
                    "perf": [
                        {
                            "alias": "Intel(R) PRO/1000 MT Network Connection_total",
                            "int_value": {
                                "critical": 100000,
                                "value": 790273982464,
                                "warning": 10000
                            }
                        }
                    ]
                }
            ],
            "result": "CRITICAL"
        }
    ]
}
```

#### /query/check_os_version

Retrieve the Operating System version.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_os_version' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_os_version",
            "lines": [
                {
                    "message": "OK: Windows 10 (10.0.10586)",
                    "perf": [
                        {
                            "alias": "version",
                            "int_value": {
                                "critical": 50,
                                "value": 100,
                                "warning": 50
                            }
                        }
                    ]
                }
            ],
            "result": "OK"
        }
    ]
}
```

#### /query/check_pdh

Check a specific performance counter. Note: `counter=\Processor(_total)\% Processor Time`
has been URL encoded to `counter%3D%5CProcessor%28_total%29%5C%25%20Processor%20Time`.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_pdh?counter%3D%5CProcessor%28_total%29%5C%25%20Processor%20Time' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_pdh",
            "lines": [
                {
                    "message": "OK: \\Processor(_total)\\% Processor Time = 49",
                    "perf": [
                        {
                            "alias": "\\Processor(_total)\\% Processor Time_value",
                            "float_value": {
                                "critical": 0.0,
                                "value": 49.0,
                                "warning": 0.0
                            }
                        }
                    ]
                }
            ],
            "result": "OK"
        }
    ]
}
```

#### /query/check_service

Check if a specific service is started. `wscsvc` checks the Windows Security Center service.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_service?service=wscsvc' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_service",
            "lines": [
                {
                    "message": "OK: All 1 service(s) are ok.",
                    "perf": [
                        {
                            "alias": "wscsvc",
                            "int_value": {
                                "critical": 0,
                                "value": 4,
                                "warning": 0
                            }
                        }
                    ]
                }
            ],
            "result": "OK"
        }
    ]
}
```


#### /query/check_uptime


Check the host's uptime.

```
$ curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/query/check_uptime' | python -m json.tool
{
    "header": {
        "source_id": ""
    },
    "payload": [
        {
            "command": "check_uptime",
            "lines": [
                {
                    "message": "CRITICAL: uptime: 10:6h, boot: 2017-06-14 10:56:38 (UTC)",
                    "perf": [
                        {
                            "alias": "uptime",
                            "int_value": {
                                "critical": 86400,
                                "unit": "s",
                                "value": 36370,
                                "warning": 172800
                            }
                        }
                    ]
                }
            ],
            "result": "CRITICAL"
        }
    ]
}
```



## Settings Endpoint

### /settings/status

Determine whether settings need to be stored.

```
curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/settings/status' | python -m json.tool
{
    "header": {},
    "payload": [
        {
            "result": {
                "code": "STATUS_OK"
            },
            "status": {
                "context": "ini://${shared-path}/nsclient.ini",
                "has_changed": false,
                "type": "ini"
            }
        }
    ]
}
```

## Core Endpoint

### /core/reload

Reload the NSClient++ service.

```
curl -k -s -H 'password: icinga' 'https://nsclient1.localdomain:8443/core/reload'
```
