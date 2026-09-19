# Plan: host facts

Status: proposal, not started. This is the design and implementation plan for
**facts**: structured, hierarchical inventory about a host that modules
contribute, the agent exposes locally, and the fleet server receives. Nothing
in this document is built yet; each phase lists the files it touches so the
work can be split across branches. File references point at the code as of
this writing so the anchors can be checked before each phase starts.

## 1. What facts are

Facts are to inventory what tags are to group membership:

| | Tags (exist today) | Facts (this plan) |
|---|---|---|
| Shape | flat `key=value` strings (`service/tag_repository.hpp`) | a JSON tree: sections, scalars, and lists of records |
| Size | 256 entries, 1 KiB values | one document per host, capped at 1 MiB |
| Purpose | fleet group selectors (`sqlserver = "detected"`) | inventory, monitoring definition, and later *context* attached to a failing check |
| Default | modules publish a handful unconditionally (`drives`, `os_version`, …) | **nothing is published until a fact set is enabled** |
| Producer API | push: `core->set_tag(k, v)` from anywhere | pull: the core calls `fetchFacts()` on a schedule, like `fetchMetrics()` |
| Wire | `reported_tags` object in every state report | `facts_hash` in every state report, the document itself on a separate call only when it changes |

Tags stay exactly as they are. Facts do not replace them: a selector needs a
flat, cheap, always-present value, an inventory needs structure and lists. The
fleet server may later derive tags from facts, but that is server work and out
of scope here. Today a "list" tag is a comma-joined string (`drives=c:,d:`,
`modules/CheckDisk/CheckDisk.cpp:59`), which is exactly the shape facts exist
to get away from.

### 1.1 Uses, in priority order

1. **Fleet monitoring definition.** The server reads a host's facts (which
   volumes exist, which services run, which software is installed) to decide
   what to monitor and to build bundles, instead of the operator typing it in.
2. **Inventory.** The fleet server's host page and the agent's own web UI show
   what the host is: hardware, OS, software, network.
3. **Context on failure (future).** When a check fails, the result can carry
   the fact subtree the check relates to (`check_drivesize` on `d:` → the
   `storage.volumes[d:]` record). Only the hook is designed here (section
   4.6); nothing is built for it in this plan.

## 2. Data model

### 2.1 The document

A host's facts are one JSON object. The first level is a **fact set**: the
unit that is enabled, produced, and documented. Below that the producer is free
to nest objects and lists.

```json
{
  "agent":    { "version": "0.20.0", "hostname": "web-01", "modules": ["CheckDisk", "CheckSystem"] },
  "os":       { "family": "windows", "name": "Windows Server 2022", "version": "10.0.20348", "arch": "x86_64", "boot_time": "2026-09-01T04:12:09Z" },
  "hardware": { "vendor": "Dell Inc.", "model": "PowerEdge R650", "serial": "ABC1234", "chassis": "rack",
                "cpu": { "model": "Intel Xeon Gold 6338", "sockets": 2, "cores": 64 },
                "memory": { "total_bytes": 274877906944, "modules": [ { "id": "DIMM_A1", "size_bytes": 34359738368, "speed_mhz": 3200 } ] } },
  "storage":  { "volumes": [ { "id": "c:", "fs": "NTFS", "size_bytes": 255000000000, "type": "fixed" }, { "id": "d:", "fs": "NTFS", "size_bytes": 2000000000000, "type": "fixed" } ] },
  "network":  { "interfaces": [ { "id": "Ethernet0", "mac": "00:50:56:aa:bb:cc", "addresses": ["10.0.0.5"], "speed_bps": 10000000000, "state": "up" } ] },
  "software": { "installed": [ { "id": "Microsoft SQL Server 2022", "version": "16.0.1000.6", "vendor": "Microsoft Corporation", "installed": "2026-03-02" } ] }
}
```

Rules, enforced by the core when it accepts a fact set:

- **Keys** are `snake_case` ASCII (`[a-z][a-z0-9_]*`), at most 64 characters.
  A key that carries a unit ends with it (`total_bytes`, `speed_bps`,
  `speed_mhz`), as metric keys do.
- **Scalars** are JSON strings, numbers or booleans. No nulls: an unknown
  value is omitted, not written as `null` or `""`.
- **Lists are lists of records**, except for plain string lists such as
  `addresses` or `modules`. A record is an object with an **`id`** field that
  is unique within the list and stable across runs (the drive letter or mount
  point, the interface name, the package name, the DIMM locator). The server
  diffs lists by `id`; without one every refresh looks like a full
  replacement.
