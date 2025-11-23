# Getting started with NRPE

The getting started guide has been split into multiple sections to allow easier reading.

* [Getting-started](./index.md)
* [Getting-started with NSClient++](./nsclient.md)
* [Checking with NRPE client](./nrpe.md)
* [Checking with NSCA client](./nsca.md)

## Checking with NRPE client

**Sections:**

* [Insecure version](#insecure-version)
* [Using certificates (still insecure)](#using-certificates-still-insecure)
* [Using client certificates](#using-client-certificates)


### Insecure version

When NRPE was first released the world was a different place and security was not a big concern.
So the first version of NRPE did not have any real authentication or encryption.

> You can still use this but it is not recommended.

To use the insecure version of NRPE you need to enable the insecure mode in NSClient++.
You can do this using the command line:

```commandline
$ nscp nrpe install --allowed-hosts 127.0.0.1 --insecure --arguments=safe
WARNING: Inconsistent insecure option will overwrite verify=peer-cert with none due to --insecure
Enabling NRPE via SSL from: 127.0.0.1 on port 5666
WARNING: NRPE is currently insecure.
SAFE Arguments are allowed.
```

Here we for convenience also allow `safe` arguments. `safe` arguments are arguments which do not contain `|&><'\"\\[]{}` to prevent shell execution.
This is not really useful as `NRPE Server` does not allow remote shell execution but as we do not know how `CheckExternalScripts` are configured it adds some weak security.

As always lets start NSClient++ in debug mode again:

```
nscp test
...
exit
```

And next lets run a check using the `check_nrpe` command line tool.

```commandline
$ check_nrpe -H 127.0.0.1 -2
I (0.4.0 2025-08-30) seem to be doing fine...
```

Here we set `-2` to use the old version of NRPE and `-d 1` to enable insecure ADH key exchange.

### Using certificates (still insecure)

Next up lets make this a bit more secure by using TLS certificates.

```commandline
$ nscp nrpe install --allowed-hosts 127.0.0.1 --insecure=false
Enabling NRPE via SSL from: 127.0.0.1 on port 5666.
WARNING: NRPE is currently insecure.
Arguments are NOT allowed.
```

As you can see it used the default certificate and not our custom certificate.
SO lets change that:

```commandline
$ nscp nrpe install ^
        --allowed-hosts 127.0.0.1 ^
        --insecure=false --verify=none ^
        --certificate ${certificate-path}\server.pem ^
        --certificate-key ${certificate-path}\server.key
Enabling NRPE via SSL from: 127.0.0.1 on port 5666.
WARNING: NRPE is not secure, while we have proper encryption there is no authentication expect for only accepting traffic from 127.0.0.1.
Traffic is encrypted using nrpe_test\server.crt and nrpe_test\server.key.
Arguments are NOT allowed.
```

Now we can restart NSClient++ in debug mode again:

```
nscp test
...
exit
```

And run the check again using the `check_nrpe` command line tool.

```commandline
$ check_nrpe -H 127.0.0.1 --ssl-version TLSv1.2+
I (0.4.0 2025-08-30) seem to be doing fine...
```

### Using client certificates

As you can see there is still no authentication but at least the traffic is encrypted.
To make this a bit better we can use client certificates to authenticate the client.

First we need to enable client certificate authentication in NSClient++.

```commandline
$ nscp nrpe install ^
        --allowed-hosts 127.0.0.1 ^
        --insecure=false --verify=peer-cert ^
        --certificate ${certificate-path}\server.pem ^
        --certificate-key ${certificate-path}\server.key ^
        --ca ${certificate-path}\ca.pem
Enabling NRPE via SSL from: 127.0.0.1 on port 5666.
NRPE is currently reasonably secure and will require client certificates.
The clients need to have a certificate issued from nrpe_test\ca.crt
Traffic is encrypted using nrpe_test\server.crt and nrpe_test\server.key.
Arguments are NOT allowed.
```

Then we also need to get the root certificate from mkcert.

```
mkcert -CAROOT
```

In this path you will find the `rootCA.pem` file which we need to copy to the NSClient++ security folder as `ca.pem`.

```
copy rootCA.pem "c:\program files\nsclient++\security\ca.pem"
```

Now we can restart NSClient++ in debug mode again:

```
nscp test
...
exit
```

Then we need to generate a client certificate using `mkcert`.

```
mkcert -client nagios
```

Next copy the generated files to the Nagios server.
Then we need to configure the `check_nrpe` command to use the client certificate.

```commandline
$ check_nrpe -H 127.0.0.1 -3 --cert nagios-client.pem --key nagios-client-key.pem
I (0.4.0 2025-08-30) seem to be doing fine...
```

Next to verify that the client certificate is required we can try to run the command without the certificate.

```commandline
$ check_nrpe -H 127.0.0.1 -3
CHECK_NRPE: Receive header underflow - only 0 bytes received (4 expected).
```

And that is it we now have a reasonably secure NRPE setup.

If you run into any issues I can recommend validating the connection using `openssl s_client`:

```commandline
openssl s_client -connect 127.0.0.1:5666 -cert nagios-client.pem -key nagios-client-key.pem
```

In general certificates can be a bit tricky to get right.
