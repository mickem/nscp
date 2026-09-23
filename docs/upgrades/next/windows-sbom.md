---
icon: "📦"
modules: [packaging]
action: none
---
**Windows downloads carry a software bill of materials.** Nothing to do. Each Windows release now publishes
`NSCP-<version>-<platform>.cdx.json` next to its MSI and zip, and the zip holds
the same file as `sbom.cdx.json` plus a `SHA256SUMS` of its contents. The SBOM
is [CycloneDX](https://cyclonedx.org) 1.6 JSON and lists every third-party
component the build compiled or bundled, with the upstream URL and the SHA-256
or git commit the build verified it against. See
[Verifying the download](installing.md#verifying-the-download).
