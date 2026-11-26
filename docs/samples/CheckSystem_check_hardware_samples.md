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

**Linux, agent running as root (the SMBIOS table gives the per-DIMM inventory):**

```
check_hardware
OK: Dell Inc. PowerEdge R760 (Rack Mount Chassis), serial=ABC1234, 8 memory module(s), 256GB|'hardware_memory'=274877906944 'hardware_modules'=8

check_hardware "detail-syntax=${module_list} (slots=${slots}, speed=${memory_speed}MHz, chassis=${chassis})"
OK: A1: 32GB@4400MHz; A2: 32GB@4400MHz; A3: 32GB@4400MHz; A4: 32GB@4400MHz; B1: 32GB@4400MHz; B2: 32GB@4400MHz; B3: 32GB@4400MHz; B4: 32GB@4400MHz (slots=16, speed=4400MHz, chassis=Rack Mount Chassis)

check_hardware "detail-syntax=${vendor} ${model} uuid=${uuid} asset=${asset_tag} memory=${memory} (${memory_source})"
OK: Dell Inc. PowerEdge R760 uuid=4C4C4544-0042-3510-8051-B4C04F4B4E32 asset=ASSET-42 memory=256GB (smbios)
```

**Linux, unprivileged agent (serials, UUID and the module inventory need root):**

The module count is `unknown`, so `warn=modules < 8` stays quiet instead of
firing, and `memory` is the kernel's memory-block total.

```
check_hardware "warn=modules < 8"
OK: Dell Inc. PowerEdge R760 (Rack Mount Chassis), serial=, unknown memory module(s), 16GB|'hardware_memory'=17179869184

check_hardware "detail-syntax=${vendor} ${model} serial=${serial} uuid=${uuid} modules=${modules} memory=${memory} (${memory_source})"
OK: Dell Inc. PowerEdge R760 serial= uuid= modules=unknown memory=16GB (sysfs)
```

**Linux container or microVM without DMI (the documented no-data result, status UNKNOWN):**

```
check_hardware
No hardware inventory available: this host exposes no DMI/SMBIOS data (no /sys/class/dmi/id and no SMBIOS table; typical of containers and microVMs)
```
