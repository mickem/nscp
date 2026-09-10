---
title: "REST script uploads were staged at a predictable path"
fixed_in: 0.19.0
severity: "Medium"
modules: [WEBServer]
action: none
---
`PUT /api/v2/scripts/…` (admin only) wrote the body to `${temp}/<name>` —
`/tmp`, or `C:\Windows\Temp` for a SYSTEM service — with an unchecked
truncating write, then imported it as a command. A local user who created that
file first won a race against the copy, or won outright where the service's
overwrite was refused and the failure ignored, and the planted content then ran
as the service account.

Stock DEB/RPM installs were not exploitable for code execution: the service
runs as `nsclient` and the script root is root-owned, so the import fails.

Uploads now go to a randomly named file, created exclusively and owner-only,
with every write checked and the file removed once the module has consumed it.

**What to do:** nothing; the endpoint's contract is unchanged.