- **Timestamps** are ISO 8601 UTC strings; dates without a time are
  `YYYY-MM-DD`.
- **Depth** at most 6, **list length** at most 5000 records, **document** at
  most 1 MiB serialised. A fact set that breaks a rule is rejected whole and
  the previous value of that set is kept; the rejection is logged once at
  error level with the reason, the way an oversized tag is
  (`service/core_api.cpp:203`).

### 2.2 Fact sets and ownership

A **fact set** is named by its top-level key (`os`, `hardware`, `software`,
`storage`, `network`, `security`, `docker`, `tasks`, `agent`, …). One fact set
is produced by exactly one module on a given platform; two modules never write
into the same top-level key. Where the same set exists on both platforms
(`os`, `hardware`, `software`, `network`) the Windows and Unix modules produce
the same keys with the same meaning — the existing `check_network` structs
already mirror each other across `modules/CheckSystem/check_network.hpp` and
`modules/CheckSystemUnix/check_network.h` on purpose, and facts keep that
discipline the way `core_label()` keeps metric labels identical.

Within a set, a module may split enablement finer: `software` has
`software.installed` and `software.hotfixes`, each with its own switch
(section 3.1), because the installed-software list is the one that costs the
most to collect and to ship.

The **id of an enableable unit** is therefore a dotted path of depth 1 or 2:
`os`, `hardware`, `software.installed`, `software.hotfixes`. These ids are
what the settings, the docs and the `nscp test` output speak about.

### 2.3 Registry of fact sets (initial)

Every data source below already exists as a pure gather function or struct;
the producer work is a mapping from that struct to the builder, not new
collection code, except where marked *new*.

| Id | Producer (Windows / Unix) | Existing source | Content | Cost |
|---|---|---|---|---|
| `agent` | core | `CURRENT_SERVICE_VERSION`, the plugin registry | version, hostname, loaded modules, enrolled yes/no (never the identity) | none |
| `os` | CheckSystem / CheckSystemUnix | `os_version_filter::filter_obj` (`modules/CheckSystem/filter.hpp:116`); `os_release_info` + `os_version_filter` (`modules/CheckSystemUnix/check_os_version.h`) | family, name, version, build/ubr, kernel, arch, boot time, timezone; Windows adds `pending_reboot` from `pending_reboot_check::gather_pending_reboot()` | none |
| `identity` | CheckSystem / CheckSystemUnix | `host_identity` (`modules/CheckSystem/check_hostname.hpp`); `uname` + `/etc/hostname` on Unix | hostname, FQDN, DNS domain, domain-joined | none |
| `hardware` | CheckSystem / CheckSystemUnix | `hardware_check::gather_hardware()` → `hardware_info` + `memory_module` (`modules/CheckSystem/check_hardware.hpp`); `cpu_frequency` per socket; Unix *new*: `/sys/class/dmi/id`, `/proc/cpuinfo`, `/proc/meminfo` | vendor, model, serial, asset tag, chassis, UUID, CPU model/sockets/cores, total memory, memory modules | low |
| `storage.volumes` | CheckDisk | `check_drive` containers (`check_drive_win.cpp` / `check_drive_unix.cpp`), `read_mount_points()` (`modules/CheckDisk/check_mount.hpp`) | id (drive letter / mount point), filesystem, size, type (fixed/removable/network/…), label, device | none |
| `network.interfaces` | CheckSystem / CheckSystemUnix | `network_interface` (`check_network.hpp` / `check_network.h`) | id, MAC, addresses, speed, link state, NIC-team membership (Windows) | none |
| `software.installed` | CheckSystem / CheckSystemUnix | `installed_software_check::gather_installed_software()` → `software_entry` (Uninstall hives, `modules/CheckSystem/check_installed_software.hpp`); `detect_package_manager()` + `fetch_installed()` (dpkg/rpm/pacman, `modules/CheckSystemUnix/check_installed_software.h`) | id (name), version, vendor, install date, architecture, hive/user (Windows) | **high**: hundreds of records, a process exec on Unix |
| `software.hotfixes` | CheckSystem | the hotfix list `check_patch_age` reads | id (KB), installed date, description | medium |
| `services` | CheckSystem / CheckSystemUnix | `win_list_services` (`check_service.h`); `check_svc_filter::active_units` + `systemctl list-units` | id, display name, state, start mode, account (Windows) | medium |
| `security` | CheckSecurity | the per-area `gather(out, error)` functions in `posture_win.cpp` / `posture_unix.cpp` (firewall, antivirus, defender, bitlocker, secureboot, activation, local accounts, nla) | the posture matrix, one object per area, lists where the area is per-volume/profile/product | medium |
| `docker` | CheckDocker | `GET /info`, `GET /containers/json`, `GET /system/df` through the existing fetcher (`modules/CheckDocker/check_docker.hpp`) | engine version, containers (id = name, image, state), images (id, tags, size) | medium, needs the socket |
| `tasks.scheduled` | CheckTaskSched | the task enumeration in `TaskSched.cpp` / `filter.hpp` | id (path), state, last run, next run, exit code, author; **no arguments** | medium |
| `updates.pending` | CheckSystem | the TTL-cached WUA fetcher behind `check_os_updates.hpp` | pending updates (id, title, severity, category, kb) | **high**: the WUA query takes seconds and runs off the collector |

