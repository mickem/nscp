---
icon: "🔒"
modules: [packaging]
action: none
---
**Every DLL the Windows installer ships is Authenticode-signed.** Nothing to
do. The modules, the NSClient++ libraries and the bundled third-party libraries
now carry the same signature as `nscp.exe`, so an AppLocker or App Control
publisher rule for NSClient++ covers them too, and endpoint protection sees
signed files instead of unsigned ones. See the
[security notice](../security/notices.md#windows-releases-every-shipped-binary-signed-and-signing-credentials-only-in-the-release-build).
