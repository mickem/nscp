---
icon: "📦"
modules: [packaging, PythonScript]
action: none
---
**Windows on ARM is packaged again, without the Python module.** Releases carry
`NSCP-<version>-ARM64.msi` and `NSCP-<version>-ARM64.zip` once more; ARM64 was
built for one release in April 2026 and then withdrawn because the build
failed. Nothing to do on x64 or 32-bit Windows, and an ARM64 machine running
the x64 package under emulation keeps working — the native package is simply
faster.

One difference from the x64 package: **the ARM64 build does not include
`PythonScript`**, nor the embedded Python runtime that comes with it. The
package is cross-compiled on an x64 runner and there is no ARM64 CPython to
embed, so the module and its `python*.dll` / `python*.zip` are absent from the
MSI and the Python Scripting feature does not appear in the installer. An
ARM64 host that needs internal Python scripts should run the x64 package under
emulation. `CheckExternalScripts` is unaffected and can still run a
system-installed `python.exe`.

One more difference, and this one needs action on a fresh machine: **the ARM64
MSI does not install the Visual C++ runtime**. The x64 and 32-bit installers
bundle it as a merge module, but Microsoft ships no ARM64 merge module for the
toolset this is built with, so there is nothing to bundle. Install the
[Visual C++ Redistributable for ARM64](https://aka.ms/vs/17/release/vc_redist.arm64.exe)
before the agent, or the service will fail to start with a missing
`vcruntime140.dll`. Most machines already have it — anything that has run a
recent ARM64 desktop application will — and upgrades over an existing install
are unaffected.