Everything in the table is opt-in (section 3). The cost column decides how a
producer paces itself (section 4.3) and is stated in the settings description
so an operator knows what enabling a set costs. `CheckNet`, `CheckWindowsApps`
(IIS pools/sites, RDS licences), `CheckMSSQL` (databases) and `CheckEventLog`
(channels) are candidates for later sets; none is in this plan.

## 3. Configuration

### 3.1 Enabling facts

One section, owned by the core, keyed by fact-set id:

```ini
[/settings/facts]
; nothing here = no facts at all (the default)
os = true
hardware = true
storage.volumes = true
software.installed = true
```

- The core registers the path `/settings/facts` with its description, next to
  `/settings/fleet` (`service/NSClient++.cpp:510`); each producer registers
  the keys for the sets it can produce, from `loadModuleEx`, through the
  normal `settings_registry` (`add_key_to_settings("/settings/facts")
  .add_bool(id, sh::bool_key(&flag, false), title, description)`), so every
  key carries a title, a description that states the content and the cost, a
  default of `false`, and lands in the generated reference docs and in
  `nscp settings --generate`.
- A key is a bool as the settings helper defines it (`sh::bool_key`,
  `include/nscapi/settings/helper.hpp:37`), so a fleet bundle flips it with
  the same INI it already renders.
- **No implicit enablement.** Loading `CheckSystem` does not enable `os`;
  `nscp enroll` does not enable anything; the web installer does not tick
  anything. A fresh install reports the `facts_hash` of the empty document and
  nothing else. This is deliberate: facts are inventory, inventory is data an
  operator did not necessarily agree to ship, so the operator (or the fleet
  bundle they accepted) turns it on.
- The core-level `[/settings/facts]` also carries:

| Key | Default | Meaning |
|---|---|---|
| `interval` | `1h` | How often the core asks every producer to refresh. |
| `max size` | `1048576` | Serialised document cap; a set that would push the document past it is rejected. |

There is deliberately no global `enabled` switch: an empty section is "off",
which keeps the reading of the INI unambiguous (`enabled = true` with no sets
enabled would be a puzzle). There is also deliberately no comma-separated
token list like `[/settings/system/windows] disable`
(`modules/CheckSystem/disable_list.hpp`): fleet bundles are applied as an RFC
7396 merge patch (`libs/onboarding/sync.cpp`), which merges per key, so one
bundle can enable `os` and another `software.installed` without either
clobbering the other. A single list value would be replaced whole.

### 3.2 Fleet-managed enablement

Because enablement is plain INI under `/settings/facts`, a fleet bundle turns
facts on for a group exactly as it configures anything else:

```ini
[/settings/facts]
os = true
software.installed = true
```

That is the intended workflow: the server side offers "collect inventory for
this group", which is a bundle with this fragment. No new protocol is needed
for enablement.

## 4. Architecture

### 4.1 Core: `fact_repository`

`service/fact_repository.hpp` (+ `fact_repository_test.cpp`), a sibling of
`service/tag_repository.hpp`:

```cpp
class fact_repository {
 public:
  enum class set_result { changed, unchanged, rejected };
  // Replace one fact set (top-level key) with `value`, recording which plugin
  // owns it. Validates keys, depth, list ids and the size budget; a rejected
  // set leaves the previous value in place and `error` says why.
  set_result set(const std::string &fact_set, unsigned int plugin_id, const boost::json::value &value, std::string &error);
  set_result remove(const std::string &fact_set);
  void remove_owned_by(unsigned int plugin_id);           // unload / reload
  void retain_only(const std::set<std::string> &enabled);  // settings reload
  boost::json::object get_all() const;                    // the whole document
  boost::optional<boost::json::value> get(const std::string &path) const;  // dotted path lookup
  std::string get_hash() const;                          // sha256 of the canonical serialisation
  unsigned long long get_revision() const;               // bumps on every effective change
  // Per-set collection errors from the last round, for the API and the UI.
  std::map<std::string, std::string> get_errors() const;
};
```

