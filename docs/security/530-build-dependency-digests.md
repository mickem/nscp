---
title: "Build dependencies: digests recorded, and tag archives replaced by commit-pinned clones"
fixed_in: 0.23.0
severity: "Low: build-chain integrity, no impact on a running agent"
modules: [packaging]
action: none
---
The checksum gate added in the previous release
([notice](#windows-release-build-actions-pinned-and-a-checksum-gate-for-downloaded-dependencies)) was in place but
empty: every dependency line in `.github/dependency-checksums.txt` read
`unrecorded`, so each download warned and continued rather than verifying
anything. Everything on that list ends up inside artifacts this project
Authenticode-signs, so a compromised or re-uploaded upstream asset would have
been signed and shipped.

The digests are now recorded for the release assets the build downloads:
OpenSSL (cross-checked against the `.sha256` OpenSSL publishes next to the
asset), protobuf, Crypto++, miniz, and the prebuilt `check_nsclient.exe` for
both architectures the Windows workflow builds.

Three dependencies were fetched as the zip or tarball GitHub generates for a
tag — TinyXML2, Mongoose and MariaDB Connector/C. That archive is not a release
artifact anyone signs, and the tag it is generated from can be moved. They are
now cloned at the tag and verified against the recorded commit, which is what
googletest already did: a tag is a movable pointer, a commit is not.

Lua remains unrecorded. Its digest has to be taken on a machine that can reach
`www.lua.org` and cross-checked against the checksums Lua publishes on the same
page; recording it from an unverified fetch would only write down what the
build was served, which is the thing this file exists not to trust.

The `actions/cache` uses in the dependency actions and the Linux workflows are
now pinned to commit ids rather than floating tags, matching the third-party
actions pinned in the previous release.

**What to do:** nothing. This affects the release build only. Note that bumping
a dependency version now requires adding its line to
`.github/dependency-checksums.txt` first — a missing line fails the build, on
purpose.
