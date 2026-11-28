// What the Kubernetes page knows about CheckKubernetes' settings
// (modules/CheckKubernetes/kube_settings.hpp).
import { fixed, KeyTable } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const KUBE_MODULE = "CheckKubernetes";
export const KUBE_PATH = "/settings/kubernetes";

export const KUBE_TABLE: KeyTable = {
  [KUBE_PATH]: {
    "api server": { builtIn: fixed("") },
    token: { builtIn: fixed("") },
    "token file": { builtIn: fixed("") },
    kubeconfig: { builtIn: fixed("") },
    context: { builtIn: fixed("") },
    ca: { builtIn: fixed("${ca-path}") },
    "verify mode": { builtIn: fixed("peer") },
    "tls version": { builtIn: fixed("tlsv1.2+") },
    timeout: { builtIn: fixed("30") },
    "max response size": { builtIn: fixed("64") },
  },
};

// How the agent finds the cluster: the API server set wins, then a
// kubeconfig, then the in-cluster service account.
export type KubeSource = "server" | "kubeconfig" | "incluster";

export function sourceOf(get: (k: string) => string): KubeSource {
  if (get("api server").trim()) return "server";
  if (get("kubeconfig").trim()) return "kubeconfig";
  return "incluster";
}

export function kubeProblem(get: (k: string) => string, source: KubeSource): string | null {
  if (source === "server" && !get("token").trim() && !get("token file").trim())
    return "With an API server set, the agent needs a token or a token file.";
  const size = Number(get("max response size"));
  if (!Number.isFinite(size) || size < 0)
    return "The largest response is a number of MB, 0 for no limit.";
  if (!(Number(get("timeout")) > 0)) return "The timeout has to be above 0.";
  return null;
}

export function kubeRisks(get: (k: string) => string, source: KubeSource): Risk[] {
  const out: Risk[] = [];
  if (source === "server" && /^http:\/\//i.test(get("api server").trim()))
    out.push({
      severity: "error",
      title: "The token is sent in clear",
      reason: "The API server is http://: anyone on the path reads the bearer token. Use https://.",
    });
  const verify = get("verify mode")
    .split(",")
    .map((t) => t.trim());
  if (!verify.some((t) => ["peer", "certificate", "peer-cert"].includes(t)))
    out.push({
      severity: "error",
      title: "The API server is not verified",
      reason:
        "Any certificate is accepted, and the bearer token is still sent: anyone who can intercept the connection gets it.",
    });
  if (/^(tlsv1\.[01]|tls1\.[01]|1\.[01]|any|sslv3)/.test(get("tls version").trim()))
    out.push({
      severity: "warning",
      title: "An outdated TLS version is allowed",
      reason: `${get("tls version")} lets the connection fall back below TLS 1.2.`,
    });
  if (source !== "kubeconfig" && get("token").trim() && !get("token file").trim())
    out.push({
      severity: "warning",
      title: "The token is stored in the settings",
      reason:
        "Prefer a token file the agent's account alone can read; the settings may be copied to other places.",
    });
  return out;
}

export const KUBE_TLS_VERSIONS = ["tlsv1.3", "tlsv1.2+", "tlsv1.2", "tlsv1.1+", "tlsv1.0+", "any"];