- **Canonical serialisation**: keys sorted, no whitespace, numbers in the
  shortest round-trip form. The unit test pins the exact bytes of a fixed
  document because the hash is what the server compares against.
- The hash is recomputed lazily on read after a change, not on every `set`,
  so a producer replacing five sets in one round costs one hash.
- Thread safety as `tag_repository`: one mutex, copies out.
- Exposed from `NSClientT` as `get_fact_repository()` next to
  `get_tag_repository()` (`service/NSClient++.h:105`), constructed with
  `tags_` (`service/NSClient++.cpp:261`) and passed to `fleet_sync`
  alongside it (`service/NSClient++.cpp:557`).

### 4.2 Plugin ABI: `fetchFacts`

Mirror the metrics producer path exactly, with JSON instead of protobuf across
the ABI: facts are a JSON document on every consumer (REST, fleet, UI),
`boost::json` is already used by the core, WEBServer, CheckDocker, CheckNet and
CheckSystem, and there is no protobuf `Struct` in the lite runtime.

| Layer | Metrics (today) | Facts (new) |
|---|---|---|
| `module.json` | `"metrics": "produce"` | `"facts": { "os": "…", "hardware": "…" }` — the set ids the module can produce with one line each, the same shape as `commands`; presence generates the glue |
| `NSCAPI.h` | `lpFetchMetrics` (`include/NSCAPI.h:176`) | `lpFetchFacts(unsigned plugin_id, const char *request, unsigned request_len, char **response, unsigned *response_len)` |
| Generated glue (`build/python/create_plugin_module.py`, the four `module.metrics` blocks at lines 36, 111, 585, 746, 797, 913) | `NSFetchMetrics` export, `fetchMetrics(std::string&)` wrapper | `NSFetchFacts` export; wrapper parses the request, calls `impl_->fetchFacts(const nscapi::facts::request &, nscapi::facts::response &)`, serialises the response, catches everything into a per-round error |
| Module-side marshalling (`include/nscapi/nscapi_plugin_wrapper.hpp:360`) | `metrics_wrapper` | `facts_wrapper` |
| Plugin interface (`service/plugins/plugin_interface.hpp:79`) | `hasMetricsFetcher()` / `fetchMetrics()` | `hasFactsFetcher()` / `fetchFacts(const std::string &request, std::string &response)` — on `dll_plugin` **and** `zip_plugin` |
| DLL loader (`service/plugins/dll_plugin.cpp:660`) | `load_proc("NSFetchMetrics")` | `load_proc("NSFetchFacts")`, nullptr when absent |
| Plugin manager (`service/plugins/plugin_manager.cpp:590`, `:1240`) | `metrics_fetchers_` list + `process_metrics()` | `facts_fetchers_` list + `process_facts(reason)`: builds the request from `[/settings/facts]`, fans out, applies each response to the repository |
| Scheduler (`service/scheduler_handler.hpp:14`, `.cpp:55`) | `METRICS` task every `metrics interval` | `FACTS` task every `[/settings/facts] interval`; **not registered at all when no set is enabled** |
| Module helper | `nscapi/nscapi_metrics_helper.hpp` builder | `nscapi/nscapi_facts_helper.hpp` builder (section 4.4) |

Request and response over the ABI are UTF-8 JSON strings:

```json
// request: what the core wants this round
{ "enabled": ["os", "hardware", "software.installed"], "reason": "scheduled" }
// response: one entry per fact set the module produced this round
{ "sets": { "os": { ... }, "hardware": { ... } }, "errors": { "software.installed": "access denied to HKLM\\..." } }
```

- The core passes the enabled ids, so a module never reads `/settings/facts`
  for the decision (it registers the keys for the docs and reads nothing
  back); the module returns only the sets it is asked for and can produce. A
  set the module was asked for but did not return is left as it was, not
  removed — a transient failure must not blank the inventory. An explicit
  `null` in `sets` removes it: the producer discovered the data is gone (the
  docker socket disappeared).
- `reason` is `startup`, `scheduled`, `reload` or `manual` (from `nscp test`
  or the REST refresh) so a producer can skip an expensive collection on a
  `reload` when nothing changed for it.
- Errors are per set. They reach the log at warning level once per distinct
  message per set (the transport-error de-duplication in
  `service/fleet_sync.cpp:log_transport_failure` is the pattern), are kept in
  the repository, and `/api/v2/facts` reports them under `errors` so the UI
  can show "software.installed: not collected: …" instead of silence.
