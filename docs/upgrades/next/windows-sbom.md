---
icon: "📦"
modules: [packaging]
action: none
---
**Windows downloads carry a software bill of materials, and every release
asset is attested.** Nothing to do. Each Windows release now publishes
`NSCP-<version>-<platform>.cdx.json` next to its MSI and zip, and the zip holds
the same file as `sbom.cdx.json` plus a `SHA256SUMS` of its contents. The SBOM
is [CycloneDX](https://cyclonedx.org) 1.6 JSON and lists every third-party
component the build compiled or bundled, with the upstream URL and the SHA-256
or git commit the build verified it against. The bundled check_nsclient
brings its own SBOM, nested under its entry. Release assets are attested by
the release workflow, so `gh attestation verify <file> --repo mickem/nscp`
shows which workflow run and commit produced a file. See
[Verifying the download](installing.md#verifying-the-download).
