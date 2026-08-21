**Check that Windows is activated (Windows)**

The default is critical when Windows is not licensed and warning when a grace or
KMS renewal period has less than 30 days left.

```
check_activation
L        cli OK: Windows(R), Professional edition: licensed (genuine, grace 0d)|'license_grace'=0d;0;0
```

```
check_activation
L        cli CRITICAL: Windows(R), Professional edition: initial_grace (genuine, grace 12d)|'license_grace'=12d;0;0
```

**Show the channel, genuine state and remaining grace period**

```
check_activation "top-syntax=${status}: ${list}" "detail-syntax=${name} [${channel}] status=${activation_status} genuine=${genuine_state} grace=${grace_days}d"
L        cli OK: Windows(R), Professional edition [Volume:GVLK] status=licensed genuine=genuine grace=178d|'license_grace'=178d;0;0
```

**Warn earlier on a KMS client whose renewal is falling behind**

A KMS activation is good for 180 days and is renewed every 7 days, so a
countdown that gets far down means renewal has been failing for months.

```
check_activation "warning=grace_days > 0 and grace_days < 90"
L        cli WARNING: Windows(R), Professional edition: licensed (genuine, grace 61d)|'license_grace'=61d;0;0
```

**Include every licensed product, not just Windows**

Give each product its own perfdata label when you do.

```
check_activation all-products=true "detail-syntax=${name}: ${activation_status}" "perf-syntax=${key}"
L        cli OK: Windows(R), Professional edition: licensed, Office 16, Office16ProPlus edition: licensed|'W269N_grace'=0d;0;0 '6MWKP_grace'=0d;0;0
```

**Alert only when Windows reports itself as non-genuine**

`genuine_state` is `unknown` when the state could not be determined, so exclude
it to avoid alerting on a missing answer.

```
check_activation "critical=genuine = 0 and genuine_state != 'unknown'"
L        cli CRITICAL: Windows(R), Professional edition: notification (invalid_license, grace 0d)|'license_grace'=0d;0;0
```

**Skip the genuine evaluation**

```
check_activation skip-genuine=true
L        cli OK: Windows(R), Professional edition: licensed (unknown, grace 0d)|'license_grace'=0d;0;0
```

**On non-Windows platforms**

```
check_activation
L        cli UNKNOWN: check_activation is not supported on this platform (Windows Software Licensing only)
```
