**Emit each counter's declared maximum as its own series (`mode=extract`):**

The original counters are kept and the extracted ones are added alongside,
renamed by substituting `used` with `size` everywhere it appears in the label.

```
xform_perf command=check_drivesize mode=extract field=max "replace=used=size"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.25672GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used'=29.77734MB;36.98125;41.6039;0;46.22656 '/opt/env-runner used %'=64%;80;90;0;100 '/ size'=251.97227GB;201.57782;226.77505;0;251.97227 '/ size %'=100%;80;90;0;100 '/opt/claude-code size'=229.94921MB;183.95937;206.95429;0;229.94921 '/opt/claude-code size %'=100%;80;90;0;100 '/opt/env-runner size'=46.22656MB;36.98125;41.6039;0;46.22656 '/opt/env-runner size %'=100%;80;90;0;100
```

The graph now has a `size` line to draw `used` against.

**`field=min` works the same way; every other field is a silent no-op:**

Despite what the option help suggests, `value`, `warn` and `crit` add nothing —
the performance data comes back exactly as the wrapped check produced it, with
no error to tell you so.

```
xform_perf command=check_drivesize mode=extract field=crit "replace=used=used_crit"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.25701GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used'=29.77734MB;36.98125;41.6039;0;46.22656 '/opt/env-runner used %'=64%;80;90;0;100
```

**Pin percentage counters to a 0-100 axis (`mode=minmax`):**

Only counters whose unit is `%` are touched; everything else is left alone.
`check_drivesize` already declares 0/100 on its percentage counters, so here the
transformation is a no-op — it matters for checks that do not.

```
xform_perf command=check_drivesize mode=minmax
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.2454GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
```

**An unrecognised mode is an error:**

```
xform_perf command=check_drivesize mode=bogus
UNKNOWN: WARNING /opt/claude-code: 202.746MB/229.949MB used
UNKNOWN: Invalid mode specified	...
```
