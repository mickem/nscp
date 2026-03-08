# AI Agent Instructions — NSClient++ Workspace

This document provides a comprehensive guide for AI agents to navigate and work within the NSClient++ (NSCP) workspace. It covers the project structure, where to find tests, source code, configuration, build files, and other important locations.

---

## 1. Workspace Overview

The workspace root is `C:\src` and contains two top-level areas:

| Path | Description |
|------|-------------|
| `C:\src\nscp\` | **Main project** — NSClient++ monitoring agent (C++, CMake) |
| `C:\src\build\` | **Third-party dependencies** (source trees used at build time) |

### Third-Party Dependencies (`C:\src\build\`)

| Path | What |
|------|------|
| `C:\src\build\lua-5.4.7\src\` | Lua 5.4.7 interpreter source |
| `C:\src\build\mongoose-7.19\` | Mongoose embedded HTTP library |
| `C:\src\build\tinyxml2-10.1.0\` | TinyXML2 XML parser |

Additional dependencies configured in `C:\src\nscp\build.cmake`:
- **Boost 1.86.0** — `C:\src\build\boost_1_86_0\`
- **Protobuf 21.12** — `C:\src\build\protobuf-21.12\`
- **OpenSSL 3.5.4** — `C:\src\build\openssl-3.5.4\`
- **Crypto++ 8.9.0** — `C:\src\build\CRYPTOPP_8_9_0\`
- **Google Test** — fetched via CMake `FetchContent` (release-1.12.1)

---

## 2. Main Project Structure (`C:\src\nscp\`)

```
nscp/
├── CMakeLists.txt            # Root CMake build file
├── build.cmake               # Local build configuration (paths, flags)
├── build/
│   ├── cmake/                # CMake helper modules & functions
│   │   ├── functions.cmake   # NSCP_CREATE_TEST, LOAD_SECTIONS, etc.
│   │   ├── dependencies.cmake
│   │   └── ...
│   ├── powershell/           # PowerShell build helpers
│   └── python/               # Python build scripts
├── include/                  # Shared headers & header-only libraries
├── libs/                     # Internal static/shared libraries
├── modules/                  # Plugin modules (each is a shared library)
├── clients/                  # Client implementations (nrpe, nscp)
├── service/                  # Core service / daemon executable
├── tests/                    # Integration & unit test registration (CMakeLists.txt)
├── scripts/                  # Runtime scripts (Lua, Python, Batch, PowerShell)
├── web/                      # Web UI (Vite + TypeScript frontend)
├── docs/                     # Documentation (MkDocs-based)
├── installers/               # Installer projects (MSI, etc.)
├── ext/                      # External vendored code (miniz)
├── rust/                     # Rust components
├── files/                    # Package files (deb/rpm scripts)
├── resources/                # Icons, resources
├── op5/                      # Op5-specific config & scripts
├── vagrant/                  # Vagrant VM definitions
└── .github/workflows/        # CI/CD pipelines
```

---

## 3. Finding Tests

### 3.1 Test Naming Convention

All C++ unit test files follow the pattern **`*_test.cpp`**. They use **Google Test (gtest)** framework.

### 3.2 Test Registration

Tests are registered with CMake in **`C:\src\nscp\tests\CMakeLists.txt`** using the `NSCP_CREATE_TEST()` function (defined in `build/cmake/functions.cmake`). This function:
1. Creates an executable from specified sources
2. Links specified libraries
3. Registers it with CTest via `add_test()`

### 3.3 Complete Test File Inventory

#### Header/Include Library Tests (`include/`)

| Test Target | Test File(s) | Component |
|-------------|-------------|-----------|
| `http_client_test` | `include/http/client_test.cpp` | HTTP client (requires OpenSSL) |
| `str_test` | `include/str/format_test.cpp` | String formatting |
| | `include/str/xtos_test.cpp` | String-to-number conversion |
| | `include/str/nscp_string_test.cpp` | NSCP string utilities |
| | `include/str/utils_test.cpp` | String utilities |
| | `include/str/wstring_test.cpp` | Wide string utilities |
| `logger_test` | `include/nsclient/logger/log_level_test.cpp` | Log level |
| | `include/nsclient/logger/log_message_factory_test.cpp` | Log message factory |
| | `include/nsclient/logger/logger_helper_test.cpp` | Logger helper |
| | `include/nsclient/logger/log_driver_interface_impl_test.cpp` | Log driver interface |
| | `include/nsclient/logger/logger_impl_test.cpp` | Logger implementation |
| `nscapi_protobuf_test` | `include/nscapi/protobuf/functions_convert_test.cpp` | Protobuf convert functions |
| | `include/nscapi/protobuf/functions_copy_test.cpp` | Protobuf copy functions |
| | `include/nscapi/protobuf/functions_exec_test.cpp` | Protobuf exec functions |
| | `include/nscapi/protobuf/functions_perfdata_test.cpp` | Protobuf perfdata functions |
| | `include/nscapi/protobuf/functions_query_test.cpp` | Protobuf query functions |
| | `include/nscapi/protobuf/functions_response_test.cpp` | Protobuf response functions |
| | `include/nscapi/protobuf/functions_status_test.cpp` | Protobuf status functions |
| | `include/nscapi/protobuf/functions_submit_test.cpp` | Protobuf submit functions |
| | `include/nscapi/protobuf/settings_functions_test.cpp` | Protobuf settings functions |
| `settings_helper_test` | `include/nscapi/settings/helper_test.cpp` | Settings helper |
| `settings_object_test` | `include/nscapi/settings/object_test.cpp` | Settings object |
| `settings_filter_test` | `include/nscapi/settings/filter_test.cpp` | Settings filter |
| `settings_proxy_test` | `include/nscapi/settings/proxy_test.cpp` | Settings proxy |
| `shellapi_test` | `include/win/shellapi_test.cpp` | Windows Shell API (Win32 only) |
| `wmi_query_test` | `include/wmi/wmi_query_test.cpp` | WMI queries (Win32 only) |
| — | `include/socket/allowed_hosts_test.cpp` | Socket allowed hosts |
| — | `include/nscapi/nscapi_protobuf_functions_test.cpp` | NSCAPI protobuf functions |

#### Service Tests (`service/`)

| Test File | Component |
|-----------|-----------|
| `service/cron_test.cpp` | Cron scheduling |
| `service/path_manager_test.cpp` | Path management |
| `service/plugins/plugin_manager_test.cpp` | Plugin manager |
| `service/plugins/plugin_list_test.cpp` | Plugin list |
| `service/plugins/plugin_interface_test.cpp` | Plugin interface |
| `service/plugins/plugin_cache_test.cpp` | Plugin cache |
| `service/plugins/master_plugin_list_test.cpp` | Master plugin list |
| `service/plugins/dll_plugin_test.cpp` | DLL plugin loading |

#### Library Tests (`libs/`)

| Test File | Component |
|-----------|-----------|
| `libs/plugin_api/nscapi_helper_test.cpp` | NSCAPI helper |
| `libs/perfconfig_parser/perfconfig_test.cpp` | Performance config parser |
| `libs/expression_parser/expression_test_old.cpp` | Expression parser |

#### Module Tests (`modules/`)

| Test File | Component |
|-----------|-----------|
| `modules/WEBServer/session_manager_interface_test.cpp` | Web session manager |
| `modules/WEBServer/grant_store_test.cpp` | Web grant store |
| `modules/WEBServer/token_store_test.cpp` | Web token store |

#### Integration / Acceptance Tests (`tests/`)

| Path | Component |
|------|-----------|
| `tests/socket/allowed_hosts_test.cpp` | Socket allowed hosts (integration) |
| `tests/acceptance-tests.bat` | Windows acceptance tests |
| `tests/acceptance-tests.sh` | Linux acceptance tests |
| `tests/nrpe/` | NRPE protocol tests |
| `tests/nsca/` | NSCA protocol tests |
| `tests/rest/` | REST API tests |
| `tests/msi/` | MSI installer tests |

### 3.4 How to Search for Tests

```
# Find all test files
**/*_test.cpp

# Find test CMake registration
tests/CMakeLists.txt

# Pattern: test files live alongside the source they test
#   source: include/str/format.hpp
#   test:   include/str/format_test.cpp
```

---

## 4. Source Code Locations

### 4.1 Headers & Header-Only Libraries (`include/`)

| Path | Purpose |
|------|---------|
| `include/http/` | HTTP client (`client.hpp`, `client_test.cpp`) |
| `include/socket/` | Socket abstractions, allowed hosts |
| `include/str/` | String utilities (format, xtos, wstring, etc.) |
| `include/nscapi/` | NSCAPI interface (helpers, protobuf, settings, targets) |
| `include/nscapi/protobuf/` | Protobuf function wrappers (convert, copy, exec, query, etc.) |
| `include/nscapi/settings/` | Settings framework (helper, filter, proxy, object) |
| `include/nsclient/logger/` | Logger framework (levels, drivers, helpers, factories) |
| `include/client/` | Client command-line parser |
| `include/parsers/` | Parsers (cron, expression, filter, where-filter, perfconfig) |
| `include/lua/` | Lua scripting integration headers |
| `include/json/` | JSON support (`use_json.cpp`) |
| `include/net/` | Networking utilities |
| `include/metrics/` | Metrics collection |
| `include/scheduler/` | Task scheduler |
| `include/settings/` | Settings manager interfaces |
| `include/error/` | Error handling (platform-specific) |
| `include/win/` | Windows-specific APIs (shellapi, services, processes, WMI) |
| `include/wmi/` | WMI query wrappers |
| `include/nrpe/` | NRPE protocol |
| `include/nsca/` | NSCA protocol |
| `include/check_mk/` | Check_MK protocol |
| `include/check_nt/` | Check_NT protocol |
| `include/collectd/` | Collectd protocol |
| `include/b64/` | Base64 encoding |
| `include/zip/` | ZIP file handling |

### 4.2 Internal Libraries (`libs/`)

| Path | Purpose |
|------|---------|
| `libs/plugin_api/` | Plugin API library |
| `libs/protobuf/` | Protobuf definitions & generated code |
| `libs/settings_manager/` | Settings manager implementation |
| `libs/where_filter/` | Where-filter engine |
| `libs/expression_parser/` | Expression parser library |
| `libs/perfconfig_parser/` | Performance config parser |
| `libs/mongoose-cpp/` | C++ wrapper around Mongoose HTTP server |
| `libs/lua/` | Lua integration library |
| `libs/lua_nscp/` | Lua NSCP bindings |
| `libs/nscpcrypt/` | NSCP cryptography library |
| `libs/minizip/` | Minizip compression library |
| `libs/win_sysinfo/` | Windows system info library |
| `libs/dotnet-plugin-api/` | .NET plugin API |
| `libs/protobuf_net/` | Protobuf .NET bindings |

### 4.3 Plugin Modules (`modules/`)

Each subdirectory is a plugin module built as a shared library:

| Module | Purpose |
|--------|---------|
| `CheckDisk` | Disk space checks |
| `CheckEventLog` | Windows event log checks |
| `CheckExternalScripts` | External script execution |
| `CheckHelpers` | Helper check commands |
| `CheckLogFile` | Log file monitoring |
| `CheckNet` | Network checks |
| `CheckNSCP` | NSCP self-checks |
| `CheckSystem` / `CheckSystemUnix` | System checks (CPU, memory, etc.) |
| `CheckTaskSched` | Task scheduler checks |
| `CheckWMI` | WMI-based checks |
| `CheckPowershell` | PowerShell-based checks |
| `CheckDocker` | Docker monitoring |
| `CheckMKClient` / `CheckMKServer` | Check_MK protocol |
| `NRPEClient` / `NRPEServer` | NRPE protocol |
| `NSCAClient` / `NSCAServer` | NSCA protocol |
| `NSClientServer` | NSClient protocol (legacy) |
| `NSCPClient` | NSCP protocol client |
| `WEBServer` | Built-in web server & REST API |
| `Scheduler` | Scheduled task execution |
| `SimpleCache` | Simple data caching |
| `SimpleFileWriter` | File output |
| `GraphiteClient` | Graphite metrics |
| `ElasticClient` | Elasticsearch integration |
| `CollectdClient` | Collectd integration |
| `NRDPClient` | NRDP protocol |
| `SMTPClient` | SMTP email notifications |
| `SyslogClient` | Syslog forwarding |
| `Op5Client` | Op5 integration |
| `LUAScript` | Lua scripting engine |
| `PythonScript` | Python scripting engine |
| `DotnetPlugins` | .NET plugin host |
| `CommandClient` | Command client |
| `CauseCrashes` | Crash test module (development) |
| `SamplePluginSimple` | Sample plugin template |

### 4.4 Service / Core Daemon (`service/`)

| File | Purpose |
|------|---------|
| `service/NSClient++.cpp` | Main entry point |
| `service/core_api.cpp` | Core API implementation |
| `service/cli_parser.cpp` | Command-line parsing |
| `service/path_manager.cpp` | Path resolution |
| `service/settings_client.cpp` | Settings client |
| `service/storage_manager.cpp` | Storage management |
| `service/scheduler_handler.cpp` | Scheduler handler |
| `service/plugins/` | Plugin loading & management subsystem |
| `service/logger/` | Logger implementations (console, file, threaded) |

---

## 5. Build System

### 5.1 Key Build Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Root build script |
| `build.cmake` | **Local machine configuration** (dependency paths, flags) |
| `build/cmake/functions.cmake` | Custom CMake functions (`NSCP_CREATE_TEST`, `LOAD_SECTIONS`, etc.) |
| `build/cmake/dependencies.cmake` | Dependency detection |
| `build/cmake/module.cmake` | Module build helpers |
| `check_deps.cmake` | Dependency validation |

### 5.2 Key CMake Variables

| Variable | Meaning |
|----------|---------|
| `NSCP_INCLUDEDIR` | Points to `${CMAKE_SOURCE_DIR}/include` |
| `BUILD_ROOT` | Third-party dependency root (`C:/src/build`) |
| `MODULE_SUBFOLDER` | `"modules"` — where plugin DLLs go |
| `BUILD_TARGET_EXE_PATH` | Output directory for executables |
| `BUILD_TARGET_LIB_PATH` | Output directory for plugin modules |
| `USE_STATIC_RUNTIME` | Whether to use static CRT linking |

### 5.3 Build Configuration

The build output directory is: `C:\src\nscp\cmake-build-relwithdebinfo-visual-studio\`

### 5.4 How to Build & Run Tests

```powershell
# Configure (from project root)
cmake -B cmake-build-relwithdebinfo-visual-studio -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build all
cmake --build cmake-build-relwithdebinfo-visual-studio --config RelWithDebInfo

# Run all tests
ctest --test-dir cmake-build-relwithdebinfo-visual-studio --build-config RelWithDebInfo

# Run a specific test
ctest --test-dir cmake-build-relwithdebinfo-visual-studio --build-config RelWithDebInfo -R str_test
```

---

## 6. CI/CD Pipelines (`.github/workflows/`)

| Workflow | Purpose |
|----------|---------|
| `build-windows.yml` | Windows build |
| `build-windows-legacy.yml` | Windows legacy/XP build |
| `build-debian.yml` | Debian package build |
| `build-redhat.yml` | Red Hat / RPM build |
| `build-rust.yml` | Rust component build |
| `build-feature.yml` | Feature branch build |
| `build-main.yml` | Main branch build |
| `integration-tests-linux.yml` | Linux integration tests |
| `integration-tests-windows.yml` | Windows integration tests |
| `create-test-bundle.yml` | Test bundle creation |
| `release.yml` | Release pipeline |
| `get-version.yml` | Version extraction |

---

## 7. Web Frontend (`web/`)

A **Vite + TypeScript** web application:

| File | Purpose |
|------|---------|
| `web/package.json` | Node.js dependencies |
| `web/vite.config.ts` | Vite configuration |
| `web/tsconfig.json` | TypeScript configuration |
| `web/src/` | Application source code |
| `web/dist/` | Built output |
| `web/CMakeLists.txt` | CMake integration for web build |

---

## 8. Documentation (`docs/`)

MkDocs-based documentation:

| Path | Purpose |
|------|---------|
| `docs/mkdocs.yml` | MkDocs configuration |
| `docs/docs/` | Documentation source (Markdown) |
| `docs/source/` | Additional documentation source |
| `docs/samples/` | Sample configurations |

---

## 9. Scripts (`scripts/`)

| Path | Purpose |
|------|---------|
| `scripts/lua/` | Lua scripts |
| `scripts/python/` | Python scripts |
| `scripts/modules/` | Zip-module scripts (packaged as modules) |
| `scripts/check_*.bat/.sh/.vbs/.ps1` | Check command scripts |

---

## 10. Key Patterns for Agents

### Finding the source for a header
- Header: `include/<component>/<name>.hpp`
- Implementation (if not header-only): same directory `<name>.cpp` or in `libs/<component>/`

### Finding the test for a source file
- Source: `include/str/format.hpp`
- Test: `include/str/format_test.cpp` (same directory, `_test.cpp` suffix)

### Finding the CMake registration for a test
- All tests are registered in: `C:\src\nscp\tests\CMakeLists.txt`
- The `NSCP_CREATE_TEST()` macro is defined in: `C:\src\nscp\build\cmake\functions.cmake`

### Finding a module's build definition
- Modules use `module.cmake` files: `modules/<ModuleName>/module.cmake`
- Loaded by `LOAD_SECTIONS()` in the root `CMakeLists.txt`

### Finding where a module is configured
- Module-specific CMakeLists: `modules/<ModuleName>/CMakeLists.txt`
- Module entry points: `modules/<ModuleName>/<ModuleName>.cpp` and `.h`

### Creating a new test
1. Create `<component>_test.cpp` alongside the source file
2. Use `#include <gtest/gtest.h>` and write `TEST()` macros
3. Register in `tests/CMakeLists.txt` using `NSCP_CREATE_TEST()`

### File search patterns
```
# All test files
**/*_test.cpp

