# Third-party notices

NSClient++ is licensed under `Apache-2.0 OR GPL-2.0-only` (see [COPYING](COPYING)).
It also incorporates the third-party components listed below, which remain under
their own licenses.

## Bundled source

| Component                                         | Location                                                                         | License                    |
|---------------------------------------------------|----------------------------------------------------------------------------------|----------------------------|
| mongoose-cpp (Grégoire Passault)                  | `libs/mongoose-cpp/` (derived files)                                             | MIT                        |
| SimpleIni (Brodie Thiesfield)                     | `include/simpleini/simpleini.h`                                                  | MIT                        |
| asio ICMP/IPv4 headers (Christopher M. Kohlhoff)  | `include/net/icmp_header.hpp`, `include/net/ipv4_header.hpp`                     | Boost Software License 1.0 |
| WiX standard installer UI (Microsoft Corporation) | `installers/ui/WixUI_MondoNSCP.wxs`, `installers/installer-NSCP/WixUI_en-us.wxl` | Common Public License 1.0  |

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
| Roboto font (via @fontsource/roboto) | Apache-2.0 |
