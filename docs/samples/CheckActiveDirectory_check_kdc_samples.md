**Default check (domain-joined; probes the discovered KDC):**

```
check_kdc
OK: dc01.example.com: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED (2ms)|'dc01.example.com'=2ms;1000
```

`KDC_ERR_PREAUTH_REQUIRED` is the *healthy* answer: the KDC processed the
request and asked for pre-authentication.

**Probe specific KDCs explicitly (works from any machine, no domain join needed):**

```
check_kdc server=dc01.example.com server=dc02.example.com realm=EXAMPLE.COM
OK: all 2 KDC(s) are responding|'dc01.example.com'=2ms;1000 'dc02.example.com'=3ms;1000
```

**KDC down (nothing answering on the port):**

```
check_kdc server=dc01.example.com realm=EXAMPLE.COM
CRITICAL: dc01.example.com: connect failed: No connection could be made because the target machine actively refused it (2028ms)|'dc01.example.com'=2028ms;1000
```

**Something answered, but it does not speak Kerberos:**

```
check_kdc server=dc01.example.com realm=EXAMPLE.COM
CRITICAL: dc01.example.com: invalid response (5ms)|'dc01.example.com'=5ms;1000
```

**Tighten the latency alert (Kerberos slowness precedes logon storms):**

```
check_kdc "warning=time > 200" "critical=responding = 0 or time > 2000"
OK: dc01.example.com: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED (2ms)|'dc01.example.com'=2ms;200;2000
```

**Custom output with the raw error code:**

```
check_kdc "detail-syntax=${kdc} port ${port} realm ${realm}: ${response} code=${error_code}"
OK: dc01.example.com port 88 realm EXAMPLE.COM: KRB-ERROR KDC_ERR_PREAUTH_REQUIRED code=25
```

**On a machine that is not domain-joined (no server= given):**

```
check_kdc
Failed to locate a KDC (is this machine domain-joined?): 54b: The specified domain either does not exist or could not be contacted. Specify server= and realm=.
```
