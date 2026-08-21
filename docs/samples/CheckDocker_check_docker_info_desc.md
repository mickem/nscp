#### About `check_docker_info`

`check_docker_info` checks the docker daemon itself: it reads `/info` from the
local daemon socket and reports the server version, host name and the
container/image counts. A responding daemon is **OK** unless you threshold the
counts; an unreachable daemon is **UNKNOWN** with the transport error — which
makes this the natural "is docker itself healthy" companion to per-container
`check_docker` checks.

Count keywords used in thresholds are emitted as performance data.
