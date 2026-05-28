# Missing-dependency build validation

This directory holds a matrix of Docker images that build NSClient++ on Linux
with one (or all) of its *optional* third-party libraries deliberately absent.
They exist to validate that the build degrades gracefully instead of
hard-failing when a library is not installed.

The libraries under test:

| Library        | Role in NSCP                              | Absent ⇒                                    |
|----------------|-------------------------------------------|---------------------------------------------|
| Google Test    | unit tests only                           | tests not built                             |
| OpenSSL        | TLS for NRPE/NSCA/check_mk/native/HTTPS   | TLS features + SMTP/NSCA-NG modules dropped |
| Crypto++       | NSCA payload encryption                   | encryption disabled                         |
| miniz          | ZIP backend (Windows, vendored)           | — (Windows only)                            |
| libzip         | ZIP backend (Linux/macOS, system package) | ZIP archive reading disabled                |
| Lua            | LUAScript + check_mk scripting            | LUAScript / CheckMK* modules dropped        |
| TinyXML2       | NRDP XML payloads                         | NRDPClient module dropped                   |
| Python (embed) | PythonScript (CPython via Boost.Python)   | PythonScript module dropped                 |

> **About miniz vs libzip.** miniz is the vendored backend used *only on
> Windows*; Linux/macOS use the system libzip. On Linux there is therefore no
> such thing as "build without miniz" — it is never used. Omitting libzip on
> Linux leaves the build with **no ZIP backend at all** (`NSCP_ZIP_BACKEND=none`),
> which is what `Dockerfile.no-zip` validates.

> **About Python.** Only the Python *development* libraries (libpython +
> Boost.Python, used to embed CPython in PythonScript) are optional. The Python
> *interpreter* is required by the build itself for protobuf / module code
> generation, so the base image always ships it; the no-python image keeps the
> interpreter and only drops the dev libraries.

## What made this possible

These builds only succeed because of the optional-dependency handling in the
build system (same branch as this directory):

- **Google Test** — `option(NSCP_BUILD_TESTS)` (default `ON`). When `OFF`, the
  googletest `FetchContent` download, `enable_testing()`, the `tests/`
  subdirectory and every `NSCP_CREATE_TEST()` call are skipped.
- **OpenSSL** — the `FATAL_ERROR` in `dependencies.cmake` is a status line. The
  shared HTTP client (`include/net/http/client.hpp`) and the check_mk server
  protocol header guard their TLS code under `USE_SSL`; modules that cannot work
  without TLS (`SMTPClient`, `NSCANgClient`) skip themselves. The Beast web
  backend still requires OpenSSL, so OpenSSL-less builds use
  `-DNSCP_WEB_BACKEND=mongoose`.
- **Crypto++** — already optional (`libs/nscpcrypt` compiles a no-encryption
  variant).
- **ZIP** — `libs/minizip` builds a stub `INTERFACE` target propagating
  `NSCP_NO_ZIP` when neither miniz nor libzip is present;
  `include/bytes/unzip.cpp` compiles a no-op reader; `unzip_test` is skipped.
- **Lua / TinyXML2** — already optional: the lua libraries guard their whole
  body on `LUA_FOUND`/`LUA_SOURCE_FOUND`, and the consuming modules
  (LUAScript, CheckMK*, NRDPClient) gate themselves in their `module.cmake`.
- **Python (embed)** — the Boost.Python component is requested via
  `OPTIONAL_COMPONENTS`, so a missing `libboost-python` no longer drags
  `Boost_FOUND` to false. `Python3 Development` is likewise optional, and
  `PythonScript` builds only when both libpython and Boost.Python are present.

## The images

`Dockerfile.base` installs the constant toolchain plus every NSCP build
dependency *except* the toggled libraries (Boost is installed as individual
component packages so Boost.Python can be left out). Each scenario image starts
`FROM nscp-build-base` and installs back only the toggled libraries it keeps —
whatever it omits is what gets validated.

| Dockerfile                | Omits                                        | Web backend | Tests |
|---------------------------|----------------------------------------------|-------------|-------|
| `Dockerfile.base`         | (toolchain + non-toggled deps only)          | —           | —     |
| `Dockerfile.no-gtest`     | Google Test                                  | beast       | OFF   |
| `Dockerfile.no-openssl`   | OpenSSL                                      | mongoose    | OFF   |
| `Dockerfile.no-cryptopp`  | Crypto++                                     | beast       | OFF   |
| `Dockerfile.no-zip`       | libzip (⇒ no ZIP backend)                    | beast       | OFF   |
| `Dockerfile.no-lua`       | Lua                                          | beast       | OFF   |
| `Dockerfile.no-tinyxml2`  | TinyXML2                                     | beast       | OFF   |
| `Dockerfile.no-python`    | libpython + Boost.Python                     | beast       | OFF   |
| `Dockerfile.minimal`      | all of the above at once                     | mongoose    | OFF   |

All scenarios build with `-DNSCP_BUILD_TESTS=OFF`: they validate that the
daemon and its modules *build* without the given library. The unit-test suite
(which needs Google Test/gmock) is exercised separately by the normal CI build,
so it is intentionally out of scope here.

## Running

From the **repository root** (the build context is the repo so the images
compile your current working tree, including uncommitted changes):

```bash
# Build the base image + every scenario and print a pass/fail summary:
build/docker/build-all.sh

# Or a subset:
build/docker/build-all.sh no-lua no-python

# Or one image by hand (base must exist first):
docker build -f build/docker/Dockerfile.base   -t nscp-build-base        build/docker
docker build -f build/docker/Dockerfile.no-lua -t nscp-validate-no-lua   .
```

A scenario **passes** when its `docker build` completes — i.e. NSClient++
compiled with that library absent. These are full from-scratch builds (Boost,
protobuf, the whole tree), so each image takes a while and downloads a lot on
first run. The `nscp-build-base` layer is cached and reused across scenarios.

## Notes / limitations

- **Base image:** `ubuntu:24.04` (provides Python 3.12, so
  `-DNSCP_BOOST_PYTHON_VERSION=python312` matches Boost.Python). Adjust if you
  rebase onto a different distro.
- The Rust `check_nsclient` copy is skipped (`-DCHECK_NSCLIENT_MISSING=TRUE`)
  so no Rust toolchain is needed — these images validate the C++ build, not the
  packaged installer.
- The base installs Boost via individual `libboost-*-dev` packages rather than
  `libboost-all-dev`, specifically so `libboost-python-dev` can be omitted for
  the no-python scenario (`libboost-all-dev` would drag it back in).
