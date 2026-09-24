---
title: "Build dependencies: Boost verified on every Windows build, and no download left unrecorded"
fixed_in: next
severity: "Low: build-chain integrity, no impact on a running agent"
modules: [packaging]
action: none
---
After the previous release every dependency line in
`.github/dependency-checksums.txt` carried a digest or a commit except two, and
one of the verified dependencies was not actually verified on most builds:

* **Boost was unverified on x86, x64 and the legacy XP build.** Those builds
  compile Boost through the external `mickem/build-boost` action, which
  downloads the source archive itself and never consults the manifest. Only the
  ARM64 build, which uses the in-repo action, called the checksum gate — and
  there the Boost line read `unrecorded`, so it warned and continued. A new
  step downloads `boost_1_86_0.tar.gz` into the runner workspace and verifies it
  before any Boost build starts; both Boost actions look there first and use
  that copy instead of downloading their own.
* **Boost and Lua digests are recorded.** Both are the values the upstream
  projects publish, cross-checked independently: the Boost `.tar.bz2` from the
  same mirror matches the digest conan-center-index records, and both Boost
  archives decompress to the same tar; the Lua tarball is byte-identical to the
  Debian/Ubuntu `lua5.4_5.4.8.orig.tar.gz`, whose `.dsc` records the same
  SHA-256.
* **The Linux `check_nsclient` binary is pinned.** The Debian and RedHat builds
  downloaded it and shipped it inside the `.deb` and `.rpm` without a check. It
  now goes through the same gate as the Windows binary, with one line per
  architecture.

* **Cached dependency builds are keyed on the recorded digest.** The build
  caches for OpenSSL, protobuf, Crypto++, Lua, MariaDB Connector/C and the
  ARM64 Boost were keyed on the version alone, and Lua, Crypto++ and MariaDB
  skip the download and its check entirely on a cache hit. A cache built while
  Lua or Boost still read `unrecorded` would therefore have been restored for
  good. The recorded SHA-256 or commit is now part of every such key, so a
  cached build is only reused for exactly the input it was verified against,
  and any change to that value forces a fresh, verified build.

No line in the manifest reads `unrecorded` any more, and a new check in the
Windows build fails if one ever does.

Windows builds now also ship a CycloneDX software bill of materials that
records, for every third-party component, the version, the upstream URL and
the digest or commit it was verified against, and the release attests every
asset and binds each SBOM to its zip and MSI. See
[Verifying the download](../setup/installing.md#verifying-the-download).

**What to do:** nothing. This affects the release build only.
