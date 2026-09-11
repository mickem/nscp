# Windows Packaging

This directory contains manifest templates and tooling that publish NSClient++
to community Windows package managers. They build on the existing CI pipeline
which already produces signed `.msi` and `.zip` artifacts and attaches them
to a GitHub release (see [`.github/workflows/release.yml`](../.github/workflows/release.yml)).

```
GitHub Release published (draft -> published, non-prerelease)
        │
        ├─► publish-winget.yml      → opens a PR against microsoft/winget-pkgs
        ├─► publish-chocolatey.yml  → choco pack + choco push
        └─► publish-scoop.yml       → commits manifest to the Scoop bucket repo
```

Each workflow downloads the artifacts from the GitHub release, recomputes the
`SHA256` of every file, renders the templates in this directory and then ships
them to the package manager. They can be invoked:

* automatically when a release is published (`on: release: types: [published]`),
* manually with a version input (`workflow_dispatch`),
* or as a reusable workflow (`workflow_call`) by another workflow.

## How publication is triggered

`release.yml` creates the GitHub release as a **draft** (`draft: true`). The
three publish workflows only fire when the release transitions from draft to
published, which today is a manual step performed in the GitHub web UI. That
means:

* Building artefacts and creating the release does **not** publish to any
  package manager — nothing happens until a human flips the draft.
* Releases marked `prerelease: true` are also skipped automatically.
* If `release.yml` is ever changed to publish non-draft releases directly,
  all three downstream workflows will fire on every release without further
  intervention. Treat that change with care.

Templates are checked out from `main`, not from the release tag. A template
fix can therefore land without re-tagging, but it also means a template
change merged after a release is what gets used when that release is later
re-published. If you need tag-locked templates, change
`actions/checkout@v6` in each publish workflow to use
`ref: ${{ github.event.release.tag_name }}`.

## First-time setup

The pipeline assumes three external accounts and one external repository
exist. None of this is created automatically; until each is wired up, the
corresponding workflow logs a warning and exits cleanly without failing the
release.

### WinGet (`publish-winget.yml`)

1. Decide on the bot account that will open PRs against
   [`microsoft/winget-pkgs`](https://github.com/microsoft/winget-pkgs). Using
   the maintainer's personal account works but is not recommended; a
   dedicated bot identity is cleaner.
2. Create a classic Personal Access Token with `public_repo` and `workflow`.
   `wingetcreate` never writes to `microsoft/winget-pkgs` directly — it forks
   it into the token's own account, pushes a branch there and opens the PR
   from that fork. So the token needs repository write access in *its own*
   account, which a fine-grained PAT scoped to `microsoft/winget-pkgs` does
   not grant. Adding the `workflow` scope is cheap insurance — `winget-pkgs`
   carries files under `.github/workflows`, which some token types are barred
   from writing.
3. Store it as the `WINGET_PR_TOKEN` repository secret.
4. The very first submission for `Mickem.NSClient` may need a manual
   `wingetcreate new` run to seed the package directory in `winget-pkgs`.
   Subsequent versions can use `wingetcreate submit` as the workflow does.

The bot's fork of `winget-pkgs` has to be current before `wingetcreate` can
push to it, and upstream takes hundreds of commits a day — a fork left alone
between releases drifts far enough that the fast-forward stops working. The
workflow syncs the fork itself before submitting, which keeps it current on
every release. If that step still fails, sign in as the bot and press "Sync
fork", or delete the fork outright (`wingetcreate` re-forks on the next run —
check the fork has no open PRs first).

### Chocolatey (`publish-chocolatey.yml`)

