---
icon: "🔒"
modules: [packaging]
action: none
---
**The Windows release build pins every third-party action to a commit and
verifies what it downloads.** Nothing to do — this concerns how the released
artifacts are built, not an installed agent. Third-party actions are pinned to
commit SHAs (one ran from a branch, in the same job that holds the code-signing
credentials), Google Test is checked against the commit its tag names, and every
downloaded dependency goes through `.github/dependency-checksums.txt`: a
mismatched digest fails the build, and so does bumping a version without
recording one. See the
[security notice](../security/notices.md#windows-release-build-actions-pinned-and-a-checksum-gate-for-downloaded-dependencies).
