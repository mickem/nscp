# Live / remote acceptance suite

These suites connect to an NSClient++ that is **already installed, configured
and running** somewhere else and verify — over the REST API — that the service
is reachable, authentication works, and the standard checks return sane results
against real live data.

They are the counterpart to the rest of `tests/`, which spawns its own
throwaway `nscp` via `NscpInstance` and configures it per test. The live suite
spawns nothing; it is aimed at:

- a freshly provisioned Azure VM (see [`build/powershell`](../../build/powershell)),
- a locally-installed service (package / MSI install),
- a dev build you started by hand.

Because it runs against an unmanipulated real machine, assertions are
"shape + healthy" (the API answers, status is a valid Nagios code, the expected
performance data is present) rather than the forced WARNING/CRITICAL threshold
cases — those are covered exhaustively by the self-spawning suites.

## Configuration

Everything is driven by environment so the identical suite runs from a pipeline
against a VM and locally against localhost:

| Variable               | Default                  | Meaning                                            |
| ---------------------- | ------------------------ | -------------------------------------------------- |
| `NSCP_TARGET_URL`      | `https://127.0.0.1:8443` | Base URL of the REST API.                          |
| `NSCP_TARGET_USER`     | `admin`                  | REST user to log in as.                            |
| `NSCP_TARGET_PASSWORD` | _(required)_             | Password for that user (the `nscp web install` pw).|
| `NSCP_TARGET_INSECURE` | `1`                      | Skip TLS cert verification (VMs are self-signed).  |
| `NSCP_TARGET_OS`       | _(auto)_                 | `windows` / `linux`; else derived from the server. |

## Run it

```sh
cd tests
npm install   # first time only

NSCP_TARGET_URL=https://<host>:8443 \
NSCP_TARGET_PASSWORD=<web-password> \
NSCP_TARGET_OS=linux \
  npm run test:live
```

On Windows / PowerShell:

```powershell
$env:NSCP_TARGET_URL="https://<host>:8443"
$env:NSCP_TARGET_PASSWORD="<web-password>"
npm run test:live
```

The live suite is **not** part of the default `npm test` run — its config
(`jest.live.config.js`) matches only `tests/live/**` and uses a global-setup
that validates `NSCP_TARGET_*` instead of requiring `NSCP_BIN`.

## Against an Azure VM

The [`build/powershell`](../../build/powershell) scripts provision a VM, install
NSClient++, run `nscp web install` (WEB on 8443, self-signed cert, random
password) and save the address + password to `build/powershell/.vm.pwd`.
`run-tests.ps1` reads that file and runs this suite for you:

```powershell
# 1. provision + install (writes .vm.pwd)
./build/powershell/linux/setup-ubuntu-machine.ps1 -VmName NSCP-Ubuntu-Test -Version 0.14.0

# 2. run the acceptance suite against it
./build/powershell/run-tests.ps1 -VmName NSCP-Ubuntu-Test -Os linux
```

## Local debugging without a VM

Install NSClient++ locally (or start a dev build) with WEB enabled on 8443, then
point the suite at localhost — the same command CI would use:

```sh
NSCP_TARGET_URL=https://127.0.0.1:8443 NSCP_TARGET_PASSWORD=<pw> npm run test:live
```

or via the runner:

```powershell
./build/powershell/run-tests.ps1 -Local -Password <pw> -Os linux
```
