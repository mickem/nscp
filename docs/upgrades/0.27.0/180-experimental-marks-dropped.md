---
icon: "🧪"
modules: [CheckDisk, CheckNet, CheckNSCP, NSCANgClient]
action: none
---
**Checks that have been around for six months are no longer marked *experimental*.**
Nothing to do on an upgrade: the checks themselves are unchanged. The mark
says that a command's options, filter keywords and output may still change;
these have settled, so `nscp test`, the web UI, the REST `experimental` field
and the reference documentation stop flagging them:

| Module         | No longer experimental                                                    |
|----------------|---------------------------------------------------------------------------|
| `CheckDisk`    | `check_single_file`, `check_disk_io`, `check_disk_health`                 |
| `CheckNet`     | `check_tcp`, `check_dns`, `check_http`, `check_connections`, `check_ntp_offset` |
| `CheckNSCP`    | `check_nscp_update`                                                       |
| `NSCANgClient` | the whole module, and with it `submit_nsca_ng`                            |

Everything added more recently keeps its mark. If you consume the REST
`experimental` field, expect it to read `false` for the commands above.
