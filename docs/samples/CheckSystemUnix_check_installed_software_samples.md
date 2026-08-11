**Default check (inventory: package count as status and perf):**

```
check_installed_software
OK: 428 software packages installed.|'count'=428;0;0
```

**Alert when unwanted software is present (an absent package is OK):**

```
check_installed_software "crit=name like 'telnetd'"
CRITICAL: telnetd 0.17-41 (Debian telnet maintainers)|'count'=428;0;0

check_installed_software "crit=name like 'bittorrent'"
OK: 428 software packages installed.|'count'=428;0;0
```

**Flag EOL software (pin the version prefix with like):**

```
check_installed_software "filter=name like 'openjdk-7'" "crit=version like '7u'"
CRITICAL: openjdk-7-jre 7u51-2.4.6-1 (Debian Java Maintainers)|'count'=1;0;0
```

**Detect recent installs (correlate incidents with software changes):**

```
check_installed_software "warn=install_date > -7d" "top-syntax=${status}: ${warn_count} recent installs: ${warn_list}"
WARNING: 2 recent installs: htop 3.3.0-4 (Debian), nmap 7.94-1 (Debian Security Tools)|'count'=428;0;0
```

**Custom output showing the detected package manager:**

```
check_installed_software "filter=name = 'bash'" "top-syntax=${status}: ${list}" "detail-syntax=${name} ${version} via ${manager}"
OK: bash 5.2.21-2 via dpkg|'count'=1;0;0
```

**Threshold on installed size (large packages):**

```
check_installed_software "warn=size > 500M" "top-syntax=${status}: ${warn_count} packages over 500M"
WARNING: 3 packages over 500M|'count'=428;0;0
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_installed_software --argument "crit=name like 'telnetd'"
OK: 428 software packages installed.
```
