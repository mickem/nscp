import type { ModulePage } from "../types.ts";
import { ASIO_TLS_VERSION_OPTIONS } from "../shared/options.ts";

const CheckNSCP: ModulePage = {
  id: "CheckNSCP",
  sections: {
    // The self-update HTTPS client.
    "/settings/nscp/check/update": {
      "tls version": { kind: "select", options: ASIO_TLS_VERSION_OPTIONS },
    },
  },
};

export default CheckNSCP;
