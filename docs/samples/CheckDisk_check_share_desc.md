#### About `check_share`

`check_share` inspects the host's **SMB shares** (Windows `Win32_Share`). It has
two modes:

- **List mode** (no `share=`): enumerate every share on the host — useful for
  inventory or `show-all` output.
- **Required mode** (one or more `share=<name>`): verify that specific shares
  exist. Each requested share becomes a row with an `exists` flag, and the check
  is **CRITICAL** when a required share is missing (default `crit=not exists`).

This complements `check_uncpath`, which checks a *remote* share's free space, 
with the server-side "are my shares published?" view.

Each share (or requested share name) becomes one row in the filter.

Defaults: `crit=not exists` (inert in list mode, since every listed share
exists), empty-state **OK** (a host with no shares is not inherently a problem).
Windows share names are case-insensitive, so `share=public` matches a `Public`
share.
