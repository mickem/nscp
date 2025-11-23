# Getting started with NSCA

The getting started guide has been split into multiple sections to allow easier reading.

* [Getting-started](./index.md)
* [Getting-started with NSClient++](./nsclient.md)
* [Checking with NRPE client](./nrpe.md)
* [Checking with NSCA client](./nsca.md)

**Sections:**

* [Ciphers and encryption](#ciphers-and-encryption)
* [Password](#password)
* [Configure NSCA server](#configure-nsca-server)
* [Verifying connection](#verifying-connection)
* [Loading modules](#loading-modules)
* [Add scheduled jobs](#add-scheduled-jobs)
* [Configure NSCA client](#configure-nsca-client)
* [Restart NSClient++](#restart-nsclient)
* [Further configuration](#further-configuration)

NSCA is a protocol used to send passive check results to Nagios.
Passive means that the monitoring server does not actively query the client for information but instead the client sends the information to the server.
This is useful in situations where the client is behind a firewall or NAT and cannot be reached by the server.

It can also be used to offload work from the monitoring server to the client.

## Ciphers and encryption

Both NSClient++ and NSCA supports multiple encryption methods and ciphers.
As they use different libraries though not all are compatible.
Here is a list of supported encryption methods:

| NSClient++ (Crypto++) | NSCA (libmcrypto) | Security               |
|-----------------------|-------------------|------------------------|
| ⚠️ none               | ️0                | ️Not secure            |
| ⚠️ xor                | ️1                | ️Not secure            |
| ⚠️ des                | ️2                | ️Insecure              |
| ⚠️ 3des               | ️3                | ️️Legacy               |
| 🟡 cast128            | 4                 | Moderate security      |
| 🟡 xtea               | 6                 | Moderate security      |
| 🟡 blowfish           | 8                 | Moderate security      |
| 🟢 twofish            | 9                 | Very secure            |
| ⚠️ rc2                | ️11               | ️Insecure              |
| ✅ aes256              | 14                | Industry standard (1)  |
| 🟢 serpent            | 20                | Paranoid Security      |
| ⚠️ gost               | ️23               | ️Questionable security |

> (1) Please note that NSCA specify block-size and NSClient++ specify key-size.
> This means `RIJNDAEL-128` (14) is the same as `AES-256` in NSClient++.
> AES is only allowed with 128 bit block size so the other NSCA options are not valid.

## Password

Both NSClient++ and NSCA uses a shared secret password to authenticate the client and server.
This is not a security best practice but is how NSCA works.
Make sure to use a strong password to prevent brute-force attacks.


##  Configure NSCA server

The best way to verify the connection is to use the nsca client in NSClient++.
So lets start by configuring the NSCA server and in this example we will use AES256 and a password `secret-password`.
Here are relevant configuration for the NSCA server:

```text
decryption_method = 14
password          = secret-password
```

If you are having issues it might be useful to change the result file to be able to see what is generated: 
```text
command_file = /tmo/result.txt
```

## Verifying connection

To verify the connection we can use the `nsca` client in NSClient++.
This is a command line tool so we do not need to configure anything in NSClient++ for now.

```commandline
$ nscp nsca --host 127.0.0.1 ^
    --password secret-password --encryption aes256 ^
    --command "service check" --result 2 --hostname "my host name" --message "Hello From command line"
Submission successful
```

Where:

* `--host` is the address of the NSCA server (Nagios server)
* `--password` is the shared secret password
* `--encryption` is the encryption method to use
* `--command` is the name of the service check to send
* `--result` is the result code (0=OK, 1=WARNING, 2=CRITICAL, 3=UNKNOWN)
* `--hostname` is the name of the host to send the check for (i.e. the host as known in Nagios)
* `--message` is the message to (text part of the result)

If you have redirected the `command_file` you should be able to see (and if not the host/check should have been updated in Nagios:

```text
[1763898484] PROCESS_SERVICE_CHECK_RESULT;my host name;service check;2;Hello From command line
```

## Loading modules

Now that we know the submissions work and we have established a working connection we can move on to configuring NSClient++ to send passive checks using NSCA.

The first thing we need to configure is the modules we need to load.
As these checks will no longer be executed by nagios we need to execute them locally instead.
To do this we need in addition to the various check modules also use the `Scheduler` module to schedule the checks.
Here is an example of the modules we need to load:

```ini
[/modules]
CheckSystem=enabled
CheckDisk=enabled
Scheduler=enabled
NSCAClient=enabled
```

## Add scheduled jobs

Now we can move on to schedule the actual check commands.
There are a couple of ways to do this but the easiest way is to use a default template for parameters and only override the command.
To do this we first add the default options to the default template:

```ini
[/settings/scheduler/schedules/default]
channel=NSCA
interval=30s
report=all
```

Here:

* `channel` specifies the channel to use for sending the results (NSCA in this case)
* `interval` specifies how often to run the check
* `report` specifies what results to report (all in this case)

Next we can add various check commands like so:

```ini
[/settings/scheduler/schedules]
host_check=check_ok
cpu=check_cpu
mem=check_memory
check_disk=check_drivesize drive=C: warning=80% critical=90%
```

The reason this work is that the default template is applied to all scheduled jobs unless overridden.
You can create sections for each command if you want more fine grained control.

## Configure NSCA client

Finally we need to configure the NSCA client to be able to connect to the NSCA server.
Here we will again use a default template for the connection parameters:

```ini
[/settings/NSCA/client/targets/default]
address = localhost
encryption = aes256
password = secret-password
```

## Restart NSClient++

Next restart NSClient++ and ensure the passive checks are populated in your monitoring system.

## Further configuration

A common problem is that your windows machines is not named the same way in your monitoring system as it is in NSClient++.
To fix this you can set the hostname to use in the NSCA client configuration like so:
```ini
[/settings/NSCA/client]
hostname = win_${host_lc}.${domain_lc}
```

Here you can use parameters to automatically set the hostname.
The following keywords are supported:

* `auto` - Automatically detect the hostname
* `${host}` - The current hostname
* `${host_lc}` - The current hostname in lowercase
* `${host_uc}` - The current hostname in uppercase
* `${domain}` - The current domainname
* `${domain_lc}` - The current domainname in lowercase
* `${domain_uc}` - The current domainname in uppercase

These can be combined to form the desired hostname like `${host_lc}.${domain_lc}.local`.

This will ensure that the correct hostname is used when sending passive check results.
