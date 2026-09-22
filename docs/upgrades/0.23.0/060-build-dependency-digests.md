---
icon: "🔒"
modules: [packaging]
action: none
---
**The Windows release build now verifies what it downloads.** Nothing to do:
this affects the release build, not a running agent. The checksum gate added
last release was in place but every line in `.github/dependency-checksums.txt`
read `unrecorded`, so each download warned and continued. The digests are
recorded now for OpenSSL (cross-checked against OpenSSL's own published
`.sha256`), protobuf, Crypto++, miniz and the prebuilt `check_nsclient.exe`.
TinyXML2, Mongoose and MariaDB Connector/C are no longer fetched as
GitHub-generated tag archives but cloned and verified against a recorded
commit, the way googletest already was. Lua stays unrecorded until its digest
can be taken against the checksums lua.org publishes. Bumping a dependency
version now means adding its line to that file first, because a missing line
fails the build. See the
[security notice](../security/notices.md#build-dependencies-digests-recorded-and-tag-archives-replaced-by-commit-pinned-clones).
