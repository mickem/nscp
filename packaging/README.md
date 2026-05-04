# Windows Packaging

This directory contains manifest templates and tooling that publish NSClient++
to community Windows package managers. They build on the existing CI pipeline
which already produces signed `.msi` and `.zip` artifacts and attaches them
to a GitHub release (see [`.github/workflows/release.yml`](../.github/workflows/release.yml)).

```
GitHub Release published
        │
        ├─► publish-winget.yml      → opens a PR against microsoft/winget-pkgs
        ├─► publish-chocolatey.yml  → choco pack + choco push
        └─► publish-scoop.yml       → commits manifest to the Scoop bucket repo
```

The publishing workflows always download the artifacts from the published
GitHub release, recompute the `SHA256` of every file, render the templates in
this directory and then ship them to each package manager. They can be invoked:

* automatically when a release is published (`on: release: types: [published]`),
* manually with a version input (`workflow_dispatch`),
* or as a reusable workflow (`workflow_call`) by another workflow.

Releases marked as `draft` or `prerelease` are skipped automatically.

## Required GitHub Actions secrets

| Secret               | Used by                | Notes                                                                               |
| -------------------- | ---------------------- | ----------------------------------------------------------------------------------- |
| `WINGET_PR_TOKEN`    | `publish-winget.yml`   | Personal access token (classic, `public_repo`) for the bot account that opens PRs.  |
| `CHOCOLATEY_API_KEY` | `publish-chocolatey.yml` | API key from `https://community.chocolatey.org/account` for the publishing account.|
| `SCOOP_BUCKET_PAT`   | `publish-scoop.yml`    | PAT (or deploy key) with write access to the Scoop bucket repository.               |

If a required secret is missing the workflow logs a warning and exits without
failing the release; this means the release pipeline can be deployed before all
external accounts are wired up.

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

Placeholders rendered at publish time:

* `{{VERSION}}`              – release version (e.g. `0.6.1`).
* `{{RELEASE_TAG}}`          – the GitHub release tag (typically same as version).
* `{{URL_MSI_X64}}`          – signed MSI URL for x64.
* `{{URL_MSI_X86}}`          – signed MSI URL for x86.
* `{{URL_MSI_X86_LEGACY}}`   – signed MSI URL for the legacy XP x86 build.
* `{{URL_ZIP_X64}}`          – x64 ZIP archive URL.
* `{{URL_ZIP_X86}}`          – x86 ZIP archive URL.
* `{{SHA256_MSI_X64}}`       – uppercase SHA256 of the x64 MSI.
* `{{SHA256_MSI_X86}}`       – uppercase SHA256 of the x86 MSI.
* `{{SHA256_MSI_X86_LEGACY}}`– uppercase SHA256 of the legacy x86 MSI.
* `{{SHA256_ZIP_X64}}`       – uppercase SHA256 of the x64 ZIP.
* `{{SHA256_ZIP_X86}}`       – uppercase SHA256 of the x86 ZIP.
* `{{RELEASE_DATE}}`         – ISO-8601 date the release was published.

## Manual / dry-run publishing

Each workflow exposes a `dry_run` boolean input. With `dry_run: true` the
manifest is rendered, validated and uploaded as a workflow artifact but
nothing is pushed to the upstream package manager. This is the recommended
way to test changes to the templates.
