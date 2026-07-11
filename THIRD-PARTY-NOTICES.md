# Third-party notices

NSClient++ is licensed under `Apache-2.0 OR GPL-2.0-only` (see [COPYING](COPYING)).
It also incorporates the third-party components listed below, which remain under
their own licenses.

The `GPL-2.0-only` half of the dual license exists specifically for the legacy
web backend: builds configured with `NSCP_WEB_BACKEND=mongoose` link the Cesanta
mongoose server (GPL-2.0), which is **incompatible** with Apache-2.0. Offering
NSClient++'s own code under GPL-2.0-only as well makes that combined binary
distributable. Builds using the Boost.Beast backend (the default on Linux, and
soon on modern Windows) do not link mongoose and can be used under Apache-2.0.

**Effective license of a distributed binary:** the source is `Apache-2.0 OR
GPL-2.0-only` (recipient's choice), but a *legacy binary that links mongoose* is
effectively **GPL-2.0-only** — the Apache option cannot be exercised on a work
containing GPL-2.0 code. Beast-backend binaries carry the full dual choice.

## Binary dependencies

Fetched and built at packaging time (not vendored in this repository) and linked
into the shipped binary. Their license notices are reproduced because we
distribute them. Pinned versions live in `.github/workflows/build-windows.yml`.

| Component                              | Notes                                   | License       |
|----------------------------------------|-----------------------------------------|---------------|
| OpenSSL                                | TLS/crypto; Windows binary (3.x)        | Apache-2.0    |
| Crypto++ (Wei Dai et al.)              | crypto; Windows binary                  | BSL-1.0       |
| Protocol Buffers (Google)              | serialization; Windows binary           | BSD-3-Clause  |
| Boost                                  | Windows binary (statically linked)      | BSL-1.0       |
| Lua (PUC-Rio)                          | scripting engine; Windows binary        | MIT           |
| TinyXML-2 (Lee Thomason)               | XML parsing; Windows binary             | Zlib          |
| miniz (Rich Geldreich et al.)          | zip/deflate; Windows binary             | MIT           |
| Mongoose web server (Cesanta Software) | legacy `NSCP_WEB_BACKEND=mongoose`      | GPL-2.0-only  |

On Linux, some of these (OpenSSL, Boost, Protobuf, Lua) are typically resolved as
system/dynamic libraries and are not redistributed in the package; the notice
obligation applies wherever we actually ship the library.

## Bundled source

| Component                                         | Location                                                                         | License                    |
|---------------------------------------------------|----------------------------------------------------------------------------------|----------------------------|
| mongoose-cpp (Grégoire Passault)                  | `libs/mongoose-cpp/` (derived files)                                             | MIT                        |
| SimpleIni (Brodie Thiesfield)                     | `include/simpleini/simpleini.h`                                                  | MIT                        |
| asio ICMP/IPv4 headers (Christopher M. Kohlhoff)  | `include/net/icmp_header.hpp`, `include/net/ipv4_header.hpp`                     | Boost Software License 1.0 |
| WiX standard installer UI (Microsoft Corporation) | `installers/ui/WixUI_MondoNSCP.wxs`, `installers/installer-NSCP/WixUI_en-us.wxl` | Common Public License 1.0  |
| WiX custom action library, prebuilt (Microsoft Corporation) | `installers/installer-NSCP/wixca.dll`                                            | Common Public License 1.0  |
| C#/.NET/Mono CMake modules, from GDCM (Mathieu Malaterre)   | `build/cmake/{Find,Use}{CSharp,Mono,DotNetFrameworkSdk}.cmake`                   | BSD-3-Clause               |
| Protobuf CMake modules (Ange Optimization ApS)              | `build/cmake/FindProtoBuf.cmake`, `build/cmake/GoogleProtoBuf.cmake`             | BSD-3-Clause               |
| Google.Protobuf .NET runtime, prebuilt (Google LLC)         | `libs/protobuf_net/Google.Protobuf.dll`                                          | BSD-3-Clause               |

## Web UI

The management web UI (`web/`) is built from npm dependencies that are fetched at
build time and bundled into the shipped assets. Their attributions are listed in
the in-app **About** page and summarized here:

| Component                            | License    |
|--------------------------------------|------------|
| React, React DOM, React Router       | MIT        |
| MUI (Material UI, Icons, X Charts)   | MIT        |
| Emotion                              | MIT        |
| Redux Toolkit, React Redux           | MIT        |
| Zod                                  | MIT        |
| Roboto font (via @fontsource/roboto) | OFL-1.1    |
