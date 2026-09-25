# Integration / scenario tests

Cross-platform replacement for the per-protocol `tests/<proto>/run-test.bat` scripts. The same suites run on Linux (WSL
or native), macOS and Windows under one Jest + TypeScript harness,
using [testcontainers-node](https://node.testcontainers.org/) to drive the per-test Docker images
and [execa](https://github.com/sindresorhus/execa) to spawn the `nscp` CLI.

> **Testing an already-running server (VM / installed service)?** These suites
> spawn their own `nscp`. To instead point a suite at an nscp that is already
> installed and running (a provisioned Azure VM, a package install, a hand-started
> build), use the **live acceptance suite** — `npm run test:live` — documented in
> [`live/README.md`](live/README.md).

## Requirements

- Node.js 20+ and npm (matches `tests/rest/`)
- Docker Desktop (Windows, macOS) or a working Docker daemon (Linux) - or
  `NSCP_SKIP_DOCKER=1`, which is how the CI package jobs run
- A built `nscp` binary

## Quick start

```sh
cd tests
npm install

# Point at your built nscp binary. The harness also auto-detects
# cmake-build-debug-wsl/nscp and cmake-build-debug/nscp at the repo root.
export NSCP_BIN=/abs/path/to/nscp

# Run everything
npm test

# Run a single scenario by path or name
npx jest --runInBand --testPathPattern nrdp

# Run only the docker-free scenarios (the 16 rest-* suites). Useful for CI
# stages that don't have a docker daemon available — the docker-using
# scenarios skip themselves at the describe level.
npm run test:no-docker
# (equivalent to: NSCP_SKIP_DOCKER=1 npm test on POSIX shells)
```

On Windows / PowerShell, set the env var explicitly:

```powershell
$env:NSCP_SKIP_DOCKER = "1"; npm test
```

On Windows:

```cmd
set NSCP_BIN=C:\path\to\nscp.exe
npm test
```

## Coverage

Because the harness spawns a real `nscp` and lets it `dlopen` the real modules,
pointing `NSCP_BIN` at a gcov-instrumented build turns these scenarios into a
coverage report of actual command dispatch and REST argument parsing:

```sh
SUITES=integration tools/coverage/run.sh     # from the repo root
```

That builds `build-coverage/` with `-DNSCP_COVERAGE=ON`, runs this suite against
it and writes `coverage/integration.html`. It works because `NscpInstance.stop()`
stops the daemon with SIGTERM rather than SIGKILL — gcov only flushes its
counters from an `atexit` handler. See the _Coverage reports_ section of
`build.md` for the details and the caveats.

## What runs

Docker-using scenarios (skipped when `NSCP_SKIP_DOCKER=1`):

| File                              | Replaces                                                                                                                                                                                       |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `tests/nrdp-submit.test.ts`       | `tests/nrdp/run-test.bat`                                                                                                                                                                      |
| `tests/nsca-ciphers.test.ts`      | `tests/nsca/run-test.bat`                                                                                                                                                                      |
| `tests/nsca-ng-submit.test.ts`    | `tests/nsca-ng/run-test.bat`                                                                                                                                                                   |
| `tests/smtp-send.test.ts`         | `tests/smtp/run-test.bat`                                                                                                                                                                      |
| `tests/nrpe-tls.test.ts`          | `tests/nrpe/run-test.bat`                                                                                                                                                                      |
| `tests/http_proxy-nrdp.test.ts`   | `tests/http_proxy/run-test.bat`                                                                                                                                                                |
| `tests/check_mk-agent.test.ts`    | `tests/check_mk/run-test.bat`                                                                                                                                                                  |
| `tests/check_mk-site.test.ts`     | `tests/check_mk/run-test-cmk-site.bat`                                                                                                                                                         |
| `tests/icinga-submit.test.ts`     | `tests/icinga/run-test.bat`                                                                                                                                                                    |
| `tests/icinga-client-api.test.ts` | `tests/icinga-client/run-test.bat`                                                                                                                                                             |
| `tests/graphite-submit.test.ts`   | (new) GraphiteClient metrics + submit                                                                                                                                                          |
| `tests/graphite-tls.test.ts`      | (new) GraphiteClient TLS (socat proxy)                                                                                                                                                         |
| `tests/check_nt-client.test.ts`   | (new) NSClientServer vs the real nagios-plugins check_nt, incl. the `allow` gate                                                                                                               |
| `tests/fleet-server-live.test.ts` | (new) the agent against a **real** nsclient-fleet server, built from its newest GitHub release                                                                                                 |
| `tests/gearman-fixtures.test.ts`  | (new) Mod-Gearman: the gearmand image, and Naemon + Nagios Core 4.5 images scheduling checks through gearmand to a stub worker (`src/gearman.ts`); its fixture round-trip block is docker-free |
| `tests/gearman-worker.test.ts`    | (new) Mod-Gearman: the agent's own worker loop, with the test playing the core against the gearmand image                                                                                      |
| `tests/gearman-core.test.ts`      | (new) Mod-Gearman end to end: the agent answering the checks a real Naemon and a real Nagios Core 4.5 schedule, asserted on the core's own status file                                         |

The two gearman suites that only need a **job server** — `gearman-worker` and
`gearman-submit` — also run without docker when `NSCP_GEARMAND=host:port` names
an already-running gearmand (port defaults to 4730):

```sh
gearmand --listen=127.0.0.1 --port=14731 &
NSCP_GEARMAND=127.0.0.1:14731 npx jest --runInBand gearman-worker
```

Docker stays the default — it pins the gearmand version and starts with empty
queues — and the one case an external server cannot serve, restarting the job
server under the agent to prove it reconnects, skips itself. The suites that
need a monitoring core (`gearman-core`, `gearman-proxy`, `gearman-fixtures`)
have no such escape hatch: there is nothing to point them at but the images.

Docker-free scenarios (always run, including in no-docker CI pipelines):

| File                                    | Notes                                                                                        |
| --------------------------------------- | -------------------------------------------------------------------------------------------- |
| `tests/checksystem-commands.test.ts`    | CheckSystem check commands, both OSes                                                        |
| `tests/checknet-commands.test.ts`       | CheckNet tcp/ssh/http/dns/web checks                                                         |
| `tests/checkdisk-commands.test.ts`      | CheckDisk drive/IO checks, both OSes                                                         |
| `tests/checkdisk-unix.test.ts`          | CheckDisk file/drive checks (Linux)                                                          |
| `tests/checkmssql-commands.test.ts`     | CheckMSSQL contract tests (Windows); a docker-gated block adds live SQL Server 2022 coverage |
| `tests/metrics-realtime.test.ts`        | Metrics + real-time filters, both OSes                                                       |
| `tests/rest-aliases-v2.test.ts`         | CheckHelpers alias listing                                                                   |
| `tests/rest-api-discovery.test.ts`      | `/api`, `/api/v1`, `/api/v2`, isalive                                                        |
| `tests/rest-auth.test.ts`               | Login + all auth schemes                                                                     |
| `tests/rest-events.test.ts`             | events_controller GET / DELETE                                                               |
| `tests/rest-index.test.ts`              | StaticController fallback                                                                    |
| `tests/rest-info.test.ts`               | `/api/v2/info` shape                                                                         |
| `tests/rest-legacy-auth-icinga.test.ts` | Icinga UA-allowlisted query auth                                                             |
| `tests/rest-legacy-query.test.ts`       | Pre-v1 `/query/<cmd>` endpoint                                                               |
| `tests/rest-log.test.ts`                | logs CRUD + `/logs/since`                                                                    |
| `tests/rest-metadata.test.ts`           | metadata_controller                                                                          |
| `tests/rest-modules-v1.test.ts`         | modules lifecycle (v1)                                                                       |
| `tests/rest-modules-v2.test.ts`         | modules lifecycle (v2)                                                                       |
| `tests/rest-permissions.test.ts`        | Role gating on `/modules`                                                                    |
| `tests/rest-queries-v1.test.ts`         | queries × execute × json/nagios/text                                                         |
| `tests/rest-queries-v2.test.ts`         | queries v2 of the above                                                                      |
| `tests/rest-settings.test.ts`           | settings GET / PUT / DELETE                                                                  |

The Checkmk end-to-end test (`check_mk-site.test.ts`) pulls a ~500MB image and is also gated by `RUN_CMK_SITE_TEST=1`
(must be set _and_ docker must not be skipped).

`fleet-server-live.test.ts` is the one suite that talks to something outside this repo. Every other fleet suite drives a
fake server written in node; this one runs the real `nsclient-fleet` so that a change to the agent/server protocol fails
here instead of on someone's machine (it exists because exactly that happened — the bundle signature changed shape and
every fake kept passing). It resolves the **newest GitHub release** of `mickem/nsclient-fleet-server` and builds a small
image around that release's musl binary, rather than pulling `ghcr.io/mickem/nsclient-fleet:latest`, because the
published image can lag the release by a protocol version. Knobs:

| Variable                    | Effect                                                                        |
| --------------------------- | ----------------------------------------------------------------------------- |
| `NSCP_FLEET_SERVER_VERSION` | Pin the release (`0.1.0`) instead of asking the GitHub API                    |
| `NSCP_FLEET_SERVER_IMAGE`   | Skip the build and run this image (e.g. one built from a server working tree) |
| `GITHUB_TOKEN`              | Used for the release lookup when set; the anonymous API allows 60 calls/hour  |

It publishes the server on host port **19443** (fixed, because the server must be told its own address before it starts),
so a local fleet server on 9443 does not collide. The built image is cached as `nscp-it/nsclient-fleet:<version>`.

`gearman-fixtures.test.ts` is the first step of the GearmanClient (Mod-Gearman worker) plan. Its docker-free block
decrypts the payloads captured from both Mod-Gearman flavours (`modules/GearmanClient/fixtures/`, regenerated with the
`capture.sh` there) and proves the TypeScript envelope in `src/gearman.ts` reproduces them byte for byte; the docker
blocks build `Dockerfiles/gearmand.Dockerfile`, `naemon-gearman.Dockerfile` (ConSol Labs packages) and
`nagios-gearman.Dockerfile` (Nagios Core and nagios-mod-gearman built from their release tarballs, a couple of minutes
on a cold cache) and drive a real core through gearmand to a stub worker in the test. To run the core block against a
core you started yourself instead (for example `Dockerfiles/entrypoints/gearman-core.sh` run natively), set
`NSCP_GEARMAN_LIVE=<name>:<gearmand port>:<status.dat path>`.

`gearman-worker.test.ts` is the third step: the GearmanClient module answering for real. It needs only the gearmand
image - the test itself plays the core, putting encrypted jobs on the hostgroup queue and reading the answers off the
result queue - so it is the fast, deterministic tier where the worker's edge cases live (host binding, `max age`,
timeouts, the wrong key, unencrypted payloads, the refusals that keep a misconfigured worker from starting, and
reconnecting after gearmand restarts). It publishes gearmand on host port **14731**, fixed so the reconnect case
survives a container restart.

