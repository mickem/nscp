---
icon: "🔧"
modules: [packaging]
action: none
---
**The web bundle is built with `npm ci` and gated on `npm audit`.** `npm install`
tolerates a lockfile that has drifted from `package.json` and rewrites it, so
the bytes in the web zip and in the MSI's bundled `web/dist` were not guaranteed
to match `package-lock.json` — the `.sha256` manifest the installer verifies
proved integrity in transit, not provenance of the dependency tree. `npm ci`
fails on drift instead, and a new step runs
`npm audit --omit=dev --audit-level=high` so a known-vulnerable runtime
dependency stops the release build. Nothing to do; contributors whose
`package-lock.json` is out of date will see the build fail rather than see it
quietly rewritten.
