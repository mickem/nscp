// Target keys every client shares (nscapi's target object): where the target
// is, how long to wait, and whether a request may send it elsewhere.
import type { TargetField } from "./targets.ts";

export function connectionFields(port: string): Record<string, TargetField> {
  return {
    address: {
      label: "Address",
      kind: "text",
      builtIn: "",
      help: `Host name or IP, optionally with :port (port ${port} if left out).`,
    },
    host: {
      label: "Host",
      kind: "text",
      builtIn: "",
      help: "The older spelling of the address's host part; Address wins when both are set.",
    },
    port: {
      label: "Port",
      kind: "number",
      builtIn: port,
      help: "Used when the address names no port.",
    },
    timeout: { label: "Timeout (seconds)", kind: "number", builtIn: "30" },
    retries: {
      label: "Retries",
      kind: "number",
      builtIn: "3",
      help: "Further attempts after a failed connection.",
    },
    "allow host override": {
      label: "Let a request redirect this target's credentials",
      kind: "bool",
      builtIn: "false",
      help: "Off refuses a request that names another host, proxy or TLS setting while using this target's password or token. Add a separate target instead.",
    },
  };
}

// The keys the Connection section shows; host and port are under Advanced.
export const CONNECTION_KEYS = ["address", "timeout", "retries"];
