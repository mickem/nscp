import type { ModulePage } from "../types.ts";
import { ASIO_TLS_VERSION_OPTIONS } from "../shared/options.ts";
import NrpeServerWizard from "./NrpeServerWizard.tsx";

const NRPEServer: ModulePage = {
  id: "NRPEServer",
  Wizard: NrpeServerWizard,
  sections: {
    "/settings/NRPE/server": {
      "tls version": { kind: "select", options: ASIO_TLS_VERSION_OPTIONS },
    },
  },
};

export default NRPEServer;
