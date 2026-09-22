---
icon: "🍎"
modules: [packaging, CheckSystem, CheckDisk, CheckLogFile]
action: none
---
**NSClient++ now ships a macOS (Apple silicon) package.** Nothing changes for
existing Windows or Linux installations. Releases now also carry
`NSCP-<version>-macos-arm64.pkg` and a plain
`NSCP-<version>-macos-arm64.tar.gz`. The `.pkg` is self-contained - it bundles
the libraries it links against, so the target Mac needs no Homebrew - and
installs under `/usr/local`, creates a hidden `_nsclient` service account, and
registers the agent as a launchd daemon
(`/Library/LaunchDaemons/com.nsclient.nscp.plist`) that starts at boot. It is
managed with `launchctl` rather than `systemctl`:

```bash
sudo installer -pkg NSCP-<version>-macos-arm64.pkg -target /
sudo launchctl print system/com.nsclient.nscp          # status
sudo launchctl kickstart -k system/com.nsclient.nscp   # restart
sudo /usr/local/sbin/uninstall-nsclient --purge        # remove
```

Three caveats for this first release:

* **Apple silicon only.** There is no Intel build, and Rosetta 2 does not
  substitute for one; the installer refuses an Intel Mac rather than installing
  something that cannot run.
* **Not signed or notarized.** Gatekeeper blocks a double-click install. Use
  `sudo installer` from a terminal, or right-click the package and choose
  *Open*.
* **`CheckSystem`, `CheckDisk` and `CheckLogFile` are not in the macOS build.**
  Their data sources are Linux kernel interfaces (procfs, `mntent`, `inotify`),
  so there are no CPU, memory, process, uptime, service, disk or log-file
  checks on macOS yet. Everything else - the REST API and web UI, the
  NRPE/NSCA/NSCP/check_mk listeners and clients, external scripts, the Lua and
  Python script engines, the network and security checks, the scheduler and the
  forwarders - is present. A macOS configuration that enables one of those three
  modules logs a "module not found" error on startup, which is worth checking
  for if you are reusing a Linux `nsclient.ini`.

See [Installing on macOS](installing.md#installing-on-macos-pkg) and
[Supported platforms](supported-platforms.md#macos).