`gearman-core.test.ts` is the fourth step: the same agent against the real cores, once per image. The core containers
schedule the checks (`check_always_ok check_ok …`, `check_ok message=hello`, a collector-backed `check_cpu`, and a
`check_slow` external script the agent supplies so one check overruns the job's timeout), and the assertions read the
core's own `status.dat` out of the container - `plugin_output`, `performance_data`, `current_state`, `check_type=0` for
an active result and a `last_check` newer than the test. That file is the only uniform probe: Nagios Core has no REST
API, and Naemon writes the same format. The suite publishes gearmand on host port **14732**, again fixed, for the case
that restarts the whole core container and expects the agent to re-register and keep answering.

The MSI tests (`tests/msi/`) stay Windows-only and are not part of this harness.

## How the fixtures work

`src/nscp.ts` — `NscpInstance` writes a fresh `nsclient.ini` in an os.tmpdir() directory and passes `--settings <path>`
to every `nscp` call, so tests never touch the user's real install. `start()` spawns `nscp test`; `stop()` kills it on
teardown.

`src/docker.ts` — re-exports testcontainers' `GenericContainer` / `Wait` plus `hostGatewayExtraHosts()` (Linux needs
`host.docker.internal:host-gateway` to be added explicitly; Docker Desktop provides it for free) and a `dockerRunOnce()`
helper for one-shot client images.

