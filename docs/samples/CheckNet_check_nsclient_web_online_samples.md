**Check that a remote NSClient++ agent's REST API is reachable:**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=secret
OK: REST API reachable on https://192.168.56.10:8443
```

**Give host and port separately instead of a URL:**

```
check_nsclient_web_online host=192.168.56.10 port=8443 password=secret
OK: REST API reachable on https://192.168.56.10:8443
```

**Run a check on the remote agent and pass its result through:**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=secret command=check_cpu
OK: CPU load is ok.
```

**Pass arguments to the remote check (repeat `argument=`):**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=secret command=check_drivesize argument=drive=/ "argument=warn=used>80%"
OK: / 42.1% used
```

**A wrong password reports the authentication failure:**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=wrong
CRITICAL: Authentication failed (HTTP 403) on https://192.168.56.10:8443
```

**An unreachable agent is CRITICAL:**

```
check_nsclient_web_online url=https://192.168.56.10:9999 password=secret
CRITICAL: Failed to reach https://192.168.56.10:9999: Connection refused
```