- `fetchFacts` runs on the scheduler's maintenance pool with the same latitude
  as `fetchMetrics`. A producer whose collection is slow (WUA, `rpm -qa`) does
  the work in its own thread or collector and returns the last snapshot,
  exactly as `check_os_updates` and the PDH collector do for metrics.
- `remove_plugin` / `unloadModule` remove every set that plugin owns
  (`remove_owned_by`), so unloading `CheckDocker` drops `docker` rather than
  freezing it; the same call already removes the plugin from
  `metrics_fetchers_` (`plugin_manager.cpp:418`).

The `zip_plugin` (`service/plugins/zip_plugin.cpp`) implements the interface
too; it returns "no fetcher" until script producers exist (section 4.7).

### 4.3 Refresh cadence

- **Startup**: one `startup` round after all modules loaded, before the fleet
  loop's first state report, so a fresh host reports its inventory with its
  first report — the same reason `fleet_sync::run` reports tags before its
  first poll (`service/fleet_sync.cpp:739`).
- **Then every `[/settings/facts] interval`** (default `1h`). One interval for
  everyone is enough for a first version. A producer with expensive data uses
  `reason` and its own bookkeeping to skip work: `software.installed` re-reads
  the Uninstall hives only if a hive's last-write time moved; on Unix, only if
  `/var/lib/dpkg/status` or `/var/lib/rpm` moved. That keeps the cadence
  decision in the producer, where the cost is known, without a per-set
  interval matrix in the settings.
- **On reload** (settings changed): a `reload` round, because the enabled set
  may have changed. Sets that are no longer enabled are removed by the core
  (`retain_only`), not by the producer.
- **On demand**: `nscp test` → `facts refresh`, and `POST /api/v2/facts/refresh`
  (section 4.5), both `manual`.

### 4.4 Module helper: `nscapi/nscapi_facts_helper.hpp`

A thin builder over `boost::json`, so producers do not hand-assemble objects
and cannot violate the key rules by accident (the core validates regardless):

```cpp
using namespace nscapi::facts;
void CheckSystem::fetchFacts(const request &req, response &out) {
  if (req.wants("os")) {
    section os = out.set("os");
    os.value("family", "windows").value("name", name).value("version", version).value("arch", arch);
    os.time("boot_time", boot_time);                       // ISO 8601 UTC
    os.value("pending_reboot", pending);
  }
  if (req.wants("software.installed")) {
    list installed = out.set("software").list("installed");
    for (const auto &app : apps) installed.record(app.name).value("version", app.version).value("vendor", app.publisher).date("installed", app.install_date_epoch);
  }
  if (req.wants("hardware")) {
    try { ... } catch (const std::exception &e) { out.error("hardware", e.what()); }
  }
}
```

`record(id)` writes the `id` field, so a list without ids cannot be built.
`value()` overloads cover string, integer, double and bool; `time()` and
`date()` format the timestamps; empty strings are skipped (rule: omit, do not
write `""`). Unit-tested on its own (`nscapi_facts_helper_test.cpp`, in the
style of `include/nscapi/nscapi_metrics_helper_test.cpp`: sentence-shaped test
names, assert the absence of invented fields too), built into
`libs/plugin_api` next to the metrics helper.

### 4.5 Consumers

**Core API** (`service/core_api.cpp`, `include/NSCAPI.h:127`,
`include/nscapi/nscapi_core_wrapper.hpp:75`): one query-style entry point,
`NSAPIFactsQuery(request, response)`, in the shape of `NSAPISettingsQuery`,
with a JSON request `{ "op": "get", "path": "software.installed" }` or
`{ "op": "refresh" }` (which runs a `manual` round and returns the document).
`core_wrapper` gains `get_facts_json(path = "")`, `get_facts(path)` returning a
`boost::json::value`, and `refresh_facts()`; all degrade to `{}` / `false` on a
core without the API, as `get_tags_json()` does. Read side only: there is no
push-style `set_fact`, so the enable list is authoritative and no module can
publish an un-enabled set from a side thread.

**REST** (`modules/WEBServer`), a `facts_controller` next to
`tags_controller.cpp`, registered at `WEBServer.cpp:481` and added to the
`/api/v2` index (`api_controller.cpp:43`, `facts_url`):

