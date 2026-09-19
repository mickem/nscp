---
icon: "📦"
modules: [packaging, core]
action: conditional
---
**Common code moved out of the individual plugins into shared libraries.**
Code that every plugin used to compile privately now lives in one place, so the
install is smaller: about a fifth on Debian and RedHat, and on Windows every
TLS-capable module sheds the private copy of OpenSSL that used to make up most
of it. New libraries ship next to `nscp.exe`: `nscp_net.dll` (socket and TLS
helpers), `nscp_client.dll` (the client-side command line handling the sender
modules share), `boost_json.dll` (Boost.JSON, which every JSON-speaking module
used to compile into itself and which is now linked from the Boost build like
the other Boost libraries), and OpenSSL itself as
`libcrypto-3-x64.dll` and `libssl-3-x64.dll` (`libcrypto-3.dll` and
`libssl-3.dll` on 32-bit) — one copy for the whole service, where before each
of NRPE, NSCA, check_mk, the web server, the HTTP clients and the checksum
checks carried its own. `plugin_api.dll` also grew, having taken over the
settings helpers and the program-options helpers that each plugin used to
carry its own copy of. The legacy Windows XP build is not affected: it still
links everything statically.

Nothing to do when installing from the MSI or from the Debian/RedHat packages —
they install the new libraries for you, and the modules themselves are
unchanged from the outside. Only a **hand-rolled deployment** needs to check:
if you copy `modules\*.dll` into place yourself rather than installing a
package, copy the new libraries (the OpenSSL DLLs included) from the
installation root as well, or the modules that need them will fail to load.
