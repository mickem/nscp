---
icon: "🔒"
modules: [packaging]
action: none
---
**Every build dependency is now verified, including Boost on every Windows
build.** Nothing to do: this affects the release build, not a running agent.
The x86, x64 and legacy XP builds compiled Boost from an unverified download;
Boost and Lua digests were still unrecorded; and the Linux packages shipped
`check_nsclient` without a check. All three are closed. See the
[security notice](../security/notices.md#build-dependencies-boost-verified-on-every-windows-build-and-no-download-left-unrecorded).
