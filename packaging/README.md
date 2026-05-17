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
2. Create a classic Personal Access Token with the `public_repo` scope (or a
   fine-grained PAT with `Pull requests: write` on `microsoft/winget-pkgs`).
3. Store it as the `WINGET_PR_TOKEN` repository secret.
4. The very first submission for `Mickem.NSClient` may need a manual
   `wingetcreate new` run to seed the package directory in `winget-pkgs`.
   Subsequent versions can use `wingetcreate submit` as the workflow does.

### Chocolatey (`publish-chocolatey.yml`)

1. Create / claim the `nscp` package on
   [community.chocolatey.org](https://community.chocolatey.org/). There are
   historical packages with similar names — confirm ownership of the
   identifier before the first push.
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
| `WINGET_PR_TOKEN`    | `publish-winget.yml`     | Classic PAT with `public_repo`, or fine-grained with `Pull requests: write` on winget-pkgs. |
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
│   ├── nscp.nuspec.tmpl
│   └── tools/
│       ├── chocolateyInstall.ps1.tmpl
│       ├── chocolateyUninstall.ps1
│       └── VERIFICATION.txt.tmpl
└── scoop/
    └── nscp.json.tmpl
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

### `publish-scoop.yml`

| Placeholder              | Meaning                              |
| ------------------------ | ------------------------------------ |
| `{{URL_ZIP_X64}}`        | x64 ZIP archive URL.                 |
| `{{URL_ZIP_X86}}`        | x86 ZIP archive URL.                 |
| `{{SHA256_ZIP_X64}}`     | Uppercase SHA256 of the x64 ZIP.     |
| `{{SHA256_ZIP_X86}}`     | Uppercase SHA256 of the x86 ZIP.     |

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
