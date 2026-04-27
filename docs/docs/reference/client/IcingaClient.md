# IcingaClient

Submits NSClient++ check results to an [Icinga 2](https://icinga.com/) server
using the Icinga 2 [REST API](https://icinga.com/docs/icinga-2/latest/doc/12-icinga2-api/).

The module is the HTTP/JSON counterpart of `NRDPClient` and `NSCAClient`: each
configured target translates a passive check result into a
`POST /v1/actions/process-check-result` call against an Icinga 2 endpoint.

## Enable module

Add `IcingaClient = enabled` to the `[/modules]` section in `nsclient.ini`:

```
[/modules]
IcingaClient = enabled
```

## Configuration

Configure the client section and at least one target:

```
[/settings/icinga/client]
hostname = auto
channel  = ICINGA

[/settings/icinga/client/targets/default]
address          = https://icinga.example.com:5665
username         = nscp-api
password         = secret
ensure objects   = true
host template    = generic-host
service template = generic-service
check command    = dummy
verify mode      = peer-cert
ca               = /etc/ssl/certs/ca.pem
```

| Key                | Default            | Description                                                                                                                                                            |
|--------------------|--------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `address`          | (required)         | Full Icinga 2 base URL, e.g. `https://icinga.example.com:5665`. The default port is `5665` for both `http://` and `https://`.                                          |
| `username`         |                    | Icinga 2 API user used for HTTP basic authentication.                                                                                                                   |
| `password`         |                    | Icinga 2 API password.                                                                                                                                                  |
| `timeout`          | `30`               | Connect/read timeout in seconds.                                                                                                                                        |
| `ensure objects`   | `false`            | When `true` the client will probe (`GET`) the host/service object and create it (`PUT`) if missing. Required for hosts that are not pre-defined in the Icinga config.   |
| `host template`    | `generic-host`     | Comma-separated list of Icinga 2 templates used when auto-creating the host object.                                                                                     |
| `service template` | `generic-service`  | Comma-separated list of Icinga 2 templates used when auto-creating the service object.                                                                                  |
| `check command`    | `dummy`            | Icinga 2 `check_command` set on auto-created service objects. Use `dummy` for purely passive services.                                                                  |
| `check source`     | (local hostname)   | Override for the `check_source` field reported to Icinga 2.                                                                                                             |
| `verify mode`      |                    | TLS peer verification mode (`none`, `peer`, `peer-cert`, ...). For self-signed Icinga setups use `peer-cert` with a custom `ca`, or `none` to disable verification.     |
| `ca`               |                    | Path to the certificate authority bundle used to verify the Icinga 2 server certificate.                                                                                |
| `tls version`      | (system default)   | TLS version to negotiate (`1.0`, `1.1`, `1.2`, `1.3`).                                                                                                                  |

## Behaviour notes

* For service results the Icinga 2 `exit_status` (0..3) is forwarded as-is.
* For host results (`alias = host_check`) the value is clamped to `0` or `1`,
  since the Icinga 2 API rejects host `exit_status` values outside that range
  with HTTP 400.
* Performance data is split on whitespace (with single-quoted labels honoured)
  and submitted as a JSON array to `performance_data`.
* When `ensure objects = true`, the client probes objects with `GET` first and
  only `PUT`s missing ones. This is required because Icinga 2 returns HTTP 500
  (not 409) on duplicate creates, so the status code alone is not enough to
  distinguish a duplicate from a real error.

## Submit from the command line

```
nscp client --module IcingaClient --command submit \
    --target default \
    --command-name check_disk \
    --result 0 \
    --message "OK | used=42%;80;90;0;100"
```

## Channel

The module registers itself on the `ICINGA` channel by default. Other modules
(for example `Scheduler` or `CheckSystem`) can forward results to Icinga by
setting their `channel` to `ICINGA`.