1. Create / claim the `nsclient` package on
   [community.chocolatey.org](https://community.chocolatey.org/). There are
   historical packages with similar names (including a stale `nscp`) —
   confirm ownership of the identifier before the first push.
2. Generate an API key at
   `https://community.chocolatey.org/account` for the publishing account.
3. Store it as the `CHOCOLATEY_API_KEY` repository secret.

### Scoop (`publish-scoop.yml`)

1. Create a Scoop bucket repository — the workflow defaults to
   `mickem/scoop-bucket`. Override via the `SCOOP_BUCKET_REPO` env in
   `publish-scoop.yml` if you fork. The repo must contain (or be allowed to
   contain) a `bucket/` directory at its root.
2. Create a PAT — classic with `repo`, or fine-grained with
   `Contents: write` on the bucket repo.
3. Store it as the `SCOOP_BUCKET_PAT` repository secret.

## Required GitHub Actions secrets

| Secret               | Used by                  | Notes                                                                                   |
| -------------------- | ------------------------ | --------------------------------------------------------------------------------------- |
| `WINGET_PR_TOKEN`    | `publish-winget.yml`     | Classic PAT with `public_repo` + `workflow`. Not a fine-grained PAT — the fork lives in the token's own account. |
| `CHOCOLATEY_API_KEY` | `publish-chocolatey.yml` | API key from `https://community.chocolatey.org/account`.                                |
| `SCOOP_BUCKET_PAT`   | `publish-scoop.yml`      | PAT with write access to the Scoop bucket repository (`mickem/scoop-bucket` by default). |

If a required secret is missing the workflow logs a warning and exits without
failing the release, so the pipeline can land before all external accounts
are wired up.

## Layout

```
packaging/
├── README.md
├── winget/
│   ├── Mickem.NSClient.installer.yaml.tmpl     # installer URLs + SHA256s
│   ├── Mickem.NSClient.locale.en-US.yaml.tmpl  # description, license, tags
│   └── Mickem.NSClient.yaml.tmpl               # version manifest
├── chocolatey/
│   ├── nsclient.nuspec.tmpl
│   └── tools/
│       ├── chocolateyInstall.ps1.tmpl
│       ├── chocolateyUninstall.ps1
│       └── VERIFICATION.txt.tmpl
└── scoop/
    └── nsclient.json.tmpl
```

## Placeholders

`render_templates.py` fails fast if a template references a placeholder that
the invoking workflow did not provide. Each workflow therefore exposes only
the subset of placeholders it needs.

### Provided to every workflow

| Placeholder         | Meaning                                              |
| ------------------- | ---------------------------------------------------- |
| `{{VERSION}}`       | Release version (e.g. `0.6.1`).                      |
| `{{RELEASE_TAG}}`   | The GitHub release tag (typically same as version).  |
| `{{RELEASE_DATE}}`  | ISO-8601 date the release was rendered.              |

### `publish-winget.yml` and `publish-chocolatey.yml`

| Placeholder              | Meaning                              |
| ------------------------ | ------------------------------------ |
| `{{URL_MSI_X64}}`        | Signed MSI URL for x64.              |
| `{{URL_MSI_X86}}`        | Signed MSI URL for x86.              |
| `{{SHA256_MSI_X64}}`     | Uppercase SHA256 of the x64 MSI.     |
| `{{SHA256_MSI_X86}}`     | Uppercase SHA256 of the x86 MSI.     |

### `publish-winget.yml` only

| Placeholder                   | Meaning                                          |
| ----------------------------- | ------------------------------------------------ |
| `{{PRODUCT_CODE_MSI_X64}}`    | `ProductCode` read out of the x64 MSI.           |
| `{{PRODUCT_CODE_MSI_X86}}`    | `ProductCode` read out of the x86 MSI.           |
| `{{RELEASE_NOTES}}`           | Body of the GitHub release, as Markdown.         |

`Product Id` is `*` in `Product.wxs`, so every build mints a fresh
`ProductCode`. The workflow downloads each MSI and reads the property out of
its `Property` table before rendering; WinGet uses it to tie the installed
package to its Add/Remove Programs entry for upgrade and uninstall.

`{{RELEASE_NOTES}}` is a multi-line value, supplied with `--extra-file`
rather than `--extra`. A placeholder that is alone on its line expands with
that line's indentation applied to every continuation line, which is what
drops it straight into a YAML block scalar:

```yaml
ReleaseNotes: |-
  {{RELEASE_NOTES}}
```

`--extra-file-drop-section Detailed` removes the release body's
`## Detailed changes` section and everything nested under it, keeping the
intro, `## Highlights`, `## Upgrade notes` and the changelog link. That is the
part of the notes a `winget show` reader wants, and it is what keeps the field
inside WinGet's cap: 0.19.0's body is 22774 characters whole and 8192 without
that section, 0.20.0's 13512 and 5991. Anything still over the cap is
truncated to 10000 characters (`--extra-file-max-chars`) on a line boundary,
since WinGet rejects an oversized `ReleaseNotes` outright rather than trimming
it — but with the section dropped that is a backstop, not the normal path.

One wart to know about: a bullet under `## Upgrade notes` that refers back to
`## Detailed changes` ("see the table above") loses its referent. Keep upgrade
notes self-contained when writing the release.

### `publish-scoop.yml`

| Placeholder              | Meaning                              |
| ------------------------ | ------------------------------------ |
| `{{URL_ZIP_X64}}`        | x64 ZIP archive URL.                 |
| `{{URL_ZIP_X86}}`        | x86 ZIP archive URL.                 |
| `{{SHA256_ZIP_X64}}`     | Uppercase SHA256 of the x64 ZIP.     |
| `{{SHA256_ZIP_X86}}`     | Uppercase SHA256 of the x86 ZIP.     |

### Keeping metadata consistent between versions

`winget-pkgs` runs a metadata-consistency check that compares a submission
against the previously published version and flags every property the new
manifest drops. So a property that has ever shipped — `ReleaseNotes`,
`Documentations`, `InstallerLocale`, `Scope`, `InstallerSwitches`,
`ProductCode` — has to keep being rendered. Before deleting anything from a
template, check it against the published manifest under
`manifests/m/Mickem/NSClient/` upstream.

The legacy XP MSI (`NSCP-<ver>-Win32-legacy-xp.msi`) is produced by
`release.yml` but is **not** plumbed through these workflows; XP coverage
via winget/chocolatey is not a goal. Add `URL_MSI_X86_LEGACY=...` /
`SHA256_MSI_X86_LEGACY=...` to the relevant `--asset` invocation if that
ever changes.

## Manual / dry-run publishing

Each workflow exposes a `dry_run` boolean input. With `dry_run: true` the
manifest is rendered, validated and uploaded as a workflow artifact but
nothing is pushed to the upstream package manager. This is the recommended
way to test changes to the templates before tagging a real release.
