#### About `list_cache`

`list_cache` returns the keys currently held in the SimpleCache module's
in-memory cache, as a comma-separated list.

It is a debugging aid rather than a monitoring check. When
[`check_cache`](#check_cache) reports `Entry not found`, the usual cause is that
the key the reader assembles does not match the key the writer stored under —
different `primary index` expression, a host name that arrived in a different
form, an alias that was empty on submission. Listing the keys shows what is
actually there, and the mismatch is normally obvious at a glance.

It takes no options and reports every key, so on a busy relay the output can be
long.

Note that the returned **status is always UNKNOWN**, including on a successful
listing: the command reports data rather than a verdict, and has no notion of a
healthy key set to compare against. Read the message, not the status, and do not
wire this command up as an alerting check.
