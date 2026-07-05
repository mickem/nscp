#### Count the logged-on users

```
check_users
L        cli OK: 2 user(s) logged on: mickem, root
```

#### Alert when too many sessions are open

`count` is a built-in summary variable.

```
check_users "warn=count > 5" "crit=count > 10"
L        cli OK: 2 user(s) logged on: mickem, root
```

#### Alert on any interactive session (e.g. a locked-down server)

```
check_users "crit=count > 0"
L        cli CRITICAL: 2 user(s) logged on: mickem, root
```

#### Only count RDP / remote sessions

```
check_users "filter=session_type = 'rdp'" "crit=count > 0"
```

On Linux, network logins (ssh) have `session_type = 'remote'`:

```
check_users "filter=session_type = 'remote'" "detail-syntax=${user}@${client}" "top-syntax=${list}"
L        cli OK: mickem@10.0.0.5
```

#### Alert on a disconnected-but-open RDP session (Windows)

```
check_users "crit=session_state = 'disconnected'" "detail-syntax=${user} (${session_state})"
```
