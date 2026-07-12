#### About `check_group_members`

`check_group_members` reports the membership of a **local group** (default
`Administrators`) via `NetLocalGroupGetMembers`, and — given an expected
allow-list — alerts on **membership drift**: any member that is not on the list.
This is the standard "who is in the local Administrators group, and did that
change?" check.

Keywords (one row per member):

| Keyword    | Description                                                          |
|------------|---------------------------------------------------------------------|
| `group`    | The local group being checked                                       |
| `member`   | Member as `DOMAIN\name`                                             |
| `name`     | Member name component                                               |
| `domain`   | Member domain component (`BUILTIN`, machine name, AD domain, …)     |
| `sid`      | Member SID                                                          |
| `type`     | Member type: `user`, `group`, `wellknown`, `alias`, `deleted`, …    |
| `expected` | `1` if the member is on the `expected=` allow-list (or none given)  |

Options:

| Option      | Description                                                                                 |
|-------------|---------------------------------------------------------------------------------------------|
| `group`     | Local group to inspect (default `Administrators`)                                            |
| `expected`  | An allowed member (repeatable), matched against `DOMAIN\name` or the bare name               |

Default: **CRITICAL** on drift (`expected = 0`). When no `expected=` list is
given, every member is treated as expected, so the check simply **lists** the
group's members and is OK. empty-state is **OK**; a group that does not exist is
reported as an error.

`expected=` entries match case-insensitively against either the full
`DOMAIN\name` (e.g. `BUILTIN\Administrators`, `MYPC\localadmin`) or just the
name (`Administrator`, `Domain Admins`).
