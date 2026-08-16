#### About `check_file_security`

`check_file_security` inspects the owner and the access control list of files,
folders and service binaries, and alerts when something can be written by
someone who should not be able to: a world-writable data directory, a service
binary whose owner has changed, a folder that inherits `Everyone: Modify` from
its parent.

Owner and DACL are read with `GetNamedSecurityInfo`; the ACL is then walked entry
by entry (`GetAce`) so that **inherited** entries are judged exactly like the
ones set on the object itself — inheritance is how most of these problems arrive.
Trustees are resolved with `LookupAccountSid`. With `service=` the image path is
first read from the service control manager, so the check follows the binary the
service actually runs.

A trustee counts as having write access when its allow entry carries any of
`FILE_WRITE_DATA`, `FILE_APPEND_DATA`, `DELETE`, `FILE_DELETE_CHILD`,
`WRITE_DAC`, `WRITE_OWNER`, `GENERIC_WRITE` or `GENERIC_ALL`. The harmless
`FILE_WRITE_EA`/`FILE_WRITE_ATTRIBUTES` bits, which Windows hands out widely, do
not count on their own. A deny entry covering the same bits cancels the grant.

These trustees are always allowed to write: `NT AUTHORITY\SYSTEM`,
`BUILTIN\Administrators`, `NT SERVICE\TrustedInstaller` and `CREATOR OWNER`.
Write access held by `Everyone`, `Authenticated Users`, `BUILTIN\Users`,
`BUILTIN\Guests`, `BUILTIN\Power Users` or `ANONYMOUS LOGON` makes the path
**world writable**; any other trustee outside the allow-list is simply
unexpected. All matching is done on the SID first, so it works on a localized
Windows where these groups have translated names.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `path` | string | The inspected file or directory. |
| `service` | string | Service the path was resolved from (empty for a `path=` entry). |
| `owner` | string | Owner as `DOMAIN\name`, or the SID when it cannot be resolved. |
| `owner_sid` | string | Owner SID. |
| `writable` | string | Comma separated trustees holding write access. |
| `unexpected` | string | The subset of `writable` that is not allow-listed. |
| `dacl` | string | The whole DACL as `trustee(rights)`; `!` prefixes a deny entry, `~` an inherited one. Rights are `F` or a subset of `R`ead, `W`rite, e`X`ecute, `D`elete, `P`ermissions. |
| `error` | string | Why the security descriptor could not be read (empty when it could). |
| `state` | string | One-line verdict used by the default detail syntax. |
| `exists` | bool | True when the path exists. |
| `readable` | bool | True when the security descriptor could be read. |
| `is_dir` | bool | True when the path is a directory. |
| `owner_expected` | bool | True when the owner is on the `expected-owner` list (or no list was given). |
| `unexpected_write` | bool | True when a trustee outside the allow-list holds write access. |
| `world_writable` | bool | True when an untrusted group (see above) holds write access. |
| `ace_count` | int | Number of entries in the DACL (perfdata). |

Options:

| Option | Repeatable | Meaning |
|---|---|---|
| `path` (or `file`) | yes | File or directory to inspect. |
| `service` | yes | Service whose binary is inspected. |
| `expected-owner` | yes | An acceptable owner, matched by SID, `DOMAIN\name` or bare name. When omitted the owner is only reported. |
| `allow-write` | yes | A trustee allowed to hold write access, in addition to the four always-allowed ones. |

At least one `path` or `service` is required; without either the check reports
UNKNOWN rather than a silent OK.

Default thresholds: **critical** when
`exists = 0 or readable = 0 or owner_expected = 0 or world_writable = 1` and
**warning** when `unexpected_write = 1`. Reading a security descriptor requires
the agent to have permission to do so, which is why an unreadable descriptor is
treated as a finding instead of a pass. **Windows only.**
