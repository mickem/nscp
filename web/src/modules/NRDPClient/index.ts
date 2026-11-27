// NRDP targets: the Nagios NRDP endpoints results are posted to over HTTP(S).
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { connectionFields, httpTlsFields, httpTlsRisks, scheme } from "../kit/targetFields.ts";
import type { TargetSpec, TargetValues } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";
import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";

const https = (v: TargetValues) => scheme(v("address")) === "https";

export const NRDP_CLIENT_TARGETS: TargetSpec = {
  moduleId: "NRDPClient",
  path: "/settings/NRDP/client/targets",
  noun: "NRDP server",
  fields: {
    ...connectionFields("80 for http, 443 for https"),
    token: {
      label: "Token",
      kind: "secret",
      builtIn: "",
      help: "The NRDP server's token. key and password are other spellings of it.",
    },
    key: { label: "Token (key)", kind: "secret", builtIn: "" },
    password: { label: "Token (password)", kind: "secret", builtIn: "" },
    ...httpTlsFields({ verify: "peer", ca: "${ca-path}", tls: "1.3" }),
    proxy: {
      label: "Proxy",
      kind: "text",
      builtIn: "",
      help: "http://user:pass@proxy:3128/",
    },
    "no proxy": {
      label: "Bypass the proxy for",
      kind: "text",
      builtIn: "",
      help: "Comma-separated host names.",
    },
  },
  sections: [
    { title: "Connection", keys: ["address", "timeout", "retries", "proxy", "no proxy"] },
    { title: "Security", keys: ["token", "verify mode", "ca", "tls version"] },
  ],
  // The same token under its two older names: the page writes `token`.
  expertOnly: ["key", "password"],
  risks: (v) => {
    const token = v("token") || v("key") || v("password");
    const out = httpTlsRisks(v, https(v), token ? "The token and every result" : "Every result");
    if (!token)
      out.push({
        severity: "warning",
        title: "No token",
        reason: "The NRDP server refuses results without its token.",
      });
    return out;
  },
  security: (v) => (https(v) ? `HTTPS, verify ${v("verify mode") || "none"}` : "HTTP"),
};

const NRDPClient: ModulePage = {
  id: "NRDPClient",
  purpose: {
    tools: ["Nagios XI", "Nagios"],
    transport: "NRDP",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  sections: { "/settings/NRDP/client": HOSTNAME_WIDGET },
  collections: { [NRDP_CLIENT_TARGETS.path]: targetsCollection(NRDP_CLIENT_TARGETS) },
};

export default NRDPClient;
