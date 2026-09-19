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

An included file may be named with a [host name placeholder](#host-name-placeholders), which is how
one configuration rolled out to many machines pulls in a per-host file:

```ini
[/includes]
client = ${host}-nsclient.ini
```

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

### https settings

Https is a regular ini file (see above) except that it is loaded remotely and refreshed periodically.
The refresh period is configurable and if the file has changed NSClient++ will reload and re-read the new configuration.
If it is not possible to fetch the latest configuration NSClient++ will keep using the last one it received.

examples:

```shell
# Use remote configuration.
nscp settings --switch https://www.myserver.com/nsclient.ini
```

!!! warning "Use https, not http"

    The remote store is the agent's entire configuration: `[/modules]`, the external script
    definitions, the credentials the submit clients use. Plain `http://` does not authenticate
    the server at all, so anyone on the network path - or anyone who can answer for the host
    name through DHCP or DNS - decides what your agents run, as SYSTEM or root. It is re-fetched
    at boot and on every housekeeping pass, so one answered query is enough.

    NSClient++ therefore **refuses** a settings url, an `[/includes]` entry or an
    `[/attachments]` source that is not `https://`, and logs why. If you genuinely need plain
    http - a lab, an air-gapped network - opt in explicitly in `boot.ini`:

    ```ini
    [tls]
    allow plaintext = true
    ```

    The fetch then happens and is logged as `INSECURE` every time. See also
    [`verify mode`](#using-tls) below: an https url whose certificate
    is not verified is no better than plain http.

In the nsclient.ini file you can specify a series of attachments which will be downloaded (for instance scripts).

Adding a script:

```ini
[/attachments]
${scripts}/myscript.bat = https://www.myserver.com/myscript.bat
```

The key is where the file is written and the value is where it is fetched from. Both sides take
the [host name placeholders](#host-name-placeholders) below, and the key additionally takes the
usual path tokens, so one configuration can give every agent in a fleet its own file:

```ini
[/attachments]
${shared-path}/${host}-nsclient.ini = https://nsclient.mydom.local/nsclient/hosts/${host}-nsclient.ini
```

Name the target with a path token, or with an absolute path if you want it somewhere specific. A
bare relative name such as `scripts/myscript.bat` still works and is taken relative to
`${shared-path}` — but say which folder you mean, because the relative form used to be resolved
against the service's working directory and that is not something you can predict from the
configuration.

#### Query parameters

The url may carry a query string, which is passed on to the server unchanged.
This lets a script generate the configuration per host instead of serving a static file:

```ini
[settings]
1 = https://nsclient.mydom.local/nsclient/nsclient.php?RootFolder=myhost/&Filename=nsclient.ini
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

A settings url may contain the same host name placeholders the submit clients (NRDP, Graphite,
Syslog and friends) accept, so a single `boot.ini` can be rolled out to an entire fleet and each
agent asks for its own configuration:

```ini
[settings]
1 = https://cfgsrv/nsclient.php?host=${hostname}
```

| Placeholder | Expands to |
|---|---|
| `${hostname}` | the system host name as reported, e.g. `srv01.example.com` |
| `${host}` | the part before the first `.`, e.g. `srv01` |
| `${domain}` | the part after the first `.`, e.g. `example.com` |

Each of the three also has a `_lc` and a `_uc` variant (`${hostname_lc}`, `${host_uc}`, ...) that
lower- or upper-cases the result.

Placeholders are expanded before the url is parsed, so they may appear anywhere in it - in the
query, in the path (`https://cfgsrv/hosts/${host}/nsclient.ini`) or even in the host name. They are
expanded before percent-encoding, so a host name containing a character that needs escaping is
escaped rather than corrupting the request. The cache file name is derived from the expanded url,
so each host caches its own configuration.

The same placeholders are resolved in three more places, so a per-host configuration does not stop
at the settings url:

| Where | Example |
|---|---|
| A settings url | `[settings] 1 = https://cfgsrv/hosts/${host}.ini` |
| An attachment's source url | `[/attachments] scripts/local.bat = https://cfgsrv/${host}.bat` |
| An attachment's target path | `[/attachments] ${shared-path}/${host}.ini = https://cfgsrv/${host}.ini` |
| An included file | `[/includes] client = ${host}-nsclient.ini` |

An attachment target and an included file are paths, so they take path tokens (`${shared-path}`,
`${exe-path}`, ...) as well. The two kinds of placeholder share a syntax but not a vocabulary: the
host name placeholders in this table are resolved first, and everything else is left to the path
tokens. Only the copy being opened is expanded - the placeholder stays in the configuration file,
which is the point. When the substituted value lands in a local path (an attachment target, an
included file) it is additionally reduced to the characters a legal host name can contain, so a
host name outside the operator's control cannot carry a path separator or a `..` into a path the
agent reads or writes.

> **New in 0.17:** `${hostname}`, `${hostname_lc}` and `${hostname_uc}`, and host name placeholders
> in attachment targets and in `[/includes]` (issue
> [#458](https://github.com/mickem/nscp/issues/458)). The other placeholders already existed for the
> submit clients; this makes them available across the settings subsystem too.

An unrecognised path token is an error, and the setting that carried it is reported and skipped
rather than applied. It did not used to be: an unknown `${...}` quietly resolved to
the installation directory, so a mistyped `${scripst}/check.bat` was not rejected but turned into a
real path under the install folder — and whatever depended on it went somewhere nobody was looking.
That is also what made a pre-0.17 `${host}` in a path fail silently rather than loudly.

The tokens are not a closed list: anything you define in boot.ini's `[paths]` section is a valid
token everywhere a path is read. Only a token that is neither built in nor defined there is an
error.

If the query carries a credential (`?token=...`), note that it is still sent in clear text unless
the url is `https://`. NSClient++ keeps query parameters out of its own log and out of the settings
url it prints (`nscp settings --show`): both render a settings url as scheme, host and path only.
Anything else that handles the url - a proxy, the settings server's own access log - is of course
outside the agent's control.

> **Changed in 0.16.1:** query parameters used to be silently dropped from the request, so the
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

| Key             | Default Value | Values          | Description                                                                |
|-----------------|---------------|-----------------|----------------------------------------------------------------------------|
| version         | 1.3           | 1.0, 1.1, 1.3   | The TLS version to use.                                                    |
| verify mode     | peer          | none, peer      | The verify mode to use (Set this to none to use self signed certificates). |
| ca              | `${ca-path}`  | Path to CA file | The path to the CA certificate to use. Defaults to the platform CA bundle: the auto-exported Windows ROOT store on Windows, the distribution bundle on Linux. |
| allow plaintext | false         | true, false     | Allow a settings url, `[/includes]` entry or `[/attachments]` source that is not `https://`. Off by default: such a fetch is refused and logged. When on, every plaintext fetch is logged as `INSECURE`. |

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

Anywhere the configuration names a file or a folder you can write a `${token}` instead of a literal
path, and the agent substitutes it when it reads the setting. That is what lets one configuration
file work on a Windows install under `C:\Program Files`, a Linux package under `/usr/lib/nsclient`,
and a relocated installation — without any of them spelling out where they are.

### How a path is resolved

1. **Tokens are substituted**, and the result is substituted again, because a token's value may
   itself contain tokens. `${certificate-path}` is `${shared-path}/security`, which on the modern
   Windows layout is `${common-appdata}/NSClient++/security`. This is why moving one folder moves
   everything defined relative to it.
2. **Each token is looked up in this order**, first match wins:

    | Precedence | Source |
    |---|---|
    | 1 (highest) | `--path-override <key>=<value>` on the command line |
    | 2 | the `[paths]` section of `boot.ini` |
    | 3 (lowest) | the compiled-in default from the table below |

3. **An unknown token is an error.** The setting that carried it is reported by name and skipped,
   rather than applied. It previously resolved, silently, to the installation directory,
   so a mistyped `${scripst}/check.bat` became a real path under the install folder and whatever
   depended on it went somewhere nobody was looking. Note that the token set is *open*: anything you
   define in `[paths]` is a valid token everywhere, so this only rejects names that are neither
   built in nor defined by you.

Two rules apply to the value once the tokens are gone:

* **`none` means "no file at all"** and is never treated as a path. It is accepted by the log file
  (`[/settings/log] file name = none` switches file logging off) and by every `ca` option, where it
  falls back to the TLS library's own trust store.
* **A value that names no location of its own is taken relative to the folder its setting owns**,
  for the settings that *write* files — see [Relative paths](#relative-paths) below.

An absolute path is always used exactly as written. Pointing a setting anywhere on the filesystem
stays your decision; tokens are a convenience, not a restriction.

### The tokens

`Kind` says where the value comes from: **static** is the table below, **runtime** is a lookup the
binary makes about its own location or the OS, and **probe** means the answer depends on what is
actually on disk.

| Key              | Kind    | Value (Windows)            | Value (Linux)              | Comment                                                                |
|------------------|---------|----------------------------|----------------------------|------------------------------------------------------------------------|
| exe-path         | runtime | Folder holding `nscp.exe`  | Folder holding `nscp`      | Where the running binary lives. `base-path` is a synonym.              |
| base-path        | runtime | Folder holding `nscp.exe`  | Folder holding `nscp`      | Synonym for `exe-path`.                                                |
| shared-path      | mixed   | `${exe-path}` (legacy), `${common-appdata}/NSClient++` (modern) | /usr/lib/nsclient | Moves with `[layout] mode` on Windows. The package directory on Linux: root-owned, not written at runtime. |
| data-path        | runtime | The user's profile folder  | /var/lib/nsclient          | Writable per-machine state on Linux; the same as `${appdata}` on Windows. |
| certificate-path | static  | ${shared-path}/security    | ${shared-path}/security    | Shipped and admin-supplied certificates. Read-only at runtime on Linux. |
| module-path      | static  | ${exe-path}/modules        | ${shared-path}/modules     |                                                                        |
| web-path         | static  | ${shared-path}/web         | ${shared-path}/web         | Stays with the program, never in the writable state — see [File layout](file-layout.md#the-web-root-stays-with-the-program). |
| scripts          | static  | ${exe-path}/scripts        | ${shared-path}/scripts     |                                                                        |
| cache-folder     | static  | ${shared-path}/cache       | ${shared-path}/cache       | Where a downloaded configuration is cached.                            |
| crash-folder     | static  | ${shared-path}/crash-dumps | ${shared-path}/crash-dumps |                                                                        |
| log-path         | static  | ${shared-path}/log         | /var/log/nsclient          | Created and owned by the service account by the package.               |
| fleet-folder     | static  | ${shared-path}/fleet       | ${data-path}/fleet         | Everything the fleet sync owns: `fleet.ini`, staged scripts, bundle cache. Must be writable by the service account. |
| ca-path          | static  | ${certificate-path}/windows-ca.pem | the distribution's CA bundle | Trusted CA bundle used when a check names none of its own. Detected at build time on Linux. |
| temp             | runtime | The OS temporary folder    | /tmp                       | Shared with every other account on the machine.                        |
| common-appdata   | runtime | %ProgramData%              | N/A                        | Backs `${shared-path}` on the modern Windows layout.                   |
| appdata          | runtime | The user's profile folder  | N/A                        | Windows only; same value as `${data-path}`.                            |
| etc              | static  | N/A                        | /etc                       | Linux only; tracks the build's `--prefix`.                             |
| boot-conf        | special | ${exe-path}/boot.ini       | ${etc}/nsclient/boot.ini   | See [Special tokens](#special-tokens).                                 |
| nrpe-dh          | probe   | whichever candidate holds the files | same               | See [Special tokens](#special-tokens).                                 |
| modern-nrpe-dh   | static  | ${certificate-path}        | ${certificate-path}        | Candidate behind `${nrpe-dh}`.                                         |
| legacy-nrpe-dh   | static  | ${exe-path}/security       | ${certificate-path}        | Candidate behind `${nrpe-dh}`; the two candidates coincide on Linux.   |

The Linux values above are for a default `--prefix=/usr` package build; a build
with another prefix moves them together (see the packaging variables in
`CMakeLists.txt`). For the full picture of what lives where, and which account
owns it, see [File layout](file-layout.md).

On Windows, `${shared-path}` — and therefore everything defined relative to it —
depends on which layout the installation uses. `boot.ini`'s `[layout] mode`
selects it; see [File layout](file-layout.md#windows).

### Special tokens

Two tokens do not behave like the rest, and both will surprise you if you assume they do.

**`${nrpe-dh}` is a lookup, not a fixed path.** The shipped Diffie-Hellman parameters are package
content, so on Windows the installer leaves them beside the executable while `${certificate-path}`
moves to `%ProgramData%` under the modern layout — one fixed value cannot name both. So it is
resolved by looking: it answers `${modern-nrpe-dh}` when that folder actually contains
`nrpe_dh_*.pem`, and `${legacy-nrpe-dh}` otherwise. The lookup runs on every resolution, by design,
so the answer follows the files when an upgrade or a layout migration moves them. You can override
either candidate, or `nrpe-dh` itself to skip the lookup entirely. See
[File layout](file-layout.md#nrpe-dh).

**`${boot-conf}` is resolved before `[paths]` is read.** It names `boot.ini` itself, and `[paths]`
lives *inside* `boot.ini` — so a `[paths] boot-conf = ...` entry cannot take effect, because the
file would have to be found before it could be read. This is deliberate rather than an oversight.
Only `--path-override boot-conf=...` relocates it:

```shell
nscp --path-override boot-conf=/etc/nsclient/test-boot.ini service --run
```

### Relative paths

A value that carries neither a token nor a leading `/` (or drive letter) only means something
relative to *some* folder. That folder used to be the service's working directory, which is
`C:\Windows\System32` for a Windows service, `/` under a bare init script, the package directory
under the shipped systemd unit, and the shell's directory for `nscp test` — four different answers
from the same configuration file, none of them visible in it.

Settings that *write* files now name the folder they own, and a relative value lands there:

| Setting | Relative values land in |
|---|---|
| `[/attachments]` target | `${shared-path}` |
| `[/settings/log] file name` | `${log-path}` |
| `[/settings/crash] archive folder` | `${crash-folder}` |
| `[/settings/fleet] managed path` | `${fleet-folder}` |
| `[/settings/filewriter] file` | `${log-path}` |

Prefer naming the folder explicitly anyway — `${scripts}/check.bat` says what you mean, where
`scripts/check.bat` only works if you already know which base it is measured from.

A script *name* is a different thing and is not covered by this. `[/settings/python/scripts]` and
`[/settings/lua/scripts]` entries are resolved by **searching**, so a bare `check_foo.py` is found
in the module's script folder and the search either finds it or reports that it could not:

| Written as | Found at |
|---|---|
| `check_foo.py` | `${scripts}/python/check_foo.py`, or `${scripts}/check_foo.py` |
| `sub/check_foo.py` | `${scripts}/python/sub/check_foo.py`, or `${scripts}/sub/check_foo.py` |

Lua is the same with `lua` in place of `python`. The search also tries the value as-is first, so a
name that happens to exist relative to the service's working directory wins — see the warning
below.

Whatever the search finds has to be **inside the script folder**, or inside a folder you have named
as an additional root. A value that climbs out of it (`../foo.py`) or points somewhere else
entirely is refused, with the allowed folders named in the log. Scripts the agent does not
ship — a plugin package's own `libexec`, a vendor directory — are allowed by listing them:

```ini
[/settings/python]
additional script roots = /usr/lib/nagios/plugins, ${shared-path}/vendor
```

Entries are comma separated and each is expanded, so path tokens work.

### External scripts resolve differently

`[/settings/external scripts/scripts]` does **not** work this way, and the difference catches
people out. The value is a command line, not a path: it is handed to the operating system to
execute, so there is no search and, importantly, **no `${...}` expansion**. Writing
`${scripts}/check_foo.sh` there does not work — the token reaches the shell literally.

| Written as | What happens |
|---|---|
| `${scripts}/check_foo.sh` | **fails** — tokens are not expanded for external scripts |
| `check_foo.sh` | **fails** — a name with no directory separator is looked up on `PATH`, not in the current directory |
| `scripts/check_foo.sh` | resolved relative to the service's working directory |
| `/opt/nscp/scripts/check_foo.sh` | works, always |

The conventional `scripts\check_foo.bat` works on a normal install because the working directory
happens to contain `scripts`: on Windows the service starts external scripts with the installation
directory as their working directory, and on Linux the shipped systemd unit sets `WorkingDirectory`
to the package directory. Neither is something the configuration states, so **prefer an absolute
path** for an external script, or keep the conventional relative form and be aware it depends on
how the agent was started.

`nscp ext-scr add --import <file>` writes that value for you, and picks the spelling that works on
the platform it runs on: `scripts\<name>` on Windows, where the working directory is known, and the
destination's absolute path everywhere else.

### Overriding

All paths can also be overridden using the `[paths]` section in `boot.ini`.

Example of overriding a path (web root folder):

```ini
[paths]
web-path = /tmp/foo
```

Path overrides can also be supplied per-invocation on the command line, which
takes precedence over anything in `boot.ini` for the keys it specifies:

```shell
nscp client --path-override module-path=/build/modules --path-override log-path=/build/logs ...
```

An override has to name an **absolute** location, whether it comes from
`[paths]` or from `--path-override`. It may be written in terms of other tokens
(`scripts = ${shared-path}/mine`) as long as the result is absolute. An override
that resolves to a relative path, or that names a token which does not exist, is
reported and ignored, and the built-in default is used instead — a relative one
would be read and written relative to the service's working directory, which is
`C:\Windows\System32` for a Windows service, `/` under a bare init script and the
package directory under the shipped systemd unit. Nothing useful can be written
against a base that changes with how the agent was started.

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