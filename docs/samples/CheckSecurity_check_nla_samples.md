**Assert the active network is the domain profile (Windows)**

`check_nla` reports each network's Location Awareness category. There is no
default threshold — assert the posture you expect:

```
check_nla "crit=connected = 1 and category != 'domain'" "detail-syntax=${network}=${category}"
L        cli OK: all networks ok
```

**Alert if any connected network is classified Public**

```
check_nla "crit=connected = 1 and category = 'public'"
L        cli CRITICAL: Wi-Fi=public
```

**List every known network and its category**

```
check_nla "top-syntax=${list}" "detail-syntax=${network}: ${category} (connected=${connected})"
L        cli OK: Corp.example.com: domain (connected=1), Café-WiFi: public (connected=0)
```

**On non-Windows platforms**

```
check_nla
L        cli UNKNOWN: check_nla is not supported on this platform (Windows Network Location Awareness only)
```
