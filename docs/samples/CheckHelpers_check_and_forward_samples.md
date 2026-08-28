Given this configuration:

```ini
[/modules]
CheckSystem  = enabled
CheckHelpers = enabled
NSCAClient   = enabled

[/settings/NSCA/client/targets/default]
address    = nsca://nagios-server:5667
password   = secret-password
encryption = aes256
```

**Run a check and send its result as a passive check:**

```
nscp client --boot --query check_and_forward command=check_cpu channel=NSCA "alias=CPU Load"
Message submitted: NSCA
```

The NSCA daemon on the monitoring server logs the result under the service
description given by `alias`, exactly as a scheduled check would deliver it:

```
nsca: SERVICE CHECK -> Host Name: 'win-server-01', Service Description: 'CPU Load',
      Return Code: '0', Output: 'OK: CPU load is ok.|...'
```

**Pass arguments to the wrapped check** — one per `arguments=`:

```
nscp client --boot --query check_and_forward command=check_cpu channel=NSCA "alias=CPU Load" "arguments=warning=load>10"
Message submitted: NSCA
```

The status of the wrapped check is what gets submitted; the `OK` you see here
only says the result was handed to the NSCA client.

**The `-a` / `--argument` spelling works too**, which is what you want from a
batch file or a script:

```
nscp client --boot --query check_and_forward --argument command=check_cpu --argument channel=NSCA --argument "alias=CPU Load"
Message submitted: NSCA
```

**A channel nobody listens on is an error, not a silent drop:**

```
nscp client --boot --query check_and_forward command=check_cpu channel=NOPE
Failed to submit to: NOPE
```

**Over REST**, to push a result on demand from a script:

```
curl -k -u admin:<password> "https://<agent>:8443/api/v2/queries/check_and_forward/commands/execute?command=check_cpu&channel=NSCA&alias=CPU+Load"
{"command":"check_and_forward","result":0,"lines":[{"message":"Message submitted: NSCA","perf":{}}]}
```

#### Trying it without a monitoring server

`SimpleFileWriter` is a channel that writes results to a local file, which makes
it a quick way to see exactly what is being submitted:

```ini
[/modules]
CheckSystem      = enabled
CheckHelpers     = enabled
SimpleFileWriter = enabled

[/settings/writers/file]
file = results.txt
```

```
nscp client --boot --query check_and_forward command=check_cpu channel=FILE "alias=CPU Load"
Message submitted: FILE
```

`results.txt` then contains one line per submitted result, as
`${alias-or-command} ${result} ${message}`:

```
CPU Load OK OK: CPU load is ok.
```

Add an argument for the wrapped check and the forwarded status changes with it —
the check really runs, it is not a fixed OK:

```
nscp client --boot --query check_and_forward command=check_cpu channel=FILE "alias=CPU Load" "arguments=warning=load>-1"
Message submitted: FILE
```

```
CPU Load WARNING WARNING: 5s: 0%, 1m: 0%, 5m: 0%
```

Without `alias` the result is named after the command instead:

```
check_cpu OK OK: CPU load is ok.
```
