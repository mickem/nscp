#### About `remote_nscpforward`

`remote_nscpforward` is the module's relay command: it is meant to pass a
request through to a remote NSClient++ agent over the NSCP protocol **as-is**,
without interpreting it, so that this host can act as a proxy for agents a
monitoring server cannot address directly.

##### The registered name does not dispatch

The client framework selects how to handle a command by matching its name
(`include/client/command_line_parser.cpp`): the relay path is taken for names
that **start with `forward_` or end with `_forward`**, and the query, exec and
submit paths for `check_*` / `*_query`, `exec_*` and `submit_*` respectively.

`remote_nscpforward` matches none of those — it ends in `nscpforward`, not
`_forward` — so it falls through to the final `else` and the call is answered
with:

```
remote_nscpforward not found
```

The command is registered and appears in the reference, but **invoking it does
nothing useful in this release**. The sibling `nrpe_forward` in
[NRPEClient](NRPEClient.md#nrpe_forward) does end in `_forward` and is
dispatched correctly.

##### What to use instead

For an NSCP relay today, register the module's own `fallback` handler on the
target, which routes unmatched requests through the same client without going
via this command name. Where an explicit command is needed and the far end is
NSClient++, [`check_remote_nscp`](#check_remote_nscp) forwards a named check and
returns its full structured result.
