# A Mod-Gearman lab in a container

A real Naemon, with the Mod-Gearman NEB module and a gearmand, in one
container — so you can point an NSClient++ at it and watch the agent pull
checks off a queue and answer them.

```text
  +--------------------------- docker ---------------------------+
  |  Naemon --schedules--> Mod-Gearman NEB --SUBMIT_JOB-->        |
  |     ^                                              gearmand   |
  |     +------- result thread <--check_results-------- :4730     |
  |  Thruk (web UI) :8080  ---- published on localhost:8088 ---->  |
  +------------------------------+--------------------------------+
                                 | 127.0.0.1:4730 (outbound only)
                     +-----------+------------+
                     |  NSClient++            |
                     |  GearmanClient worker  |
                     +------------------------+
```

Nothing listens on the machine running the agent: the agent connects **out**
to gearmand, registers on the queue the core routes, and answers whatever
lands there.

The same ground is covered by the integration suites in `tests/`
(`gearman-core.test.ts` and friends), which assert instead of letting you
poke. This one is for poking.

## What you need

* Docker (with `docker compose`).
* An NSClient++ with the `GearmanClient` module — an installed agent, or a
  build tree (`nscp.exe` plus `modules/GearmanClient.dll`).

## 1. Start the lab

```powershell
cd examples\mod-gearman
docker compose up --build
```

The first build pulls Debian and the ConSol Labs packages and takes a minute
or two; after that it starts in a couple of seconds. When it is up:

* **Web UI:** <http://localhost:8088/thruk/> — no password, everyone is an
  administrator.
* **gearmand:** `127.0.0.1:4730`.

Both ports are published on the loopback address only. If 8088 is taken, set
`LAB_WEB_PORT` (in the environment, or in a `.env` file next to
`docker-compose.yml`) and start it again.

Every check in the lab starts out `PENDING`, and the ones for the host
`nscp-lab` stay that way until an agent shows up — they are sitting in the
gearmand queue. The service `gearman-lab / Gearman queue` is the core
watching that queue itself: it goes CRITICAL while nothing is registered on
it.

## 2. Point an agent at it

`nsclient.ini` in this directory is a complete configuration for the agent
side. From a build tree:

```powershell
cd C:\src\nscp\cmake-build-relwithdebinfo-visual-studio
.\nscp.exe test --settings ini://C:\src\nscp\examples\mod-gearman\nsclient.ini
```

`test` runs the agent in the foreground with its log on the console, which is
what you want here. For an installed agent, copy the `[/modules]` lines and
the two `[/settings/gearman/...]` sections into
`C:\Program Files\NSClient++\nsclient.ini` and restart the service.

Either way the log should say:

```text
gearman: started 2 worker(s) for 1 queue(s)
gearman: nscp-<hostname>-1 connected to 127.0.0.1:4730 for hostgroup_windows-lab
```

and within half a minute the web UI fills in:

| Host / service                | What it proves                                                      |
|-------------------------------|---------------------------------------------------------------------|
| `nscp-lab` (host check)       | `check_ok` — the agent is connected and answering                   |
| `CPU`                         | `check_cpu`, collector-backed, with performance data                |
| `Memory`                      | `check_memory`                                                      |
| `Disk C`                      | `check_drivesize`                                                   |
| `Uptime`                      | `check_uptime`                                                      |
| `DHCP client service`         | `check_service` — arguments (`service=Dhcp`) survive the round trip  |
| `Passive result`              | a result the agent **files by itself**, see step 4                  |
| `Missing command`             | deliberately UNKNOWN: what a check whose module is off looks like   |
| `gearman-lab / Gearman queue` | the queue itself: two workers registered, no jobs waiting           |

## 3. Poke at it

From the host:

```powershell
# What is on the queues, and how many workers are on them.
docker compose exec gearman-lab gearadmin --port=4730 --status
# hostgroup_windows-lab   0   0   2      <- queued / running / workers
# check_results           0   0   1

# The Mod-Gearman view of the same thing.
docker compose exec gearman-lab gearman_top --batch

# The core's own log, and the NEB module's.
docker compose logs -f
docker compose exec gearman-lab tail -f /var/log/naemon/mod_gearman_neb.log
```

