---
icon: "🔒"
modules: [packaging]
action: none
---
**Every build dependency is verified against a recorded digest.** Nothing to
do: this affects the release build, not a running agent. Each third-party file
the release builds download is checked against a SHA-256 or git commit pinned
in the repository before anything is built from it, and a missing entry fails
the build. See the
[security notice](../security/notices.md#verified-build-dependencies-and-a-signed-sbom-for-windows-releases).
