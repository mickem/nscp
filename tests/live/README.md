# Live / remote acceptance suite

For the opt-in NPS/RADIUS role lab, see [NPS acceptance tests](nps.md).

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
| `NSCP_TARGET_PWD_FILE` | `build/powershell/.vm.pwd` if it exists | A credentials file the `build/powershell` setup scripts write; its `Web URL` and `Web Password` fill in whatever the two above leave unset. The default is used only when `NSCP_TARGET_PASSWORD` is unset. |
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

The [`build/powershell`](../../build/powershell) setup scripts provision a VM,
install NSClient++, run `nscp web install` (WEB on 8443, self-signed cert,
random password) and save the address + password to a credentials file,
`build/powershell/.vm.pwd` by default. The suite reads that file itself when
no password is set:

```powershell
# 1. provision + install (writes .vm.pwd)
./build/powershell/linux/setup-ubuntu-machine.ps1 -VmName NSCP-Ubuntu-Test -Version 0.14.0

# 2. run the acceptance suite against it
npm --prefix tests run test:live -- live-checks
```

`NSCP_TARGET_PWD_FILE` points it at another machine's `.vm.<name>.pwd`.

## Against a role estate

`estate.test.ts` targets a whole estate instead of one machine. For every
machine that is up, it runs the checks its role declares in
`build/powershell/estate/roles/<role>/role.json`. It is the estate's only
check runner: the estate gate (`estate/verify-estate.ps1`, see the
[build/powershell README](../../build/powershell/README.md#role-based-estate-one-command-per-suite))
waits for the fleet sync and then runs this suite. Each machine's address and
password come from the `.vm.<name>.pwd` file its manifest points at, so
`NSCP_TARGET_*` are not needed.

Against an estate kept with `run-estate.ps1 <role> -KeepVms`:

```sh
npm run test:estate                 # every estate that is up
npm run test:estate -- -t mssql     # only the machines whose name matches
```

`test:estate` finds the estate on its own: every manifest under
`build/powershell` (`.fleet-machines.json` and each run's
`.estate-runs/<ts>/suite-<role>/manifest.json`) that lists a machine whose
`.vm.<name>.pwd` still exists. Teardown removes both, so only what is up is
tested, and nothing up fails the run instead of passing an empty one.

| Variable | Meaning |
| --- | --- |
| `NSCP_ESTATE_MANIFEST` | The manifest(s) to test instead, joined with the path delimiter (`;` on Windows). Under `npm run test:live` the suite is skipped without it. |
| `NSCP_ESTATE_ROLES` | Another roles directory. |
| `NSCP_ESTATE_MACHINES` | Only these machines, comma-separated. |
| `NSCP_ESTATE_CHECK_WAIT_SECONDS` | How long one check is retried before it fails (default 60). |
| `NSCP_ESTATE_CHECK_NSCLIENT` | Also run every role check through this `check_nsclient` plugin binary, logged in to one profile per machine (the gate's `-CheckNsclient`). |
| `NSCP_ESTATE_RESULTS_DIR` | Write `<dir>/<machine>/checks.json` with every check's verdict and raw REST result (the gate sets this). |

## Local debugging without a VM

Install NSClient++ locally (or start a dev build) with WEB enabled on 8443, then
point the suite at localhost — the same command CI would use:

```sh
NSCP_TARGET_URL=https://127.0.0.1:8443 NSCP_TARGET_PASSWORD=<pw> npm run test:live
```
