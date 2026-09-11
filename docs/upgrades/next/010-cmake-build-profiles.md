---
icon: "🔧"
modules: [packaging]
action: conditional
---
**Building from source: CMake 3.21 or newer, and `USE_STATIC_RUNTIME` is
replaced by a legacy-build profile and independent knobs.** Nothing changes
for the shipped packages and installers. If you build NSClient++ yourself:

* `cmake_minimum_required` is now 3.21 (Ubuntu 22.04, Debian 12, RHEL 9 and
  the Windows runners all ship it). With it, a value set in `build.cmake`
  wins over the defaults in the CMake files instead of being silently reset.
* `USE_STATIC_RUNTIME` is deprecated. Despite its name it selected the whole
  legacy Windows XP build; that is now `-DNSCP_LEGACY_BUILD=ON` (or the
  `windows-x86-legacy` preset). The pieces it bundled are separate knobs that
  combine freely: `NSCP_STATIC_RUNTIME` (MSVC `/MT`), `NSCP_STATIC_LIBS` (the
  project's own support libraries linked in instead of shipped as
  `nscp_*.dll` / `libnscp_*.so`), `NSCP_TARGET_WINDOWS_XP` (API level) and
  `NSCP_PACKAGE_SUFFIX`. `-DUSE_STATIC_RUNTIME=OFF` was the default and can
  simply be dropped; `ON` still produces the legacy build, with a deprecation
  notice.
* `CMakePresets.json` carries the generator, toolset and profile for each
  shipped flavour (`cmake --preset windows-x64`, `windows-x86-legacy`,
  `linux`, ...); put machine-specific dependency locations in an untracked
  `CMakeUserPresets.json` or keep using `build.cmake`.
* Reconfigure from a clean build directory once: the MSVC runtime is now
  selected through `CMAKE_MSVC_RUNTIME_LIBRARY`, and an old cache still holds
  the `/MT` or `/MD` flags the previous configure forced into
  `CMAKE_CXX_FLAGS_<CONFIG>`.
