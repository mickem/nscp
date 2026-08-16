# Settings

The NSClient++ settings store is a hierarchical tree structure with key value pairs.
Traditionally this is stored in a flat ini-style file where the "paths" are defined as sections.
But there are other options as well and for instance the registry is another common place to store settings.
With the new configuration UI in 0.4.3 it is simpler to manipulate the settings if they are store in the registry and
thus probably a better place to keep them.

Which keys are available and what they mean are describe by the modules which use the various keys.
Thus it the place to find the documentation for the various configuration options are in the reference section of the
documentation.
Where it is split by module.

## Include

Another really useful feature of the settings in NSClient++ is the ability to include various other settings.
This is very flexible and you can include ini file from the registry and vice versa.

The way to include a file (if you are using ini-files) is to add a key under the /includes section.

including file:

```ini
[/includes]
foo = foo.ini
```

You can include any number of files registry or other stores. and they will be instantiated in a tree structure with a
parent child relationship.
Important to note here is that the first found key will be used. So parents will override children.

And example of this:

* nsclient.ini:

```ini
[/includes]
client = client.ini

[/test]
key1 = This values comes from nsclient.ini
```

* client.ini:

```ini
[/includes]
baseline = baseline.ini

[/test]
key1 = This values comes from client.ini
key2 = This values comes from client.ini
```

* baseline.ini:

```ini
[/test]
key1 = This values comes from baseline.ini
key2 = This values comes from baseline.ini
key3 = This values comes from baseline.ini
```

in the above example the values of /test keyx will be:

- key1=This values comes from nsclient.ini
- key2=This values comes from client.ini
- key3=This values comes from baseline.ini

This can be very useful to distribute a baseline configuration for a company or monitoring product.
Then all "machine specific customization" would go into the nsclient.ini config where as client.ini would be reserved
for the clients global config. And finally baseline.ini would be monitoring tool specific configuration.

## Settings stores

Settings are historically stored in an ini file but you can store settings in many other locations as well.
While the most obvious one to use is the registry there are other options as well.

- ini files
- registry
- dummy
- http (ini files over http)

### Ini settings

Ini file are the simplest form of configuration and also the default though on windows registry is probably a better
option.
The files are text-files following the ini file format where you have sections in brackets [] and key key = values.

sample.ini:

```ini
[/section/child/section]
key = value
```

To use an ini file you prefix the settings url with ini:// then you can use various folder strings or specify a relative
or absolute path to the file.

examples:

* Use the relative file foo.ini `nscp settings --migrate-to ini://foo.ini`
* Use the relative file foo.ini in a subfolder called conf `nscp settings --migrate-to ini://conf/foo.ini`
* Use a file stored in the profile folder (on Windows): C:\Documents and Settings\All Users\Application Data\NSClient++
  `nscp settings --migrate-to ini://%(common-appdata)/NSClient++/nsclient.ini`

### registry settings

Registry is only available on Windows and using them on Windows is recommended as it integrates better with windows and
windows management tools where you can push configuration changes and similar things.
As the registry is naturally a tree structure we use folders as section and keys and values for keys and values.

To use an ini file you prefix the settings url with ini:// then you can use various folder strings or specify a relative
or absolute path to the file.

examples:

```shell
# Use the default registry location
$ nscp settings --migrate-to registry
# Use HKEY_LOCAL_MACHINE/software/NSClient++ to store configuration
$ nscp settings --migrate-to registry://HKEY_LOCAL_MACHINE/software/NSClient++
```

### http settings

Http/Https is a regular ini file (see above) except that it is loaded remotely and refreshed periodically.
The refresh period is configurable and if the file has changed NSClient++ will reload and re-read the new configuration.
If it is not possible to fetch the latest configuration NSClient++ will keep using the last one it received.

examples:

```shell
# Use remote configuration.
nscp settings --switch http://www.myserver.com/nsclient.ini
```

