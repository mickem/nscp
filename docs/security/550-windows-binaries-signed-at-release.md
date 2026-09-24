---
title: "Windows releases: every shipped binary signed, and signing credentials only in the release build"
fixed_in: next
severity: "Low (build integrity)"
modules: [packaging]
action: none
---
Up to 0.23.0 the Windows release signed the MSI and the executables but not the
DLLs it installs: the modules, the NSClient++ libraries, and the bundled Boost,
OpenSSL, protobuf, Lua and MariaDB libraries. An unsigned library cannot be
told apart from a replaced one by its signature, cannot be covered by an
application-control publisher rule, and draws more attention from endpoint
protection than a signed one.

The signing credentials also reached more jobs than needed. They were handed
to the Windows build of every pull request from a branch of this repository
and of every merge to `main`, although only merges used them, and those jobs
build the code under review.

This release:

* signs every executable, DLL and Python extension the MSI installs, and the
  custom-action DLL embedded in the MSI that runs during installation. The list
  comes from the installer's own sources, so a new module is signed without a
  change to the build, and the unit-test executables built alongside are not.
  A third-party file that is already validly signed keeps its publisher's
  signature;
* builds and signs only the release. The release pull request, the one that
  files the upgrade notes under the new version, starts the signed build and
  the draft release. Pull requests and ordinary merges build unsigned, and
  their jobs are no longer given the signing credentials;
* pins the draft release to the commit that was built, so publishing it tags
  that commit rather than whatever `main` holds at the time.

**What to do:** nothing. To check a file, use `Get-AuthenticodeSignature` as
described in [Installing](../setup/installing.md#step-1-check-that-the-file-came-from-this-project).
