### Monitoring event-log

Monitoring the event-log is a single command away in the form of `check_eventlog`.
The default command will monitor the application/system/security logs which usually have the generic errors.

```
check_eventlog
L        cli CRITICAL: CRITICAL: 5 message(s) Application Bonjour Service (Task Scheduling Error: ... e DNS-servrarna svarade.)
L        cli  Performance data: 'problem_count'=5;0;0
```

### Time and date

The default time frame is 24 hours. This can be configured via the scan-range which specifies the number of hours from now to look.
This might seem a bit off at first but all date and time ranges which reflect past times are "negative" in NSClient++.
Thus the default is: `check_eventlog scan-range=-24h`

If you instead wish to check the past week you would change this to: `check_eventlog scan-range=-1w`

The other option you can use for defining time is the filter keyword written.
This can be used much the same but is used if you wish to use the time in the filter so you still need to specify scan-range.
But if you for instance want to get an error if the message is 24 hours old and a warning if it is 1 week you would do:
```
check_eventlog scan-range=-1w "warn=count gt 0" "critical=written > -24h"
```

### Checking for specific messages

There are many ways to find messages but the optimum solution is to filter on log, source (provider), and id.
This is as this combination is guaranteed to be unique and still quick to look for.
Another option it so look for messages which will obviously work as well but it will be magnitudes slower in terms of performance.

To find the source and event id you can easily look at the messages property in the event viewer.
![find event log message](../../images/eventlog-find-event.png)

With this information in hand we can easily create a filter for a specific message like so:

```
check_eventlog "filter=provider = 'Microsoft-Windows-Security-SPP' and id = 903"
```

Sometimes, rarely, the message is important and then it is best to add that as a last check to the above filter as it will be faster.

```
check_eventlog "filter=provider = 'Microsoft-Windows-Security-SPP' and id = 903 and message like 'foo'"
```

### Modern windows (channels)

Since version 0.4.2 NSClient++ has had the ability to check all logs on modern windows machines.
This works out of the box and you specify the path of the channel you want to look for with the file command.
A slight snag here is that the separator for "folders" is - not \ r / as one might expect this is unfortunately a windows flaw most likely related to the fact that event logs can also be read from the file system.

A simple way to find the actual name of an event log channel is to view its properties (right-click the channel and click properties):

![channel properties](../../images/eventlog-channel-names.png)

```
check_eventlog scan-range=-100w show-all filter=none "file=Microsoft-Windows-AAD/Operational"
```

### Checking for non errors

The default filters are filtering out only warnings, errors and critical messages:

| Property | Default value                             |
|----------|-------------------------------------------|
| filter   | level in ('warning', 'error', 'critical') |
| warning  | level = 'warning' or problem_count > 0    |
| critical | level in ('error', 'critical')            |

Thus if you want to find a message which is not warnings, errors and critical messages you need to either change or disable the default filter like so:

```
check_eventlog filter=none
```

### Severity/Level/Error

In the previous event-log API it was common for people to use severity to filter out errors.
This has never worked as severity was never message severity, it remains however, a common thing.
Regardless of version of API and version of NSClient++ the proper way to find errors is to use the level keyword like so:

```
check_eventlog "filter=level = 'error'"
```

While we have never been able to find an official list of the meaning of the level this is how NSClient++ interprets the values:

| Level | Keyword in NSClient++                                   |
|-------|---------------------------------------------------------|
| 1     | critical                                                |
| 2     | error                                                   |
| 3     | warning, warn                                           |
| 4     | informational, info, information, success, auditSuccess |
| 5     | debug, verbose                                          |
| #     | Specify any number for other values                     |

Thus if you run into a non standard level you can check this like so:

```
check_eventlog "filter=level = 42"
```

### Using Real-time monitoring

The benefit of real-time monitoring of the event-log is that it is often significantly faster and more resources efficient.
The drawback is that it is more complex to setup and normally requires passive monitoring (via NSCA/NRDP)

The basic idea is depict in the following figure.

![real-time monitoring](../../images/realtime-monitoring.png)

We have a filter which listens to event log entries.
These entries are (when they matched) turned into messages and statuses which is then sent onward to various channels.
On the other end of these channels are (hopefully) someone who is interested in the message.

In most cases the first channel you are interested in is NSCA which is the default name where the NSCAClient listens.
It will in turn forward all incoming messages on to Nagios via NSCA.

So in short we need to configure three things

*   Activate real time filtering
*   Add a filter which listens for events
*   Setup a destination

#### Enabling real-time filtering

To setup real time filtering we only need a single flag (as well as the eventlog module).

**configuration:**
```
[/modules]
CheckEventLog=enabled

[/settings/eventlog/real-time]
enabled = true
```

Adding this will not do much since we don't have a filter yet but adding one is pretty simple as well so lets go ahead and do that.

To make life simple we set the destination in this filter to "log" which means the information only ends up in the NSClient++ log file.
Not very usefull in reality but very usefull when we are debuggning as it removes possible errors sources.

**configuration:**
```
[/settings/eventlog/real-time/filters/my_alert]
log=application
destination=log
filter=level='error'
maximum age=30s
debug=true
```

Going through the configuration line by line we have:

* `log=application` is the log we listen to.
* `destination=log` is where the message is sent
* `filter=level='error'` means we only want to recieve error messages.
* `maximum age=30s` sets a repeating "ok" messages every 30 seconds.
* `debug=true` will increase the debug level for this filter

If we were to test this (and please do go ahead) we would start getting warning on the console about no one listening to our events.

To be able to test this we need to inject some messages in the eventlog.
This we can do with the eventcreate command.

**Add error to eventlog:**
```
eventcreate /ID 1 /L application /T ERROR /SO MYEVENTSOURCE /D "My first log"
```

**Add info to eventlog:**
```
eventcreate /ID 1 /L application /T INFORMATION /SO MYEVENTSOURCE /D "My first log"
```

If we check the log we should see something similar to this:

![eventlog output](../../images/eventlog-realtime-log.png)

* 1: Always makre sure there are not errors and that the parsed tree looks like you want it. If the filter has syntax issues nothign will work
* 2: This is how it looks when we inject an error message, it is caught and we get the `Notification 0: Application: 1 (error: My first log)` in the log.
* 3: This is the periodical "ok" message we get when there are not errors: `Notification 0: eventlog found no records`
* 4: Here we can see the output when there is a message but it does not match our filter.

#### Enabling cache to check actively

**TODO**