Things worth trying:

* **Stop the agent.** The queue starts filling up, `Gearman queue` goes
  CRITICAL, and the core eventually declares the checks orphaned. Start it
  again and everything drains.
* **Change the key** in `conf/module.conf` and `docker compose restart`. The
  jobs are still queued; the agent simply cannot read them. This is what a
  key mismatch looks like, and nothing says so except the agent's log
  (`could not decode a job … (wrong key?)`).
* **Write a threshold the Nagios way** — `"warning=load>80"` — and watch the
  check come back *Not run: the command contains illegal metacharacters*.
  `>` is a metacharacter; the filter language spells it `gt`.
* **Add a check.** Anything the agent can answer works: put a new `define
  service` in `conf/objects.cfg` with `check_command nscp!<your query>` and
  `docker compose restart`.

## 4. The passive half

The same module also **files results nobody asked for**, into the same result
queue — which is what lets a Mod-Gearman installation drop NSCA. The agent
configuration schedules one every 30 seconds, and it lands on the service
`Passive result`, which the core never actively checks.

To file one by hand:

```powershell
.\nscp.exe gearman --settings ini://C:\src\nscp\examples\mod-gearman\nsclient.ini `
    --address 127.0.0.1:4730 --key nscp-lab-secret `
    --command "Passive result" --result CRITICAL "--message=submitted by hand"
```

It shows up in the web UI within a second or two. A result whose `--command`
does not name a service the core knows is discarded without a word — that is
the protocol, not the agent.

## 5. Proxy mode

The other deployment: one agent answering for hosts that have **no agent of
their own**, with every check naming its own target. `conf/objects.cfg` ends
with a commented-out block that monitors this lab's container that way. To
try it:

1. uncomment that block in `conf/objects.cfg`;
2. in `nsclient.ini`, set `mode = proxy` in `[/settings/gearman/worker]` and
   add `CheckNet = enabled` under `[/modules]`;
3. `docker compose restart`, and restart the agent.

The agent says so once, and the sentence is the point of the mode:

```text
gearman: running in proxy mode: every check on hostgroup_windows-lab is
executed here whichever host it names, through this agent's own commands and
credentials.
```

Leave the block commented out while the worker is in agent mode, or those
checks answer *Not run: this NSClient++ agent does not answer for
lab-itself* — which is the host binding doing its job.

## What is in here

| File                             | What it is                                                         |
|----------------------------------|--------------------------------------------------------------------|
| `docker-compose.yml`             | the lab, its two published ports and the bind-mounted configuration |
| `Dockerfile`                     | Debian + Naemon + Mod-Gearman + gearmand + Thruk                    |
| `entrypoint.sh`                  | starts the three processes                                          |
| `conf/module.conf`               | the NEB module: which hostgroup goes to gearmand, the shared key    |
| `conf/objects.cfg`               | the hosts and services — **the file to edit**                       |
| `nsclient.ini`                   | the agent side, complete                                            |
| `thruk.psgi`, `thruk_local.conf` | Thruk without apache; nothing to do with Mod-Gearman                |

Edit `conf/module.conf` or `conf/objects.cfg`, then `docker compose restart`.
Both are bind-mounted, so no rebuild is needed. A configuration that does not
parse stops the container, with Naemon's own error message in
`docker compose logs`.

## Tear it down

```powershell
docker compose down
```

Nothing is persisted, so the next `up` starts from nothing.

## This is a toy

It has no authentication of any kind: gearmand has none by design, and the
web UI logs every visitor in as an administrator. The shared key is a
well-known string in a file in this repository. Anyone who can reach port
4730 with that key can queue checks for your agent to run — and in proxy
mode, checks against anything the agent can reach. Keep it on your own
machine.

The real thing is documented in
[Mod-Gearman (Naemon / Nagios Core)](../../docs/docs/scenarios/mod-gearman.md),
including the security posture, and the
[`GearmanClient` reference](../../docs/docs/reference/client/GearmanClient.md)
has every setting.
