---
icon: "📦"
modules: [packaging]
action: none
---
**A Raspberry Pi OS package.** Releases now carry
`NSCP-<version>-debian-trixie-arm64.deb`, built on Debian 13 (Trixie) — the
base of current Raspberry Pi OS 64-bit — for Raspberry Pi 3 and newer, and for
Debian 13 arm64 in general. Nothing to do on an existing install; the Ubuntu
packages are unchanged and keep working where they already do.

There is no 32-bit package. Raspberry Pi OS has defaulted to 64-bit since
Bookworm, and a 32-bit build would have to be either a slow emulated one or a
Debian `armhf` package that still leaves the ARMv6 boards (Pi 1, Pi Zero W)
out. On a 32-bit Raspberry Pi OS install, either reimage to 64-bit or build
from source.

The Debian package is built without the .NET SDK, which Debian does not
package, so it ships without the managed (C#) plugin API. The Ubuntu packages
still include it.
