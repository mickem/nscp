#### About `submit_gearman`

`submit_gearman` files a **passive** check result into a Mod-Gearman result
queue. The core's result thread reads whatever arrives on `check_results` and
files a result marked `type=passive` exactly as if it had come from the external
command file — which is all `send_gearman` (or `nagios-send-gearman`) does.

In a Mod-Gearman installation this replaces NSCA: the results travel over the
same outbound connection to the same gearmand the checks already come from, so
there is one daemon fewer to run, no inbound port and no mcrypt.

The usual way to use it is to route results rather than call it by hand. Give a
scheduled check `channel = GEARMAN`, or add `GEARMAN` to the channels a check
reports on, and each result is submitted as it is produced. A direct call is
mainly useful for verifying that the key, the queue and the host name match what
the core expects.

##### Three things must match the core

Gearman negotiates nothing, and neither does Mod-Gearman's envelope. A mismatch
is not an error on either side — the result simply never appears.

- **`key`** — the shared password from the core's `module.conf` (`key=`). At
  most 32 bytes are used, as in mod_gearman. Leave `encryption` on: with it off
  the payload is plain base64 and the core must additionally have
  `accept_clear_results=yes`.
- **`queue`** — `check_results` unless the core's `module.conf` names a
  different result queue. A result on a queue nobody reads is discarded by
  gearmand without a word.
- **`hostname`** — the name the result is filed under, which has to be the
  `host_name` the core knows this host by. It defaults to `auto`, the operating
  system's own name; set it explicitly when the core spells the host
  differently.

If results never appear on the core, check these three before anything else.

##### What the shared key is and is not

The envelope is AES-256 in ECB mode with the password used directly as the key.
It hides the content and nothing else: it does not authenticate the sender and
it does not prevent replay, so anyone holding the key — or anyone at all with
`encryption = false` — can forge results for any host the core monitors. That is
the protocol's design, and this module cannot fix it. Treat access to gearmand
and to the key as access to the monitoring data itself, and keep both on a
network you trust.

##### Naming the service

The result's `service_description` is the check's alias, or its command name
when no alias is given. The alias `host_check` is the one exception: it files a
host result, which is a result with no `service_description` at all.

##### The other half of this module

The same module also runs the checks a core queues — that is the worker half,
configured under `/settings/gearman/worker`. The two are independent
deployments: an installation that only wants the passive channel configures
`/settings/gearman/client` and leaves the worker section empty, and one that
only wants the worker does the reverse.
