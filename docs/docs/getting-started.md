# Getting started with NSClient++

This is a hands-on guide to getting started with NSClient++.
We will start with a clean slate and work our way in small easy to follow steps towards a fully functional monitoring solution.


## Table of contents

* [Installing NSClient++](#installing-nsclient)
* [Starting NSClient++ in debug mode](#starting-nsclient-in-debug-mode)
* [Configure NSClient++ from command line](#configure-nsclient-from-command-line)
* [Accessing the Web Interface](#accessing-the-web-interface)
* [Checking things from the Web Interface](#checking-things-from-the-web-interface)
* [Loading modules via Web Interface](#loading-modules-via-web-interface)
* [Configuration via Web Interface](#configuration-via-web-interface)
* [Changing settings via command line](#changing-settings-via-command-line)
* [Adding certificates to NSClient++](#adding-certificates-to-nsclient)
* [Checking with NRPE client](#checking-with-nrpe-client)
* [TODO: Using the query language](#todo-using-the-query-language)
* [TODO: Checking with REST client](#todo-checking-with-rest-client)
* [TODO: Checking with NRDP](#todo-checking-with-nrdp)

## Installing NSClient++

The first thing we need to do is to download the latest version of NSClient++.
You can find the latest version on the [releases page](https://github.com/mickem/nscp/releases).

Download `NSCP-VERSION-x64.msi` and launch it.
This will start the installer which will guide you through the installation process.

### Select monitoring tool

The first thing you will be asked is which monitoring tool you are using.
This will affect the default configuration of NSClient++.

Currently, the following monitoring tools are supported:
* Generic: Select this for any other monitoring solution.
* Op5 Monitor: Select this if you are using Op5 Monitor.

![installer select tool](images/installer-select-tool.png)

### Select Configuration

If you select `custom install` you will get the options to configure NSClient++ (if not you can skip ahead).
The next step is to select the configuration you want to use.

The default is fine it means you will use a configuration file in the ini format stored in the NSClient++ folder.
![installer select config](images/installer-select-config.png)

Options here would be to use a remote configuration file for instance hosted on a web server or place the configuration in the registry.

### Select configuration

Next we get to pick some basic configuration options.

![installer configuration](images/installer-configuration.png)

> This is important so make sure you read and understand the options carefully.

* `Allowed hosts`: This is a list of IP addresses which are allowed to connect to NSClient++.
* `Password`: This is the password used to authenticate against NSClient++. Make sure you note down the default generated password as you will need it later.
* `Enable common checks`: This will enable some common checks such as CPU, memory, disk space, etc.
* `Enable nsclient server`: This is an old and outdated protocol and not recommended to use.
* `Enable NRPE server`: This will enable the NRPE server which allows the monitoring server to execute commands on this machine.
  * `Insecure`: This will allow any host (in allowed hosts) to connect to the NRPE server without authentication. **Not recommended**.
  * `Secure`: This will require the monitoring server to authenticate with a certificates.
* `Enable NSCA client`: This will enable the NSCA client which allows NSClient++ to submit passive checks to a remote server.
* `Enable Web server`: This will enable the web server which allows you to access NSClient++ via a web interface.

In general, I would recommend the default options here if you are using Nagios as they will give you a good starting point.
But beware that this requires manual configuration if using NRPE as certificate needs to be generated and configured manually.

### Automated installation

There is a dedicated page about automating the installation of NSClient++.
See [Reference: Installing](./installing.md).

### Using NSClient++ as a service

After you install NSClient++ you will have a started service running on the machine.
This service is called `nscp` and it will start automatically when the machine boots.
You can control the service using the `net` command or the `sc` command.
```
net start nscp
net stop nscp
sc start nscp
sc stop nscp
```
You can also control the service using the Windows Services management console (services.msc).

## Starting NSClient++ in debug mode

Using NSClient++ as a service is not very useful for debugging and testing.
So it is often useful to run NSClient++ in "debug mode" which allows you to see what is happening in real-time and interact with it.
For the reminder of this guide we will run NSClient++ in debug mode.

To do this we first need to shut down the service.
```
net stop nscp
```

Since much of what NSClient++ does requires "elevated privileges" you should always run NSClient++ in an "Administrator command prompt".
Now lest start NSClient++ in debug mode.

```
cd c:\program files\nsclient++
nscp test
...
exit
```

After starting in debug mode you can type exit to stop NSClient++.

## Configure NSClient++ from command line

NSClient++ has a command line interface to make configuration changes.

To try this out lets enable the WMI module which is a module that allows you to run WMI queries on the local machine.

```
nscp settings --activate-module CheckWMI
```

If you check `nsclient.ini` you will see that the module has been added to the configuration file.

```ini
[/modules]
; ...
CheckWMI = enabled
```

There are many more command line options to interact with the configuration for a full list you can run:

```
nscp settings --help
```

## Accessing the Web Interface

When we installed NSClient++ we enabled the web server.
This allows us to access NSClient++ via a web interface.

As we have stopped the service we need to either start it again or run NSClient++ in debug mode:
```
nscp test
```

Next up to access the web interface you can open a web browser and navigate to `https://localhost:8443/`.
Then you are met with a scary looking dialog (in your language) about an untrusted certificate:
![untrusted certificates](images/web-untrusted-certificates.png)

This is normal and due to the fact that to use TLS (HTTPS) NSClient++ generates a self-signed certificate on startup.
If you have a CA in your organization you can use that to sign trusted certificates or you can click `Advanced` and then `Accept the risk and continue` to proceed to the web interface.

Next up we need to login:

![web login](images/web-login.png)

Here you can login with the username `admin` and the password you set during installation.
If you do not remember the password you can reset it using the command line:

```
nscp settings --path /settings/default --key password --set your_password
```
Once you have logged in you will be presented with the NSClient++ web interface.

![Welcome](images/web-welcome.png)

> If you fail to log in, ensure you are not running a service in the background, or that you have restarted since changing the password.


## Checking things from the Web Interface

The next step is to start checking things.
NSClient++ is a monitoring agent and as such it is designed to check various aspects of your system.
This is done using modules which provide various checks and commands.
Modules can be loaded and unloaded at runtime and they provide various features and functionality.

If we click on `Queries` in the web interface we will see a list of available queries.
In the list you will find `check_cpu` so lets try it out.

![select check_cpu](images/web-select-check_cpu.png)

Then you are met with a screen which looks a bit like this:
![check_cpu](images/web-check_cpu.png)

Here we can:

* Click `Execute` to run the check.
* Click `Get Help` to get help on how to use the check.
* Enter `Arguments` to pass arguments to the check.

Let start by click `Execute` and see what happens.

![check_cpu result](images/web-check_cpu-result.png)

If you click the `Expand` chevron you will also see the performance data from the check.

Next up lets click `Get Help` to see how to use the check.
At the very end you can find the `cores` options, so lets try that out.

Enter `cores`in the arguments field and click `Execute` again.
![check_cpu cores](images/web-check_cpu-cores.png)

And there you have it the CPU load for each core.


## Loading modules via Web Interface

Before we loaded a module using the command line.
Now we will load a module using the web interface.
To do this we will click on `Modules` in the web interface.
Here you will see a list of available modules.
Click the `CheckNet` module to configure that module.

![modules](images/web-modules.png)

Here we can see that the module is neither loaded nor enabled.

![modules check_net](images/web-modules-check_net.png)

A quick word about the difference between loaded and enabled.

* Loaded means that the module is loaded into memory and can be used.
* Enabled means that the module is configured to be loaded when NSClient++ starts.

Normally you want the module to be both loaded and enabled.
So lets click the `Load & Enable` button to load the module.

![modules check_net loaded](images/web-modules-check_net-loaded.png)

Now we can see the queries provided by the module.
Lets try out the `check_ping` query which checks the ping response time to a host.

As you noticed this is the same dialog as we saw before when we executed the `check_cpu` query.
So lets try it out by entering `host=nsclient.org` in the `Arguments` field and clicking `Execute`.

![check_ping](images/web-check_ping.png)

## Configuration via Web Interface

The web interface also allows you to configure NSClient++.
To do this you can use the `Settings` tab but a simpler way is to use the settings widget on the module dialog as that only have settings relevant for a given module so that is what we will do.
So lets click `Modules` in the web interface and then click on the `WEBServer` module.

![modules webserver](images/web-modules-webserver.png).

Once you are here Click `Settings` and then expand `/settings/WEB/server` to show the port setting.

![webserver settings](images/web-webserver-settings.png)

You should now be able to change the `port` to `1234` and click save:

![webserver settings port](images/web-webserver-settings-port.png)

After this you should get a popup asking you to save and update the settings.

![webserver settings save](images/web-webserver-settings-save.png)

If you click `Save and reload` the service will restart and the web server will now be served on port 1234 instead.
So navigate to `http://localhost:1234/` and you should see the web interface again.

## Changing settings via command line

Lets use the command line to change the port back to 8443.

First we need to exit the debug mode if you are running it.
```
...
exit
```

After this we can run the following command to change the port:

```
nscp settings --path /settings/WEB/server --key port --set 8443
```

And start NSClient++ in debug mode again:

```
nscp test
...
exit
```

And you should now be able to access the web interface again at `https://localhost:8443/`.

## Adding certificates to NSClient++

By default, NSClient++ generates a self-signed certificate on startup.
This is fine for testing but in a production environment you will want to use a certificate signed by a trusted CA.
As we do not have a CA in this example we will create our own CA using the `mkcert` tool to simulate CA here.
First we need to install `mkcert`.
You can find the installation instructions on the [mkcert GitHub page](https://github.com/FiloSottile/mkcert).
Once you have installed `mkcert` you can create a CA and generate a certificate for localhost.

```
mkcert -install
mkcert localhost
```

> **NOTICE** This will install the root certificate on your local machine.

This will install the root certificate and generate two files `localhost.pem` and `localhost-key.pem`.
Next we need to copy these files to the NSClient++ security folder.

```
copy localhost.pem "c:\program files\nsclient++\security\server.pem"
copy localhost-key.pem "c:\program files\nsclient++\security\server.key"
```
Next we need to configure NSClient++ to use these files.
We can do this using the command line:

```
nscp settings --path /settings/WEB/server --key certificate --set "${certificate-path}\server.pem"
nscp settings --path /settings/WEB/server --key "certificate key" --set "${certificate-path}\server.key"
```

If we restart NSClient++ in debug mode again:

```
nscp test
...
exit
```

We should now have a valid certificate when we visit the web interface on `https://localhost:8443/`.

If you wish to remove the root certificate you can do so using:

```
mkcert -uninstall
```

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
$ nscp nrpe install --allowed-hosts 127.0.0.1  --insecure=false --verify=none --certificate ${certificate-path}\server.pem --certificate-key ${certificate-path}\server.key
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
$ nscp nrpe install --allowed-hosts 127.0.0.1 --insecure=false --verify=peer-cert --certificate ${certificate-path}\server.pem --certificate-key ${certificate-path}\server.key --ca ${certificate-path}\ca.pem
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
$ check_nrpe -H 127.0.0.1 -3 --client-cert nagios-client.pem --key nagios-client-key.pem
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

## TODO: Using the query language

## TODO: Checking with REST client

## TODO: Checking with NRDP

