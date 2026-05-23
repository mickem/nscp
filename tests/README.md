# Integration / scenario tests

Cross-platform replacement for the per-protocol `tests/<proto>/run-test.bat` scripts. The same suites run on Linux (WSL
or native) and Windows under one Jest + TypeScript harness,
using [testcontainers-node](https://node.testcontainers.org/) to drive the per-test Docker images
and [execa](https://github.com/sindresorhus/execa) to spawn the `nscp` CLI.

## Requirements

- Node.js 20+ and npm (matches `tests/rest/`)
- Docker Desktop (Windows) or a working Docker daemon (Linux)
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

## What runs

Docker-using scenarios (skipped when `NSCP_SKIP_DOCKER=1`):

| File                                  | Replaces                               |
|---------------------------------------|----------------------------------------|
| `tests/nrdp-submit.test.ts`           | `tests/nrdp/run-test.bat`              |
| `tests/nsca-ciphers.test.ts`          | `tests/nsca/run-test.bat`              |
| `tests/nsca-ng-submit.test.ts`        | `tests/nsca-ng/run-test.bat`           |
| `tests/smtp-send.test.ts`             | `tests/smtp/run-test.bat`              |
| `tests/nrpe-tls.test.ts`              | `tests/nrpe/run-test.bat`              |
| `tests/http_proxy-nrdp.test.ts`       | `tests/http_proxy/run-test.bat`        |
| `tests/check_mk-agent.test.ts`        | `tests/check_mk/run-test.bat`          |
| `tests/check_mk-site.test.ts`         | `tests/check_mk/run-test-cmk-site.bat` |
| `tests/icinga-submit.test.ts`         | `tests/icinga/run-test.bat`            |
| `tests/icinga-client-api.test.ts`     | `tests/icinga-client/run-test.bat`     |

Docker-free scenarios (always run, including in no-docker CI pipelines):

| File                                  | Notes                                  |
|---------------------------------------|----------------------------------------|
| `tests/rest-aliases-v2.test.ts`       | CheckHelpers alias listing             |
| `tests/rest-api-discovery.test.ts`    | `/api`, `/api/v1`, `/api/v2`, isalive  |
| `tests/rest-auth.test.ts`             | Login + all auth schemes               |
| `tests/rest-events.test.ts`           | events_controller GET / DELETE         |
| `tests/rest-index.test.ts`            | StaticController fallback              |
| `tests/rest-info.test.ts`             | `/api/v2/info` shape                   |
| `tests/rest-legacy-auth-icinga.test.ts`| Icinga UA-allowlisted query auth      |
| `tests/rest-legacy-query.test.ts`     | Pre-v1 `/query/<cmd>` endpoint         |
| `tests/rest-log.test.ts`              | logs CRUD + `/logs/since`              |
| `tests/rest-metadata.test.ts`         | metadata_controller                    |
| `tests/rest-modules-v1.test.ts`       | modules lifecycle (v1)                 |
| `tests/rest-modules-v2.test.ts`       | modules lifecycle (v2)                 |
| `tests/rest-permissions.test.ts`      | Role gating on `/modules`              |
| `tests/rest-queries-v1.test.ts`       | queries × execute × json/nagios/text   |
| `tests/rest-queries-v2.test.ts`       | queries v2 of the above                |
| `tests/rest-settings.test.ts`         | settings GET / PUT / DELETE            |

The Checkmk end-to-end test (`check_mk-site.test.ts`) pulls a ~500MB image and is also gated by `RUN_CMK_SITE_TEST=1`
(must be set *and* docker must not be skipped).

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

## Cleanup

testcontainers automatically stops and removes started containers on test exit (including on crashes), and the nscp
instance's `stop()` is wired into `afterAll`. There should be no leftover state. If you cancel a test manually, sweep
with:

```sh
docker ps -a --filter "label=org.testcontainers" -q | xargs -r docker rm -f
```
