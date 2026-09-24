---
title: "Verified build dependencies and a signed SBOM for Windows releases"
fixed_in: next
severity: "Low: build-chain integrity, no impact on a running agent"
modules: [packaging]
action: none
---
Every third-party dependency the release builds download is pinned in
`.github/dependency-checksums.txt` and checked before anything is built from
it: a SHA-256 for a downloaded archive or binary, and a commit for a
dependency cloned from git, since a tag can be moved and a commit cannot. The
builds ship these dependencies inside Authenticode-signed artifacts, and TLS
only proves who served a file, not what it contains, so the recorded value is
what decides whether the build goes ahead.

* **Windows:** OpenSSL, Boost, Lua, Protocol Buffers, Crypto++, miniz,
  TinyXML-2, Mongoose, MariaDB Connector/C, GoogleTest and the bundled
  `check_nsclient.exe`.
* **Linux:** the `check_nsclient` binary shipped inside the `.deb` and `.rpm`
  packages. The other libraries come from the distribution, which verifies its
  own packages.

Where an upstream project publishes a checksum, the recorded value is that
checksum: OpenSSL's `.sha256` next to the tarball, and the SHA-256 values on
the Boost and Lua download pages. Each was also cross-checked against an
independent source: Boost against the digest conan-center-index records for
the same release, whose other archive format decompresses to the identical
tar, and Lua against the byte-identical Debian/Ubuntu source tarball, whose
`.dsc` records the same SHA-256.

How the build enforces it:

* **A missing or unrecorded entry fails the build.** Bumping a dependency
  version means adding its line to the manifest first.
* **Cached builds are keyed on the recorded value.** A cached dependency build
  is only reused for exactly the input it was verified against, and any change
  to that value forces a fresh, verified build.
* **Boost is built by the project's own action** on every Windows platform,
  with the archive verified on every run and the MSVC toolset pinned to the one
  that links it (v141 for x86, x64 and XP).

The result is published with every Windows release: a CycloneDX software bill
of materials lists every third-party component with its version, upstream URL
and the digest or commit it was verified against, and the release workflow
attests every asset and binds each SBOM to its zip and MSI. See
[Verifying the download](../setup/installing.md#verifying-the-download).

**What to do:** nothing. This affects the release build only.
