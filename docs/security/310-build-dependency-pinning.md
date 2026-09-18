---
title: "Windows release build: actions pinned and a checksum gate for downloaded dependencies"
fixed_in: next
severity: "Medium (build integrity)"
modules: [packaging]
action: none
---
The Windows build fetches every native dependency — OpenSSL, protobuf,
Crypto++, Lua, TinyXML2, Mongoose, miniz, the MariaDB connector — plus the
prebuilt `check_nsclient.exe` that ships inside the MSI, and then
Authenticode-signs the result. TLS to the download host proves who served the
bytes, not what the bytes are: a compromised or re-uploaded upstream release
asset arrives under a valid certificate and would be signed and shipped.

Separately, third-party actions ran in the same job that receives the
code-signing credentials, and one of them was pinned to a branch
(`@master`) rather than to a revision — a push to that repository would have
run in the job holding the secrets.

This release:

* pins every third-party action to a commit SHA (`ilammy/msvc-dev-cmd`,
  `shogo82148/actions-setup-perl`, `mickem/build-boost`,
  `azure/trusted-signing-action`, `codacy/git-version`,
  `codespell-project/actions-codespell`, `ncipollo/release-action`) and pins
  the Perl distribution to a version instead of `latest`;
* replaces the third-party file-writing action in the signing job with a shell
  heredoc, so nothing third-party runs there beyond the signing action itself;
* verifies Google Test against the commit its tag names, rather than trusting
  the tag;
* adds `.github/dependency-checksums.txt` and the two verification scripts next
  to it, run from every download step. A recorded digest that does not match
  fails the build, and a version with no line at all fails too — so bumping a
  dependency cannot silently skip verification.

The digests of the eight downloaded dependencies and of `check_nsclient.exe`
are marked `unrecorded` in that file for now: recording one means fetching the
artifact and cross-checking it against the upstream project's own published
checksum or signature, which has to be done from a machine that can reach those
hosts. Until each is filled in, its download warns in the build log instead of
being verified.

**What to do:** nothing — this concerns how the released artifacts are built,
not an installed agent.
