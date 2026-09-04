# Security notices

This directory is the source of the [Security notices](../docs/security/notices.md)
page. Every notice is its own file, so branches that each add one never edit
the same file and never conflict; the page is assembled when the documentation
is built (`docs/hooks/notes.py`, run as an mkdocs hook — the same hook that
assembles the [Upgrading](../upgrades/README.md) page).

## Adding a notice

Create `docs/security/<slug>.md`:

```markdown
---
title: "NRPE: decoded-argument metachar guard and optional version banner"
fixed_in: 0.18.0
severity: "Low"
modules: [NRPEServer, NRPEClient]
---
A review of the `NRPEServer` / `NRPEClient` modules produced a set of
defense-in-depth fixes. …

**What to do:** nothing required. To harden further, set
`expose version = false`.
```

The front matter:

| Key             | Required | Meaning |
|-----------------|----------|---------|
| `title`         | yes      | Rendered as the notice's `###` heading. Its slug is the anchor other pages link to (`notices.md#nrpe-decoded-argument-…`), so keep it stable once published. |
| `modules`       | yes      | The modules the change concerns, as for [upgrade notes](../upgrades/README.md): module directory names or an area (`core`, `filters`, `packaging`, `docs`). |
| `fixed_in`      | no       | The release that carries the fix (`0.18.0`). Drives the version filter and the newest-first order; omit only for an advisory against a line that has no fix release. |
| `fixed_in_note` | no       | Parenthesised after the version: `bundles OpenSSL 3.5.8`. |
| `severity`      | no       | Free text: `Low`, `Medium`, `High for NSCA-NG cert-mode targets, Low–Medium for everything else`, … |
| `reported_by`   | no       | Markdown, e.g. `"[yagust](https://github.com/yagust)"`. |
| `advisory`      | no       | Only for a **published** advisory (a CVE / GHSA). A mapping with `id`, `cve`, `severity`, `affected` and `summary`, each a Markdown string; it becomes the notice's row in the advisories table, and the notice is filed under *Published advisories* instead of *Hardening changes*. |

`fixed_in`, `severity` and `reported_by` render as the
`**Fixed in:** … · **Severity:** …` line under the heading, so do not repeat
it in the body. The body is ordinary Markdown: paragraphs, bullet lists,
`####` sub-headings (nothing shallower — the notice is a `###`), and a closing
`**What to do:**` paragraph by convention. Links are resolved from
`docs/docs/security/`, so link to other pages as the notices page does:
`../setup/upgrading.md#0180`, `../scenarios/nrpe.md`.

Notices are ordered newest `fixed_in` first, then by file name within a
release (the migrated history carries a `NNN-` prefix; new files sort after it
alphabetically, or take a prefix of their own when the order matters).

Check your notice without building the site:

```
python3 docs/hooks/notes.py --check
```

## What goes here

Every security-relevant change, including hardening handled without a CVE.
The matching [upgrade note](../upgrades/README.md) carries 🔒 and links to the
notice's anchor. For an embargoed fix, add the notice in the same branch as
the fix so it becomes public only when the fix does.