`src/tls.ts` — generates the CA + server + client certs that NRPE needs via `node-forge`, dropping the openssl CLI
dependency.

`src/files.ts` — `fileContains` / `anyFileContains` replace
`findstr /s /c:` over spooled result files.

`src/platform.ts` — the one place a suite asks which OS it is on: `onWindows`,
`onLinux`, `onDarwin` and `onUnix` (Linux or macOS), with `describeOn*` / `itOn*`
for a block that runs on one of them and `describeIf` / `itIf` for any other
condition. A case that reads procfs, talks to systemd or expects `dpkg` is
`onLinux`, not `onUnix`. `describeWithModules("CheckSystem")` gates a suite on
the modules the macOS build does not carry yet; the port that adds a module
removes it from the list there, and every gate flips at once.

## Formatting

The harness uses Prettier with TypeScript/Jest-friendly defaults (2-space indent, double quotes, trailing commas,
100-char line width — see `.prettierrc.json`).

```sh
# Reformat every .ts / .js / .json / .md file under tests/
npm run format

# CI-style check — exits non-zero if anything would change.
npm run format:check
```

`node_modules/`, generated artifacts, and the legacy `msi/` and `socket/` subprojects are excluded via
`.prettierignore`.

## Cleanup

testcontainers automatically stops and removes started containers on test exit (including on crashes), and the nscp
instance's `stop()` is wired into `afterAll`. There should be no leftover state. If you cancel a test manually, sweep
with:

```sh
docker ps -a --filter "label=org.testcontainers" -q | xargs -r docker rm -f
```
