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
```

On Windows:

```cmd
set NSCP_BIN=C:\path\to\nscp.exe
npm test
```

## What runs

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
| `tests/rest-launcher.test.ts`         | `tests/rest/run-test.bat` (launcher)   |

The Checkmk end-to-end test (`check_mk-site.test.ts`) pulls a ~500MB image and is skipped unless `RUN_CMK_SITE_TEST=1` is
set.

The MSI tests (`tests/msi/`) stay Windows-only and are not part of this harness. The Jest REST suite under `tests/rest/`
is invoked unchanged from `rest-launcher.test.ts`.

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
