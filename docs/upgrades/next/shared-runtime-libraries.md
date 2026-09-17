---
icon: "📦"
modules: [packaging, core]
action: conditional
---
**Common code moved out of the individual plugins into shared libraries.** The
Windows install is about a fifth smaller, because code that every plugin used
to compile privately is now in one place. Three new libraries ship next to
`nscp.exe`: `nscp_net.dll` (socket and TLS helpers), `nscp_client.dll` (the
client-side command line handling the sender modules share) and `nscp_json.dll`
(Boost.JSON); `plugin_api.dll` also grew, having taken over the NSCAPI helpers,
the settings helpers and the program-options helpers that each plugin used to
carry its own copy of.

Nothing to do when installing from the MSI or from the Debian/RedHat packages —
they install the new libraries for you, and the modules themselves are
unchanged from the outside. Only a **hand-rolled deployment** needs to check:
if you copy `modules\*.dll` into place yourself rather than installing a
package, copy the new libraries from the installation root as well, or the
modules that need them will fail to load.
