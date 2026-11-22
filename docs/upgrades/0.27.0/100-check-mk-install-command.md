---
icon: "🧰"
modules: [CheckMKServer, CheckMKClient]
action: none
---
**`nscp mk install` sets up the Checkmk agent from the command line.**
Like `nscp nrpe install` for NRPE, one command enables `CheckMKServer`,
`CheckSystem` and `CheckDisk`, sets the port and the hosts allowed to fetch
the agent dump, and adds or removes local checks and MRPE entries; `nscp mk
show` prints the result:

```commandline
nscp mk install --allowed-hosts 10.0.0.5 --local "CPU Load=check_cpu warn=load>80 crit=load>95"
nscp mk show
```

It starts from what is configured, so a re-run only changes what it is
given. `--allowed-hosts` is written to `/settings/check_mk/server` alone, so
the lists of NRPE and the other listeners are left as they are. An MRPE name
with a space is refused, since Checkmk cannot parse one. Nothing changes for
an existing setup until the command is run; see
[Checkmk Agent Integration](../scenarios/check-mk.md#quick-setup-with-nscp-mk-install).
