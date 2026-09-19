---
icon: "📦"
modules: [packaging, core]
action: conditional
---
**Boost.JSON is linked from the Boost build instead of being compiled in.**
`nscp_json.dll` (`libnscp_json.so` on Linux) is gone: the modules now use the
Boost.JSON library like the other Boost libraries, so the MSI ships
`boost_json.dll` in its place and the Linux packages use the system one. The
legacy Windows XP build still links everything statically. Nothing to do when
installing from the MSI or from the Debian/RedHat packages. A **hand-rolled
deployment** that copies the libraries next to `nscp.exe` itself must copy
`boost_json.dll` as well, or the modules that speak JSON will fail to load.
