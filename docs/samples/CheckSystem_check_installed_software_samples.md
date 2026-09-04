**Default check (inventory: package count as status and perf):**

```
check_installed_software
OK: 101 software packages installed.|'count'=101;0;0
```

**Alert when unwanted software is present (an absent product is OK):**

```
check_installed_software "crit=name like 'Notepad++'"
CRITICAL: Notepad++ (64-bit x64) 1.0.0 (Notepad++ Team)|'count'=101;0;0

check_installed_software "crit=name like 'BitTorrent'"
OK: 101 software packages installed.|'count'=101;0;0
```

**Detect recent installs (correlate incidents with software changes):**

```
check_installed_software "warn=install_date > -30d" "top-syntax=${status}: ${warn_count} recent installs: ${warn_list}"
WARNING: 2 recent installs: PowerToys (Preview) 0.100.2 (Microsoft Corporation), Microsoft Edge 151.0.4129.72 (Microsoft Corporation)|'count'=101;0;0
```

**Threshold on installed size (large packages):**

```
check_installed_software "warn=size > 500M" "top-syntax=${status}: ${warn_count} packages over 500M"
WARNING: 3 packages over 500M|'count'=428;0;0
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_installed_software --argument "crit=name like 'TeamViewer'"
OK: 101 software packages installed.
```

##### Windows

**Flag EOL software by version (string comparison — pin the major with like):**

```
check_installed_software "filter=name like 'Java 7'" "crit=version like '7.'"
CRITICAL: Java 7 Update 51 7.0.510 (Oracle)|'count'=1;0;0
```

**List per-user installs (software outside the machine-wide hives):**

```
check_installed_software "filter=hive = 'user'" "top-syntax=${status}: ${count} per-user packages: ${list}"
OK: 18 per-user packages: GitHub Desktop 3.6.3 (GitHub, Inc.), CLion 2026.2 (JetBrains s.r.o.), Microsoft Visual Studio Code (User) 1.115.0 (Microsoft Corporation), ...|'count'=18;0;0
```

**Only 32-bit software installed on a 64-bit host:**

```
check_installed_software "filter=architecture = 'x86'" "top-syntax=${status}: ${count} 32-bit packages"
OK: 51 32-bit packages|'count'=51;0;0
```

**Include SystemComponent entries (hidden from Add/Remove Programs):**

```
check_installed_software filter=none
OK: 233 software packages installed.|'count'=233;0;0
```

##### Linux

**Alert when unwanted software is present:**

```
check_installed_software "crit=name like 'telnetd'"
CRITICAL: telnetd 0.17-41 (Debian telnet maintainers)|'count'=428;0;0
```

**Flag EOL software (pin the version prefix with like):**

```
check_installed_software "filter=name like 'openjdk-7'" "crit=version like '7u'"
CRITICAL: openjdk-7-jre 7u51-2.4.6-1 (Debian Java Maintainers)|'count'=1;0;0
```

**Custom output showing the detected package manager:**

```
check_installed_software "filter=name = 'bash'" "top-syntax=${status}: ${list}" "detail-syntax=${name} ${version} via ${manager}"
OK: bash 5.2.21-2 via dpkg|'count'=1;0;0
```
