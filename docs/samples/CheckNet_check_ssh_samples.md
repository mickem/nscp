**Check that an SSH server presents a valid banner:**

```
check_ssh host=github.com
OK: github.com:22 ok in 13ms
L        cli  Performance data: 'github.com_22_time'=13;1000;5000
```

**Non-standard SSH port:**

```
check_ssh host=192.168.56.10 port=2222
OK: 192.168.56.10:2222 ok in 2ms
```

**A port that is not speaking SSH is CRITICAL (`no_match`):**

```
check_ssh host=www.google.com port=443
CRITICAL: www.google.com:443 no_match in 12ms
```

**Report what the server is running:**

```
check_ssh host=192.168.56.10 "top-syntax=${list}" "detail-syntax=${host} runs ${software} ${software_version} (SSH ${protocol}, ${comments})"
OK: 192.168.56.10 runs OpenSSH 9.6p1 (SSH 2.0, Ubuntu-3ubuntu13.5)
```

**Show the raw identification string:**

```
check_ssh host=gitlab.com "top-syntax=${list}" "detail-syntax=${banner}"
OK: SSH-2.0-GitLab-SSHD
```

**Alert when the server still speaks the insecure SSHv1 (`1.99` or `1.x`):**

```
check_ssh host=192.168.56.10 "crit=protocol_major < 2" "top-syntax=${list}" "detail-syntax=${host} speaks SSH ${protocol}"
OK: 192.168.56.10 speaks SSH 2.0
```

**Alert on an outdated sshd:**

```
check_ssh host=192.168.56.10 "crit=software = 'OpenSSH' and software_version not like '9.'" "top-syntax=${list}" "detail-syntax=${software} ${software_version}"
OK: OpenSSH 9.6p1
```

**Check a fleet and list each server's version:**

```
check_ssh hosts=github.com,gitlab.com,bitbucket.org "top-syntax=${list}" "detail-syntax=${host}: ${version}"
OK: github.com: 7f27de7, gitlab.com: GitLab-SSHD, bitbucket.org: conker_20260806-85ca5cadcf
```

**Tighter response-time thresholds:**

```
check_ssh host=192.168.56.10 "warn=time > 200" "crit=time > 1000 or result != 'ok'"
OK: 192.168.56.10:22 ok in 3ms
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ssh --argument "host=192.168.56.10"
OK: 192.168.56.10:22 ok in 2ms
```
