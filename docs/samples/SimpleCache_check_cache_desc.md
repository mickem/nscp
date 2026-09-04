#### About `check_cache`

`check_cache` looks up a previously submitted result in the SimpleCache module's
in-memory cache and returns it **verbatim** — the same status, message and
performance data that were originally submitted.

This makes it different from every other check in NSClient++: it does not
measure anything. It is the read side of a store-and-forward arrangement, where
some other agent, script or scheduled check submits results into the `CACHE`
channel and a monitoring server later polls for them. That is how you get
passive results into a system that only knows how to poll, and how a host behind
a firewall can push results that the poller pulls from a reachable relay.

Because the cached response is returned as-is, the filter and syntax options
other checks take do not apply here.

##### Addressing an entry

Entries are stored under a key built from the `primary index` expression
configured on the module, which defaults to `${alias-or-command}` and can
combine `${command}`, `${host}`, `${channel}`, `${alias}`, `${message}` and
`${result}`.

You can either name the key outright with `key=` — which is used as given, not
parsed — or let the check assemble it from the same parts the writer used, by
passing `host=`, `command=`, `channel=` and `alias=`. Mixing the two is not
possible: an explicit `key=` wins and the individual parts are ignored.

##### When nothing is cached

`not-found-msg=` (default `Entry not found`) and `not-found-code=` (default
`unknown`) decide what a miss looks like. Leaving the default UNKNOWN is usually
right — it is distinguishable from a genuine OK, so a monitoring server can tell
"nobody has reported" apart from "reported healthy".

The cache is **held in memory only**. It does not survive an agent restart, and
a freshly started agent answers every lookup with the not-found result until
submissions start arriving again. Size the submitting side's interval so a
restart window does not read as a fleet of failures.

Use [`list_cache`](#list_cache) to see which keys are actually present, which is
the fastest way to debug a key expression that does not match.

The legacy alias `CheckCache` is accepted for backwards compatibility.