In the nsclient.ini file you can specify a series of attachments which will be downloaded (for instance scripts).

Adding a script:

```ini
[/attachments]
scripts/myscript.bat = http://www.myserver.com/myscript.bat
```

#### Query parameters

The url may carry a query string, which is passed on to the server unchanged.
This lets a script generate the configuration per host instead of serving a static file:

```ini
[settings]
1 = http://nsclient.mydom.local/nsclient/nsclient.php?RootFolder=myhost/&Filename=nsclient.ini
2 = ini://${shared-path}/nsclient.ini
```

Each distinct query gets its own file in the cache folder, so several urls pointing at the same
script with different parameters do not overwrite each other's cached configuration. An existing
cache file written by an older version is moved to the new name on first start, so a host that
cannot reach its settings server during the upgrade still boots off its cached configuration.

Characters that are not legal in a url query - a space, most notably - are percent-encoded before
the request is sent. Anything already written as `%XX` is left as it is, so a query you encoded
yourself is not encoded twice.

#### Host name placeholders

The url may contain the same host name placeholders the submit clients (NRDP, Graphite, Syslog and
friends) accept, so a single `boot.ini` can be rolled out to an entire fleet and each agent asks for
its own configuration:

```ini
[settings]
1 = http://cfgsrv/nsclient.php?host=${hostname}
```

| Placeholder | Expands to |
|---|---|
| `${hostname}` | the system host name as reported, e.g. `srv01.example.com` |
| `${host}` | the part before the first `.`, e.g. `srv01` |
| `${domain}` | the part after the first `.`, e.g. `example.com` |

Each of the three also has a `_lc` and a `_uc` variant (`${hostname_lc}`, `${host_uc}`, ...) that
lower- or upper-cases the result.

Placeholders are expanded before the url is parsed, so they may appear anywhere in it - in the
query, in the path (`http://cfgsrv/hosts/${host}/nsclient.ini`) or even in the host name. They are
expanded before percent-encoding, so a host name containing a character that needs escaping is
escaped rather than corrupting the request. The cache file name is derived from the expanded url,
so each host caches its own configuration.

> **New in 0.17:** `${hostname}`, `${hostname_lc}` and `${hostname_uc}`. The other placeholders
> already existed for the submit clients; this makes them available in settings urls too.

If the query carries a credential (`?token=...`), note that it is still sent in clear text unless
the url is `https://`. NSClient++ keeps query parameters out of its own log and out of the settings
url it prints (`nscp settings --show`): both render a settings url as scheme, host and path only.
Anything else that handles the url - a proxy, the settings server's own access log - is of course
outside the agent's control.

> **Changed in 0.17:** query parameters used to be silently dropped from the request, so the
> server only ever saw the bare path.

#### Using TLS

You likely want to use TLS when using http settings.
To use TLS you need to configure either If you are using a custom CA or want to use self signed certificates that has to
be configured in the `boot.ini` file.
This is a special file which is used to load configuration before the configuration is loaded.
For instance, it defines which settings store to use but you can also configure TLS options.

