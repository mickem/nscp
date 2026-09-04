# Upgrade notes

This directory is the source of the [Upgrading](../docs/setup/upgrading.md)
page. Every note is its own file, so branches that each add a note never edit
the same file and never conflict; the page is assembled when the documentation
is built (`docs/hooks/notes.py`, run as an mkdocs hook). Security notices work
the same way, see [`docs/security/`](../security/README.md).

## Adding a note

Create `docs/upgrades/<version>/<slug>.md`, where `<version>` is the release
the change ships in (the next release while it is unreleased) and `<slug>` is a
short kebab-case name for the change:

```markdown
---
icon: "🔒"
modules: [NRPEServer]
---
**NRPE hardening: a new `expose version` setting.** Nothing to do on a default
install. `NRPEServer` gained `expose version` (default `true`); set it to
`false` to answer the unauthenticated `_NRPE_CHECK` ping with a generic
message. See the
[security notice](../security/notices.md#nrpe-decoded-argument-metachar-guard-optional-version-banner-and-consistency-fixes).
```

The front matter:

| Key        | Required | Meaning |
|------------|----------|---------|
| `modules`  | yes      | The modules the change concerns: module directory names (`NRPEServer`, `CheckSystem`, …) or one of the areas below. The reader filters the page on these, so name every module a reader must have enabled to be affected — and nothing else. |
| `icon`     | no       | Emoji shown in front of the title (🔒 security, 💥 breaking, 🔧 settings/CLI, 📊 perfdata, ⏱️ timing, 📤 submission, …). Several may be combined (`"📨 🔒"`). |
| `security` | no       | `true`/`false`; defaults to whether `icon` contains 🔒. Marks the note for the *security-relevant only* switch. |

Areas, for changes that are not tied to one module:

| Area        | Use for |
|-------------|---------|
| `core`      | the service itself, the settings engine, the `nscp` command line, permissions, includes/paths |
| `filters`   | the shared filter/check framework (keywords, thresholds, syntax templates, perf-config) — affects every check |
| `packaging` | installers, packages, bundled dependencies, licensing |
| `docs`      | the documentation build |

The body is ordinary Markdown. It renders as one bullet on the page, so:

* The first line must start with a bold **title** (ending in a period), the
  same style as the GitHub release notes.
* Nested lists, tables and fenced code blocks are fine; write them at column 0.
* Links are resolved from `docs/docs/setup/`, so link to other pages as the
  Upgrading page does: `../security/notices.md#…`, `../concepts/permissions.md`.
* Do not add headings — the version heading is generated.

Notes within a version are ordered by file name. The migrated history carries
a `NNN-` prefix to keep its original order; new files sort after it
alphabetically, or take a prefix of their own when the order matters (security
notes first, say).

Check your note without building the site:

```
python3 docs/hooks/notes.py --check
```

It rejects a missing front matter, an unknown module name and a body without a
bold title. `--render docs/docs` writes the assembled pages over the stubs, for
a documentation build that does not run mkdocs from this repository.

## What goes here

The same content as the release's *Upgrade notes*: breaking changes, behaviour
changes that need operator action, and new or removed defaults. A
security-relevant change also gets an entry on
[Security notices](../docs/security/notices.md), with the note here carrying 🔒
and linking to it. See `CLAUDE.md` at the repository root for the release-note
conventions.
