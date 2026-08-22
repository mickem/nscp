#### About `check_iis_request_queues`

`check_iis_request_queues` reports one record per HTTP.sys request queue from
the "HTTP Service Request Queues" performance counters. Queues are usually
named after the application pool they feed; requests wait here when no worker
is available, and once a queue hits its limit HTTP.sys rejects new requests
with 503 without the application ever seeing them.

The defaults track HTTP.sys' default per-queue limit of 1000: **warning** at
`queue_length > 800`, **critical** at `queue_length > 1000` (adjust when
`queueLength` is raised in the pool configuration).

`rejected` is cumulative, so alert on it going non-zero (`warning=rejected >
0`) after a deploy, or graph its rate; a growing `max_age` with a modest
`queue_length` points at a hung worker rather than overload.