> **Changed in 0.14:** the settings download now verifies the server certificate by default
> (`verify mode = peer` against `${ca-path}`, the platform CA bundle). Earlier versions defaulted to
> `verify mode = none`, which meant anyone who could answer for the settings host controlled the
> agent's entire configuration - including `[/settings/external scripts]`, which is command
> execution by design. See [Upgrading to verified settings downloads](#upgrading-to-verified-settings-downloads).

```ini
[tls]
version = 1.3
verify mode = peer
ca = c:\program files\NSClient++\security\ca.pem
```

| Key         | Default Value | Values          | Description                                                                |
|-------------|---------------|-----------------|----------------------------------------------------------------------------|
| version     | 1.3           | 1.0, 1.1, 1.3   | The TLS version to use.                                                    |
| verify mode | peer          | none, peer      | The verify mode to use (Set this to none to use self signed certificates). |
| ca          | `${ca-path}`  | Path to CA file | The path to the CA certificate to use. Defaults to the platform CA bundle: the auto-exported Windows ROOT store on Windows, the distribution bundle on Linux. |

##### Upgrading to verified settings downloads

If your settings server presents a certificate issued by a public CA, the new defaults work with no
configuration change.

If it presents a self-signed certificate, or one issued by a private/internal CA, the download will
now fail with a certificate verification error where it previously succeeded silently. Either point
`ca` at the issuing CA (recommended):

```ini
[tls]
verify mode = peer
ca = c:\program files\NSClient++\security\my-internal-ca.pem
```

or restore the previous behaviour explicitly, accepting that the configuration channel is
unauthenticated:

```ini
[tls]
verify mode = none
```

`verify mode = none` is still honoured, but it is now logged as a warning naming the risk on every
fetch - it can only be reached by writing it out, never by omission.

The Windows installer applies the same defaults when it downloads a settings source given as
`CONFIGURATION_TYPE=https://...`, and verifies against a temporary export of the Windows ROOT store
(`${ca-path}` belongs to the service and is not written until it first starts). Use the `TLS_CA` and
`TLS_VERIFY_MODE` properties to make the same two choices at install time - see
[Installing](../setup/installing.md#verifying-the-settings-server).

#### Using a proxy

If NSClient++ has to reach the configuration server through an HTTP proxy you can configure that in `boot.ini` as well.
The proxy is applied to the initial download and to every refresh, and is also used by any attachments declared in the
remote configuration. HTTPS targets are tunnelled through the proxy via an HTTP `CONNECT` request, so the same setting
covers both `http://` and `https://` settings URLs.

```ini
[proxy]
url = http://proxy.corp.example:3128/
no_proxy = localhost,127.0.0.1,.internal
```

| Key      | Default Value | Values                                           | Description                                                                                                                             |
|----------|---------------|--------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| url      |               | `scheme://[user:pass@]host[:port][/]`            | Proxy URL.  An empty value disables the proxy.  Currently only the `http` scheme is supported (CONNECT tunnelling for HTTPS targets).   |
| no_proxy |               | comma-separated list of hostnames or `.suffix`es | Hosts that should bypass the proxy.  An entry beginning with `.` is a suffix match (`.internal` matches `foo.internal` and `internal`). |

If the proxy requires authentication, embed the credentials in the URL — they are sent as a `Proxy-Authorization: Basic`
header (HTTPS targets receive them in the `CONNECT` request, plain HTTP targets receive them in the proxied request).
The username and password are URL-decoded, so any `@` or `:` inside them must be percent-encoded.

```ini
[proxy]
url = http://alice:s%40cret@proxy.corp.example:3128/
```

> Failed downloads still fall back to the cached copy of the configuration if one is present, so a transient proxy
> outage will not stop NSClient++ from starting — but the very first run on a fresh machine needs the proxy to be
> reachable.

## Using settings stores

NSClient++ has some feature to help work with settings stores.

All of this is performed via the settings sub command: `nscp settings --help`

Migrating settings from a ini file to the registry can be done with the migrate-to command:
`nscp settings --migrate-to registry`

This will move all data from the settings file and store it in in the registry and then setup NSClient++ to use the
registry instead of the ini file.

You can also switch settings store (without migrating data): `nscp settings --switch registry`

The effect is similar in that NSClient++ will start using the registry but you have to add the keys to the registry
manually.

To show the current settings store view can run:

```shell
$ nscp settings --show
INI settings: (ini://${shared-path}/nsclient.ini, C:\source\build\x64\dev/nsclient.ini)
```

## Overriding settings store

The default way for NSClient++ to find your settings files is via the boot.ini file.

If you want to override this, for instance you want to use multiple NSClient++ from the same folder, you can do this via
the `--settings` option: `nscp test --settings nsclient2.ini`

You can do this for the service as well by editing the service start command.

## Paths

Paths can be used in various places in the settings store to locate files. To facilitate reusable paths, there are a
number
of path variables that can be used.

| Key              | Value (Windows)                 | Value (Linux)          | Comment                                                                |
|------------------|---------------------------------|------------------------|------------------------------------------------------------------------|
| certificate-path | ${shared-path}/security         | ${shared-path}/security | Shipped and admin-supplied certificates. Read-only at runtime on Linux. |
| module-path      | ${exe-path}/modules             | ${shared-path}/modules | Moves with `shared-path` on Linux.                                     |
| web-path         | ${shared-path}/web              | ${shared-path}/web     |                                                                        |
| scripts          | ${exe-path}/scripts             | ${shared-path}/scripts |                                                                        |
| cache-folder     | ${shared-path}/cache            | ${shared-path}/cache   |                                                                        |
| crash-folder     | ${shared-path}/crash-dumps      | ${shared-path}/crash-dumps |                                                                    |
| log-path         | ${shared-path}/log              | /var/log/nsclient      | Created and owned by the service account by the package.               |
| common-appdata   | %ProgramData%                   | N/A                    | Backs `${shared-path}` on the modern Windows layout.                   |
| fleet-folder     | ${shared-path}/fleet            | ${data-path}/fleet     | Everything the fleet sync owns: `fleet.ini`, staged scripts, bundle cache. Must be writable by the service account. |
| data-path        | The user's profile folder.      | /var/lib/nsclient      | Writable per-machine state on Linux; also `${appdata}` on Windows.     |
| base-path        | Path of NSClient++ exe file     |                        | This will in the future change to an actual shared path.               |
| temp             | The temporary file path         | /tmp                   |                                                                        |
| shared-path      | Path of NSClient++ exe file     | /usr/lib/nsclient      | The package directory on Linux: root-owned, not written at runtime.    |
| exe-path         | Path of NSClient++ exe file     |                        |                                                                        |
| common-appdata   | Application data for all users. | N/A                    | The file system directory that contains application data for all users |
| appdata          | The user's profile folder.      | N/A                    |                                                                        |
| etc              | N/A                             | /etc                   | Linux only                                                             |

The Linux values above are for a default `--prefix=/usr` package build; a build
with another prefix moves them together (see the packaging variables in
`CMakeLists.txt`). For the full picture of what lives where, and which account
owns it, see [File layout](file-layout.md).

On Windows, `${shared-path}` — and therefore everything defined relative to it —
depends on which layout the installation uses. `boot.ini`'s `[layout] mode`
selects it; see [File layout](file-layout.md#windows).

All paths can also be overridden using the `[paths]` section in `boot.ini`.

Example of overriding a path (web root folder):

```ini
[paths]
web-path = /tmp/foo
```

Path overrides can also be supplied per-invocation on the command line, which
takes precedence over anything in `boot.ini` for the keys it specifies:

```shell
nscp client --path-override module-path=/build/modules --path-override log-path=. ...
```

<!-- @formatter:off -->
!!! note "Moved in 0.12.5"
    Before 0.12.5 path overrides lived in the main configuration file under a `[/paths]` section. They were moved to
    `boot.ini`'s `[paths]` section so that overrides take effect for *all* path lookups - including the bootstrap-time
    lookup that decides where the main configuration file itself lives. If you had a `[/paths]` section in your
    `nsclient.ini`, copy each `key = value` to a `[paths]` section in `boot.ini` (next to `nscp.exe`) and delete the old
    section from `nsclient.ini`; there is no automatic migration.
<!-- @formatter:on -->

## Security

Storing sensitive information in the settings store is not recommended.
You can solve this in various ways:

* The simplest approach is to move the settings file to a location only readable by the user running NSClient++.
* Use the credential manager to store sensitive information.

To learn more about the credential manager, see [Securing NSClient++](../setup/securing.md).