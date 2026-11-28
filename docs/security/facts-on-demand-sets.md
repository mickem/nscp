---
title: "Facts: on-demand sets take a per-set grant, and a facts wildcard role reads script listings"
fixed_in: next
severity: "Low"
modules: [WEBServer, core, CheckExternalScripts, PythonScript, LUAScript, CheckSystem]
action: conditional
---
`/api/v2/facts` now also serves **on-demand** fact sets: the script files of
each script runtime (`scripts.external`, `scripts.python`, `scripts.lua`) and,
on Windows, the performance counter catalog (`pdh.objects`,
`pdh.instances.<object>`). They are collected when read and never stored or
uploaded. This notice records who can reach them.

- **`facts.get` reads what it read before, and nothing new.** The grant
  covers the stored document. A caller holding it who asks for an on-demand
  set is refused with a 403 that names the grant it lacks, and the set is not
  collected for that caller: the refusal is decided before any module is
  asked.
- **An on-demand set takes `facts.get.<set>`** (`facts.get.scripts`,
  `facts.get.pdh`). No built-in role holds one except `full` (`*`). The first
  path segment is only used as a grant suffix when it is a valid set name, so
  a path such as `*` cannot be turned into a wildcard grant check.
- **A role granted `facts.*` does reach them.** A wildcard matches every
  segment after it, so a custom role that was given `facts.*` to read and
  refresh the inventory can, after this change, also read the script listings
  and the counter catalog. This is the one case in which an existing role
  reads more than it did.
- **The listing only.** `scripts.*` carries file names, and for external
  scripts the names they are registered under. It never carries a script's
  source or the command line a script runs; those stay behind
  `scripts.get.<runtime>` and the settings grants.
- **No free-form input reaches collection.** The one parameter
  (`pdh.instances.<object>`) is at most 256 printable characters, is compared
  only with the object names PDH itself enumerates, and it is the enumerated
  name that is then handed to PDH. Nothing a caller sends is parsed as
  options, a counter path or a wildcard.
- **A client cannot drive collection in a loop.** Answers are served from a
  five-second cache, and collections run one at a time.
- **Never shipped.** On-demand sets are not in the document an enrolled host
  uploads to its fleet server, and are not hashed into it.

Separately, two REST routes stopped running a module's command line to
produce a read-only listing: `GET /api/v2/scripts/<runtime>` and
`GET /api/v2/metadata/counters` built an exec request (`list --json`,
`pdh --list --json --all --no-instances`) and passed the output through. They
now read the same fact sets. Their grants and responses are unchanged, and
there is one caller fewer of the exec path.

**What to do:** nothing on a default install. If a custom role holds `facts.*`
and should not see script listings or the counter catalog, replace the
wildcard with the grants it needs (`facts.get`, `facts.refresh`). To let a
role read an on-demand set, add `facts.get.scripts` or `facts.get.pdh` to it.
