#### About `check_iis_app_pools`

`check_iis_app_pools` reports one record per IIS application pool from the
`APP_POOL_WAS` performance counters (state, uptime, recycles). When the IIS
WMI provider ("IIS Management Scripts and Tools") is installed, the records
are enriched from `root\WebAdministration`: each pool gains its `auto_start`
configuration flag, and pools that WAS has no counter instance for yet (never
started since boot) are added with state `unknown` so they cannot hide.

The default **critical** expression is
`state != 'running' and auto_start != 0`: an auto-start pool that is not
running alerts, a pool an administrator stopped on purpose (`auto_start` = 0)
stays quiet. Without the WMI provider `auto_start` is `-1` for every pool, so
every non-running pool alerts.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword      | Description                                                          |
|--------------|----------------------------------------------------------------------|
| `pool`       | Name of the application pool                                         |
| `state`      | `running`, `disabled`, `disabling`, `shutdown_pending`, `delete_pending`, `initialized`, `uninitialized` or `unknown` |
| `state_id`   | Raw `APP_POOL_WAS` state value (3 = running)                         |
| `uptime`     | Seconds since the pool last started                                  |
| `recycles`   | Pool recycles since WAS started (cumulative — alert on growth)       |
| `auto_start` | `1`/`0` from the pool configuration, `-1` when WMI is unavailable    |

`recycles` counts since WAS started, so a recycle *storm* shows as a high and
climbing value combined with a low `uptime`; `warning=recycles > 10 and
uptime < 600` is a useful storm signature.
