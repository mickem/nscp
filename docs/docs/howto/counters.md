# Introduction

What are counter? Counters or PDH is short for Performance Data Helper and is an old performance library which was added as an add-on in NT4.
It has since been replaced by WMI but don't count it out since there are still a lot of useful metrics which can only be fetched from PDH.
It is also pretty simple and straight forward to use (which WMI can sometimes not be). The main drawback to counters is that you cannot (easily) correlate different metrics.

## What's all the fuss

Counters is old as I said but more importantly it is badly implemented and has a lot of headaches and pains which will hit most users at some point.
Knowing about them though will help you navigate them and your experience will be greatly enhanced.

## Problems and workarounds

### Counters are localized

This means that a wonderful monitoring set-up in your French data center will not work at all in your German data center.
The work around for this is to use the "English fallback names" which requires 0.4.1.102 or later.

### Counters sometimes get lost

No one knows why this happens but the counter database (which in essence maps names to numbers) can some times get lost.

The work around is to use the lodctr tool to restore them.
### Counters are localized (revisited)

Now you have found this French counter which you just love to check and you have managed to find the command and you enter it in Nagios and voila: You get errors.
This is due to NRPE/check_nt not having proper encoding support.
The work around for this is to configure the NSClient++ encoding to be the same as the unix encoding (probably UTF8).

I don’t know what my counters are called in The workaround here is to use the built-in test client to list all counters

### Counters are "not working"

This can be for any number of reasons above or even other ones regardless the first step is to debug your counters
The "workaround" here is to use the built-in test client to validate your counters

### Some counters always return 0

Rate counters requires you to make two measure to check the rate between them.
The workaround here is to add the option `averages` to the check the value twice (and calculate the averages).

### Some counters does not return the correct value

Many counters are so called "capped" counters and scaled counters.
The workaround here is to use the option `TODO` option to disable capping, scaling and what not.

### Some counters return to little

Counters have different datatype and knowing which to use is up the implementer.
The workaround here is to use the `type` option to set the correct type.

## Built-in test client

NSClient++ has a built in command line client to test and debug counters.
To get help you can run: `nscp sys -- --help`

### Allowed options:

| Option             | Description                                                                                                 |
|--------------------|-------------------------------------------------------------------------------------------------------------|
| -h [ --help ]      | Show help screen                                                                                            |
| --porcelain        | Computer parsable format                                                                                    |
| --computer arg     | The computer to fetch values from                                                                           |
| --user arg         | The username to login with (only meaningful if computer is specified)                                       |
| --password arg     | The password to login with (only meaningful if computer is specified)                                       |
| --lookup-index arg | Lookup a numeric value in the PDH index table                                                               |
| --lookup-name arg  | Lookup a string value in the PDH index table                                                                |
| --expand-path arg  | Expand a counter path contaning wildcards into corresponding objects (for instance --expand-path \System\*) |
| --check            | Check that performance counters are working                                                                 |
| --list arg         | List counters and/or instances                                                                              |
| --validate arg     | List counters and/or instances                                                                              |
| --all              | List/check all counters not configured counter                                                              |
| --no-counters      | Do not recurse and list/validate counters for any matching items                                            |
| --no-instances     | Do not recurse and list/validate instances for any matching items                                           |
| --counter arg      | Specify which counter to work with                                                                          |
| --filter arg       | Specify a filter to match (substring matching)                                                              |


This tool can do:

-   List default/configured counters
-   List other counters
-   Validate the default/configured counters
-   Validate other counters
-   Convert names to indexes and vice versa
-   List instance, and what not for a given counter.

The first thing to do when you run into issues is to validate the default counters:

### Examples

Some examples of using the command line client to diagnose and investigate counters.

#### List configured counters

```
nscp sys -- --list
Listing configured counters
---------------------------
---------------------------
Listed 0 of 0 counters.No counters was found (perhaps you wanted the --all option to make this a global query, the default is so only look in configured counters).
```

in this case there are no configure counters. You can also give the option --all to list ALL counters (somewhat timeconsuming).

#### List all counters

```
nscp sys -- --list --all
Listing configured counters
---------------------------
...
...
\PhysicalDisk(_Total)\Avg. Disk Bytes/Write
\PhysicalDisk(_Total)\% Idle Time
\PhysicalDisk(_Total)\Split IO/Sec
---------------------------
Listed 36352 of 36352 counters.
```

#### List all counters matching a string
```
nscp sys -- --list Disk --all
Listing configured counters
---------------------------
...
\PhysicalDisk(_Total)\Avg. Disk Bytes/Write
\PhysicalDisk(_Total)\% Idle Time
\PhysicalDisk(_Total)\Split IO/Sec
---------------------------
Listed 159 of 36352 counters.
```

#### Validate all disk counters

```
nscp sys -- --validate Disk --all
Listing configured counters
---------------------------
...
\PhysicalDisk(_Total)\Avg. Disk Bytes/Transfer: ok-rate(0)
\PhysicalDisk(_Total)\Avg. Disk Bytes/Read: ok-rate(0)
\PhysicalDisk(_Total)\Avg. Disk Bytes/Write: ok-rate(0)
\PhysicalDisk(_Total)\% Idle Time: ok-rate(0)
\PhysicalDisk(_Total)\Split IO/Sec: ok-rate(0)
---------------------------
Listed 159 of 36352 counters.
```

## Predefined counters

Thus far we have only worked with counters on a need to use bases but there are many other ways to use counters.
One core feature of NSClient++ since the first version was to check CPU load over time. In other words to check CPU load every x seconds and calculate averages.
The way that works in NSClient++ is by predefining a counter.

Predefining counters are don using the configuration file:

```
[/settings/system/windows/counters/foo]
counter=counter=\PhysicalDisk(_Total)\Avg. Disk Write Queue Length
```

Here we define a counter called foo which will periodically check the `\PhysicalDisk(_Total)\Avg. Disk Write Queue Length` counter.

When we check this counter it works just as a regular counter except the name will be foo.

```
check_pdh counter=foo
```

This in it self is not a massive step forward except we can get around any character issues as the configuration file can be saved as UTF8.
But if we extend this counter definition with a collection strategy we get a lot of added new features.

```
[/settings/system/windows/counters/foo]
collection strategy=rrd
counter=\PhysicalDisk(_Total)\Avg. Disk Write Queue Length
```

What now happens is that the values of the counter is pushed to a stack and averages can be calculated.
To utilize this we add the :option:`CheckSystem.check_pdh.time` option to our check to fetch averages over the last x seconds or minutes.

```
check_pdh counter=foo time=30s
```
