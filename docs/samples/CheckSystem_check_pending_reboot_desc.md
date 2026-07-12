#### About `check_pending_reboot`

`check_pending_reboot` answers a question no single Windows API does: **is this
machine waiting for a reboot, and why?** A pending reboot is signalled
independently by several subsystems, so the check reads each one and reports the
union. This is the reliable way to catch servers that have applied updates but
will not finish patching until they restart.

The signals inspected (all in the 64-bit registry view, so a 32-bit agent under
WOW64 still reads the native keys):

| Keyword           | Signal (registry source)                                                                                |
|-------------------|---------------------------------------------------------------------------------------------------------|
| `servicing`       | Component Based Servicing queued a reboot (`...\Component Based Servicing\RebootPending` key exists)     |
| `windows_update`  | Windows Update queued a reboot (`...\WindowsUpdate\Auto Update\RebootRequired` key exists)               |
| `file_rename`     | A file replacement awaits reboot (`Session Manager\PendingFileRenameOperations` present and non-empty)   |
| `computer_rename` | The computer was renamed but not rebooted (`ActiveComputerName` ≠ pending `ComputerName`)                |
| `domain_join`     | A domain join / SPN update is queued in `Netlogon` (`JoinDomain` / `AvoidSpnSet` present)                |
| `pending`         | `1` if **any** of the above is set — the aggregate flag most checks threshold on                         |
| `count`           | Number of distinct signals currently set                                                                |
| `reasons`         | Comma-separated human-readable causes (`none` when clear)                                                |
| `message`         | Full status sentence, e.g. `Reboot required: Windows Update`                                             |

The default threshold is `warn=pending = 1` (WARNING whenever a reboot is
pending, no critical). Override it to escalate, to alert only on specific causes
(e.g. `crit=servicing = 1`), or to suppress the default with `warn=none`. The
check always returns a single aggregate row, so there is no empty state.
