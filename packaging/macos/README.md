# macOS packaging

The macOS artifacts are built by
[`.github/workflows/build-macos.yml`](../../.github/workflows/build-macos.yml)
on an Apple silicon runner. Unlike Windows (WiX/MSI) and Linux (CPack DEB/RPM),
the installer is **not** produced by the build system: CPack only emits the
relocatable tarball, and the `.pkg` is assembled from a staged install tree by
Apple's own `pkgbuild` and `productbuild`.

That split is not incidental. A step has to run between `make install` and the
package being sealed — see *Why the bundling pass exists* below — and CPack has
no hook there.

```
make install DESTDIR=stage            absolute install paths (NSCP_*) land under stage/
        │
        ├─► bundle_dylibs.sh          copy in the Homebrew dylibs, rewrite install
        │                             names to @rpath, re-sign
        │
        ├─► pkgbuild                  stage/ + files/macos/scripts/ -> component .pkg
        │
        └─► productbuild              + distribution.xml -> the shipped product archive
```

## Where each piece lives

| Piece | Source | Generated to |
|-------|--------|--------------|
| launchd job | [`files/macos/com.nsclient.nscp.plist.in`](../../files/macos/com.nsclient.nscp.plist.in) | installed into the payload at `/Library/LaunchDaemons` |
| preinstall / postinstall | `files/macos/{pre,post}install.sh.in` | `<build>/files/macos/scripts/` (what `pkgbuild --scripts` takes) |
| uninstaller | [`files/macos/uninstall.sh.in`](../../files/macos/uninstall.sh.in) | installed as `<sbindir>/uninstall-nsclient` |
| product archive | [`files/macos/distribution.xml.in`](../../files/macos/distribution.xml.in) | `<build>/files/macos/distribution.xml` |
| shared values | [`files/macos/pkg.env.in`](../../files/macos/pkg.env.in) | `<build>/files/macos/pkg.env`, sourced by the workflow |

Everything is templated by CMake from the same `NSCP_*` variables the
`install()` rules use, so a build configured with a different
`CMAKE_INSTALL_PREFIX` produces a consistent package rather than one with `/usr/local`
baked into half of it. `pkg.env` exists so the workflow does not have to repeat
the package identifier and the layout in YAML, where the two copies would drift
the first time either changed.

## Why the bundling pass exists

The build links against Homebrew's Boost, OpenSSL, protobuf and Lua, and the
linker records those by absolute path
(`/opt/homebrew/opt/boost/lib/libboost_thread.dylib`). On the build machine that
is fine. In a package it is not: on a Mac without Homebrew the daemon dies at
load time, and on a Mac *with* Homebrew it silently runs against whatever
version that machine happens to have.

[`bundle_dylibs.sh`](bundle_dylibs.sh) copies every non-system dylib into
`lib/nsclient` next to our own private libraries and rewrites the references to
`@rpath`, which the RPATHs CMake already emits resolve. It then **re-signs**
every image it touched: on arm64 a Mach-O without a valid signature cannot be
executed or loaded at all, and `install_name_tool` invalidates the one the
linker produced. An ad-hoc signature satisfies the loader.

The script ends by asserting that no absolute non-system reference is left, so a
library the pass missed fails the build rather than a customer's install.

## Not done yet

* **Signing and notarization.** The packages are ad-hoc signed, which is enough
  for the loader but not for Gatekeeper: a double-click install is blocked and
  users have to go through `sudo installer` or right-click → *Open*. Wiring a
  Developer ID Installer certificate and `notarytool` in needs an Apple
  Developer account and four repository secrets; the workflow is structured so
  that it can be added as a step between `productbuild` and the upload, behind
  an `if:` on the secrets being present.
* **Intel / universal builds.** arm64 only. A universal binary would mean
  building against universal dependencies, which Homebrew does not ship.
