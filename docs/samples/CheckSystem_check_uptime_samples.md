**Default check:**

```
check_uptime
uptime: -9:02, boot: 2013-aug-18 08:29:13
'uptime uptime'=1376814553s;1376760683;1376803883
```

Adding **warning and critical thresholds**::

```
check_uptime "warn=uptime < -2d" "crit=uptime < -1d"
...
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_uptime
uptime: -0:3, boot: 2013-sep-08 18:41:06 (UCT)|'uptime'=1378665666;1378579481;1378622681
```

