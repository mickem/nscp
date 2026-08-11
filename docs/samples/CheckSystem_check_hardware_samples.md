**Default check (inventory line with memory/module perf):**

```
check_hardware
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB|'hardware_memory'=34359738368;0;0 'hardware_modules'=2;0;0
```

**Pin the expected serial (CRITICAL when the box was replaced or re-imaged):**

```
check_hardware "crit=serial != 'ABC1234'"
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB

check_hardware "crit=serial != 'XYZ0000'"
CRITICAL: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB
```

**Detect a dropped DIMM (module count or total capacity shrank):**

```
check_hardware "warn=modules < 2" "crit=memory < 16G"
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB|'hardware_memory'=34359738368;0;17179869184 'hardware_modules'=2;2;0
```

**Enforce machine class (no laptops in the server fleet):**

```
check_hardware "warn=chassis like 'Laptop' or chassis like 'Notebook'"
WARNING: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB
```

**Per-DIMM inventory and socket usage:**

```
check_hardware "detail-syntax=${module_list} (slots=${slots}, speed=${memory_speed}MHz, chassis=${chassis})"
OK: DIMM A: 16GB@5600MHz; DIMM B: 16GB@5600MHz (slots=2, speed=5600MHz, chassis=Notebook)
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_hardware --argument "crit=serial != 'ABC1234'"
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB
```
