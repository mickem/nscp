---
icon: "🔌"
modules: [NRPEServer]
---
**NRPEServer** now survives a failed listener (logs an ERROR, leaves the
module loaded) instead of failing the whole module. Add "NRPE listener failed"
as a signal if you alerted on module-load failure.