| Route | Grant | Body |
|---|---|---|
| `GET /api/v2/facts` | `facts.get` | `{ "revision": 12, "hash": "…", "collected": "<ts>", "enabled": [ … ], "facts": { … }, "errors": { … } }` |
| `GET /api/v2/facts/<path>` | `facts.get` | the subtree at a dotted path (`os`, `software.installed`); 404 when absent |
| `POST /api/v2/facts/refresh` | `facts.refresh` | runs a `manual` round, returns the same body as `GET` |

`facts.get` joins the `monitoring` role's grant list (`WEBServer.cpp:366`) and
`facts.refresh` only `full`. `ETag` is the hash so the UI can poll cheaply.
Documented in `docs/docs/api/rest/facts.md` in the existing Verb / Address /
Privilege table pattern (`docs/docs/api/rest/modules.md`), with a bullet in
`docs/docs/api/rest/index.md` and a `nav:` entry in `docs/mkdocs.yml`.

**Web UI** (`web/src`): an `Inventory` page (`pages/Inventory.tsx`, route
`inventory` in `Routes.tsx`, entry in `components/SideMenu.tsx` in the
metrics/events/logs block; `getFacts` endpoint in `api/api.ts` with `"Facts"`
added to both `ALL_API_TAGS` and `tagTypes`). It renders each enabled fact set
as a card — scalars as a definition list, lists as a sortable table with the
record fields as columns, modelled on `pages/Metrics.tsx` — plus the errors
and a "collected N minutes ago / Refresh" line. With no set enabled the page
explains how to enable one, quoting the INI, instead of showing an empty tree.
Dashboard gains nothing but a link. Tests as `TagsWidget.test.tsx`; if the
shell fetches facts, `Routes.test.tsx`'s `mockShellApi()` map gains the route.

**`nscp test` console** (`include/client/simple_client.cpp:319`,
`builtin_commands()` and the dispatch chain at `:649`): verbs `facts [path]`
(pretty-print the document or a subtree), `facts refresh`, and `facts list`
(the registered set ids with enabled/disabled and their producer, from the
registry). They read through `core_wrapper::get_facts_json()`, so no
`push_facts` store is needed on the client. No standalone `nscp facts` CLI in
this plan: a one-shot would have to load every module anyway.

**Fleet sync** (`service/fleet_sync.cpp`): section 5.

### 4.6 Hook for "context on failure" (design only)

A check that wants context declares which fact records it relates to. The
cheapest future-proof hook is a convention, not code: a producer's list record
`id` is the same string the corresponding check uses as its instance name
(`storage.volumes[].id` = the `drive` keyword of `check_drivesize`;
`network.interfaces[].id` = the `name` keyword of `check_network`;
`services[].id` = the service name; `docker.containers[].id` = the container
name). The producers' unit tests enforce that where the check already exists.
The later feature can then attach `get_facts("storage.volumes")` filtered by
the failing instance to a result without a new registry.

### 4.7 Scripts (Python/Lua)

Deferred. `PythonScript` implements both sides of metrics today
(`modules/PythonScript/script_wrapper.cpp:812`, `register_fetch_metrics`), so
`register_fetch_facts(callback)` returning a dict per enabled set is the
obvious shape; but a script-produced set needs its own enablement key and
docs, so it is a follow-up after the C++ producers settle. `CheckExternalScripts`
is out of scope: an external script returning JSON inventory is a reasonable
future producer, but must be designed with the cost/timeout model, not bolted
on.

## 5. Fleet protocol

Today (`libs/onboarding/sync.cpp:363`, `build_state_report`) every state
report to `POST /agent/v1/state-report` carries the whole tag map, and the
loop re-reports whenever the tag revision moved (`service/fleet_sync.cpp:765`).
Facts are too large to repeat every 60 s, so:

1. **Every state report gains `facts_hash`** (sha256 hex of the canonical
   document; the hash of `{}` when nothing is enabled). Cheap, always present,
   lets the server see "inventory changed" and "inventory missing" without
   the document. Older servers ignore the extra key. `build_state_report`
   takes the hash as one more string argument; the exact-key-set assertion in
   `libs/onboarding/sync_test.cpp:1274` grows by one key.
2. **New `POST /agent/v1/facts`** with body
   `{ "facts_hash": "…", "collected_at": "<ts>", "facts": { … } }`, over the
   same mTLS `do_call` as every other agent call (`fleet_sync.cpp:217`).
   Sent:
   - once at startup after the first facts round (with the empty document
     too, so a server that had inventory from before learns it was switched
     off);
   - whenever the repository revision moved since the last *successful*
     upload, checked in the same place the loop re-reports tags;
   - when the server asks: the desired-state and state-report responses may
     carry `"facts_hash": "<what the server holds>"`; if it differs from ours,
     upload. This covers a server restore or a host re-added on the server
     side. A response without the key means "server does not do facts" and is
     not a trigger.
