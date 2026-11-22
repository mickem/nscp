---
icon: "⏱️"
modules: [filters, CheckNet, CheckDisk, CheckSecurity, CheckMSSQL, CheckMySQL, CheckSystem, CheckSystemUnix]
action: conditional
---
**Units in `warning`, `critical` and `filter` are read whole, and every
duration keyword takes them.** A unit used to be one letter: `500ms`, `5min`
and `10GB` did not parse, and the conversions read the letter they got by
itself, so a unit nothing knew passed through as a bare number. Now:

- **Units of more than one letter parse:** `time > 500ms`, `age > 30min`,
  `free < 4GB`, `size > 1GiB`. Sizes take `b`, and `k`, `m`, `g`, `t`, `p`, `e`
  alone or followed by `b` or `ib`, all 1024-based as before.
- **Keywords that count milliseconds take a duration**, and a bare number stays
  milliseconds (`time > 200` is unchanged):

  | Check | Keywords |
  |---|---|
  | `check_ping` | `time` |
  | `check_tcp`, `check_ssh` | `time` |
  | `check_http`, `check_dns`, `check_radius` | `time` |
  | `check_ntp_offset` | `offset`, `offset_signed`, `time`, `root_delay`, `root_dispersion` |
  | `check_tomcat_status` | `processing_time`, `max_time` |
  | `check_disk_write` | `write_time`, `read_time`, `total_time` |

- **Keywords that count whole days take `d` and `w`**, and a bare number stays
  days: `expires_in` and `expires_in_days` (`check_certificate`), `expires_in`
  (`check_domain`), `signature_age`, `quick_scan_age` and `full_scan_age`
  (`check_defender`), `grace_days` (`check_activation`). A unit below a day is
  refused. `check_certificate`'s `expires_in_sec` takes durations.
- **A unit a keyword does not take is an error**, not a bare number:
  `time > 5x`, `free < 5mo`, `size > 5%`, `written > -5x`.

**What to do:** only if a threshold used a unit on one of the keywords above,
or a unit that is not one. On these keywords it was compared against 0 and
fired on every check; it now means what it says, so it may stop firing. A unit
that is not one (`written > -5x` was five seconds) is now reported in the
debug log, and the check ends UNKNOWN once evaluation errors do.
