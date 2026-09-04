---
icon: "🐧"
modules: [packaging]
---
**RHEL/SUSE:** workaround `ca=` arguments can be dropped — `${ca-path}` now
resolves on its own (the explicit form still works). Packagers cross-building
for another distribution should set `-DCONFIG_CA_PATH=`.