3. Response handling: `200`/`204` mark the hash as uploaded; `404`/`405` mean
   an older server, logged once at debug, after which uploads are skipped
   until the agent restarts or a server response carries `facts_hash`; `413`
   is an error the operator must see, once, naming the largest sets so they
   can disable the offender; anything else goes through
   `log_transport_failure` like every other call.
4. The upload is capped by `[/settings/facts] max size` on the agent so a
   producer cannot push the agent into `413` loops. The server keeps its own
   cap.
5. **Privacy contract.** The state report keeps carrying only
   `local_config_present`, never configuration, and the test that pins this
   (`tests/fleet-sync.test.ts:296`, the body must not mention `CheckDisk`)
   stays as it is: the report carries a hash, not the document. Facts never
   contain configuration, credentials, command lines or environment; a
   producer that could (tasks, processes) strips arguments. The one
   configuration-adjacent item, the loaded-module list in `agent`, is opt-in
   like everything else.

Server-side work (the fleet server is a separate codebase): accept the new
call, store the document per host, diff lists by `id` for history, expose it
on the host page, and offer "enable inventory" as a bundle template. This plan
only pins the wire contract above so both sides can be built independently.
The fake server in `tests/fleet-sync.test.ts:180` gains the route; the
container-backed `tests/src/fleet-server.ts` exercises it once the real server
ships the endpoint.

## 6. Documentation

- `docs/docs/concepts/facts.md`: what facts are, the difference from tags, the
  document rules, how to enable sets, the cost table, the privacy stance.
  Registered in `docs/mkdocs.yml` and `docs/docs/concepts/index.md`. This is
  also where tags finally get user-facing documentation (today only
  `docs/docs/setup/fleet.md:258` mentions them).
- `docs/docs/setup/fleet.md`: a step after "Send it some configuration"
  showing the bundle fragment that enables inventory and what the server shows.
- `docs/docs/api/rest/facts.md`: the three routes with bodies and grants.
- Per-module reference: the `[/settings/facts]` keys a module registers render
  automatically through `scripts/python/docs_extract.py`; each key's
  description states the content and the cost. The generator additionally
  renders a "Facts" table per module from the `facts` object in `module.json`,
  so the docs and the generated glue share one source.
