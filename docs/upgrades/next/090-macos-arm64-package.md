---
icon: "🍎"
modules: [packaging, CheckDisk, CheckLogFile]
action: none
---
**NSClient++ now ships an experimental macOS (Apple silicon) package.** Nothing
changes for existing Windows or Linux installations. Experimental in the sense a
young check command is: it works, but the packaging - the install layout, the
service account, the launchd job - is not settled, so read this note again on
the next upgrade rather than applying it blind. Releases now also carry
`NSCP-<version>-macos-arm64.pkg` and a plain
`NSCP-<version>-macos-arm64.tar.gz` of the same tree. Both are self-contained -
they bundle the libraries they link against, so the target Mac needs no
Homebrew. The `.pkg` installs under `/usr/local`, creates a hidden `_nsclient`
service account, and registers the agent as a launchd daemon
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
* **Not signed or notarized.** Gatekeeper refuses the package with "cannot be
  opened because Apple cannot check it for malicious software". Install from a
  terminal instead, which does not go through that check:
  `sudo installer -pkg NSCP-<version>-macos-arm64.pkg -target /`. To install
  from the Finder you have to clear the quarantine flag your browser set
  (`xattr -d com.apple.quarantine <pkg>`) or approve it under System Settings →
  Privacy & Security after the first attempt.
* **`CheckDisk` and `CheckLogFile` are not in the macOS build.** Their data
  sources are Linux kernel interfaces (`mntent`, `inotify`), so there are no
  disk or log-file checks on macOS yet. Everything else - `CheckSystem` (see
  the note on it below), the REST API and web UI, the NRPE/NSCA/NSCP/check_mk
  listeners and clients, external scripts, the Lua script engine, the network
  and security checks, the scheduler and the forwarders - is present. A macOS
  configuration that enables one of those two modules logs a "module not found"
  error on startup, which is worth checking for if you are reusing a Linux
  `nsclient.ini`.

See [Installing on macOS](installing.md#installing-on-macos-pkg) and
[Supported platforms](supported-platforms.md#macos).