# All CMake files
**/CMakeLists.txt
**/module.cmake

# All headers
include/**/*.hpp
include/**/*.h

# All module source
modules/**/*.cpp

# All protobuf-related
**/protobuf/**/*

# All settings-related
**/settings/**/*
```

---

## 11. Important Configuration Files

| File | Purpose |
|------|---------|
| `build.cmake` | Local build paths & dependency locations |
| `.clang-format` | C++ code formatting rules |
| `.gersemirc` | CMake formatting rules |
| `.gitmodules` | Git submodule definitions |
| `.gitignore` | Git ignore patterns |
| `nscp.spec.in` | RPM spec template |
| `include/config.h.in` | Generated config header template |
| `include/version.hpp.in` | Generated version header template |

---

## 12. Summary Quick Reference

| What you want | Where to look |
|---------------|---------------|
| Unit tests | `**/*_test.cpp` (43 files total) |
| Test CMake registration | `tests/CMakeLists.txt` |
| Test framework helper | `build/cmake/functions.cmake` → `NSCP_CREATE_TEST()` |
| Main executable | `service/NSClient++.cpp` |
| Plugin modules | `modules/<Name>/` |
| Shared headers | `include/` |
| Internal libraries | `libs/` |
| Build configuration | `build.cmake`, `CMakeLists.txt` |
| CI/CD | `.github/workflows/` |
| Web UI | `web/` |
| Documentation | `docs/` |
| Scripts | `scripts/` |
| Third-party deps | `C:\src\build\` |