- `docs/upgrades/next/facts.md`: `action: none`, informational ("nothing is
  collected unless enabled"), `modules:` the producers plus `core`.
- No security notice: no default behaviour changes. If a producer is ever
  enabled by default, that is when one is due.

## 7. Testing

- **Unit** (gtest, next to the code, registered with `NSCP_CREATE_TEST`):
  `service/fact_repository_test.cpp` (validation rules, size cap, canonical
  bytes and hash stability, ownership removal, `retain_only`, revision
  semantics mirroring `tag_repository_test.cpp`); `nscapi_facts_helper_test.cpp`;
  per producer a `<module>_facts_test.cpp` that feeds captured structs into
  the builder and asserts the document, including the id ↔ check-instance
  convention of section 4.6; `libs/onboarding/sync_test.cpp` for `facts_hash`
  in the state report and the new upload body.
- **Integration** (`tests/`, jest + supertest, `setupRestNscp`):
  - `rest-facts.test.ts`: with nothing enabled `GET /api/v2/facts` returns the
    empty document and `enabled: []` on both platforms; with `os` and
    `storage.volumes` enabled it returns the sets with the documented fields;
    grants (`403` unauthenticated, `monitoring` can read, only `full` can
    refresh); `POST …/refresh` bumps `revision`; the `/api/v2` index lists
    `facts_url`.
  - `fleet-sync.test.ts`: the fake server sees `facts_hash` in every state
    report, no `/agent/v1/facts` call while nothing is enabled, exactly one
    upload after enabling `os` through a bundle (fleet-managed enablement end
    to end), a re-upload when the server answers with a different
    `facts_hash`, graceful silence on `404`, and the existing privacy
    assertion untouched.
  - The test-console suite: `facts list` shows every registered id as
    disabled on a default config.
  - Each producer adds its case to the module's existing suite
    (`checksystem-commands.test.ts`, `checkdisk-*.test.ts`, …), guarded per
    OS, asserting the fields that are deterministic on CI (`os.family`, that
    `storage.volumes` contains the system volume, that `software.installed`
    is non-empty and every record has `id` and `version`).
- `python3 docs/hooks/notes.py --check` for the upgrade note.

## 8. Phases

Each phase is a mergeable branch; the feature is off until phase 3 gives it a
producer, and invisible until then except for the empty `/api/v2/facts`.

| Phase | Scope | Files (new / touched) | Depends on |
|---|---|---|---|
| **0** | Agree the wire contract with the fleet server (section 5) and the document rules (section 2). | this document | — |
| **1** | Core: repository, ABI, plugin manager, scheduler, settings section, core API, helper. `feature:` commit. | `service/fact_repository.hpp` + `_test.cpp`; `service/plugins/{plugin_interface.hpp,dll_plugin.*,zip_plugin.*,plugin_manager.*}`; `service/scheduler_handler.*`; `service/NSClient++.{cpp,h}`; `service/core_api.cpp`; `include/NSCAPI.h`; `include/nscapi/nscapi_core_wrapper.*`; `include/nscapi/nscapi_plugin_wrapper.hpp`; `include/nscapi/nscapi_facts_helper.{hpp,cpp}` + `_test.cpp`; `libs/plugin_api/CMakeLists.txt`; `build/python/create_plugin_module.py` | 0 |
| **2** | Consumers: REST controller + grants, console verbs, web UI page, API docs. | `modules/WEBServer/facts_controller.{hpp,cpp}`, `WEBServer.cpp`, `api_controller.cpp`; `include/client/simple_client.cpp`; `web/src/pages/Inventory.tsx`, `api/api.ts`, `Routes.tsx`, `components/SideMenu.tsx`; `tests/rest-facts.test.ts`; `docs/docs/api/rest/facts.md`, `index.md`, `docs/mkdocs.yml` | 1 |
| **3** | Producers wave 1: `agent` (core), `os`, `identity`, `hardware`, `network.interfaces` (CheckSystem + CheckSystemUnix), `storage.volumes` (CheckDisk). Concepts doc, upgrade note. | the three modules' `module.json`, main `.cpp`, a new `facts.cpp` per module + `facts_test.cpp`; `docs/docs/concepts/facts.md`; `docs/upgrades/next/facts.md` | 1 (2 for the UI to show it) |
| **4** | Fleet: `facts_hash` in the state report, `/agent/v1/facts` upload, server-requested re-upload, fake server route, integration tests, fleet docs step. | `libs/onboarding/sync.{cpp,hpp}` + `sync_test.cpp`; `include/onboarding/sync.hpp`; `service/fleet_sync.{cpp,hpp}`; `tests/fleet-sync.test.ts`; `docs/docs/setup/fleet.md` | 1, 3 (one real producer for the end-to-end test) |
| **5** | Producers wave 2, one branch each: `software.installed` + `software.hotfixes`, `services`, `security` (CheckSecurity), `docker` (CheckDocker), `tasks.scheduled` (CheckTaskSched), `updates.pending`. | per module as in phase 3 | 1 |
| **6** | Script producers (Python/Lua), then the context-on-failure feature. Separate plans. | — | 3 |

Phases 1 and 2 can be reviewed without any producer: the unit tests drive the
repository directly and the REST test asserts the empty document. Phase 3's
producers are independent of each other and can be split further.

## 9. Decisions and alternatives considered

- **Pull (`fetchFacts`) rather than push (`set_fact` like `set_tag`).** Push is
  less plumbing, but it cannot honour "nothing unless enabled" centrally
  (every producer would check the settings itself and one bug ships
  inventory), cannot offer an on-demand refresh, and gives every producer its
  own timer. Pull costs the generator/ABI change once, and then every producer
  is a pure function of "what is wanted".
- **JSON over the ABI, not a `facts.proto`.** The document is JSON on every
  consumer and `boost::json` is already in the core and the producing
  modules. A protobuf schema would be a second definition to keep in sync for
  no consumer.
- **Opt-in per set, not per module.** `software.installed` is the expensive,
  sensitive one; forcing it on to get `os` would make people leave facts off.
- **One `[/settings/facts]` section with one bool key per set**, not a
  per-module subsection and not a token list: bundles merge per key, the
  operator gets one place to audit what leaves the host, and the bundle that
  enables inventory does not need to know which module produces `os` on which
  platform.
- **Separate upload call, not facts in the state report.** The report is
  per-minute; the document is per-change. `facts_hash` in the report keeps the
  server's view consistent without the payload, and keeps the report's
  privacy test unchanged.
- **Records carry `id`.** Without a stable identity the server cannot diff a
  list, which is most of the value of history ("this package appeared on
  Tuesday").
- **Not building** a general inventory query language, per-set intervals, or
  facts-derived tags in the agent. All three are server-side or later.
