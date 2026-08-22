**Default check (healthy domain member; actively verifies the channel):**

```
check_secure_channel
OK: secure channel to EXAMPLE via DC01.example.com: OK
```

**Broken secure channel (machine-account password out of sync):**

```
check_secure_channel
CRITICAL: secure channel to EXAMPLE via : The trust relationship between this workstation and the primary domain failed.
```

**Passive status query only (do not contact the DC):**

```
check_secure_channel verify=false
OK: secure channel to EXAMPLE via DC01.example.com: OK
```

**Check the channel to a specific trusted domain:**

```
check_secure_channel domain=PARTNER
OK: secure channel to PARTNER via DC05.partner.example: OK
```

**Custom output with the raw status code:**

```
check_secure_channel "detail-syntax=${domain}: dc=${dc} code=${error_code}"
OK: EXAMPLE: dc=DC01.example.com code=0
```

**On a workgroup machine (the fleet-wide-safe contract):**

```
check_secure_channel
This machine is not joined to a domain (workgroup WORKGROUP); there is no secure channel to check
```
