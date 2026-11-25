import type { ModulePage } from "../types.ts";
import { ASIO_TLS_VERSION_OPTIONS } from "../shared/options.ts";
import NrpePage from "./NrpePage.tsx";
import { NRPE_PATH } from "./nrpe.ts";

const NRPEServer: ModulePage = {
  id: "NRPEServer",
  Panel: NrpePage,
  handledKeys: {
    [NRPE_PATH]: [
      "port",
      "allowed hosts",
      "use ssl",
      "insecure",
      "allowed ciphers",
      "certificate",
      "certificate key",
      "ca",
      "verify mode",
      "allow arguments",
      "allow nasty characters",
    ],
  },
  purpose: {
    tools: ["Nagios", "Icinga", "Naemon", "op5"],
    transport: "NRPE",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
  sections: {
    [NRPE_PATH]: {
      "tls version": { kind: "select", options: ASIO_TLS_VERSION_OPTIONS },
    },
  },
};

export default NRPEServer;
