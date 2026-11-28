// The Kubernetes page: how the agent reaches the cluster, and how it trusts
// the API server.
import { FormControlLabel, Radio, RadioGroup, Stack } from "@mui/material";
import { useState } from "react";
import { readStored } from "../kit/changes.ts";
import { tableIni } from "../kit/keys.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import {
  KUBE_MODULE,
  KUBE_PATH,
  KUBE_TABLE,
  KUBE_TLS_VERSIONS,
  kubeProblem,
  kubeRisks,
  KubeSource,
  sourceOf,
} from "./kubernetes.ts";

export default function KubernetesPage() {
  const panel = usePanel(KUBE_MODULE, KUBE_TABLE);
  const changes = panel.pending();
  const get = (k: string) => panel.value(KUBE_PATH, k);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={KUBE_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const [chosen, setChosen] = useState<KubeSource | null>(null);
  const source = chosen ?? sourceOf(get);
  const risks = kubeRisks(get, source);
  const verify = get("verify mode");
  const verifyOptions = ["peer", "peer-cert", "none"];
  const advancedSet = ["timeout", "max response size", "tls version"].filter(
    (k) => readStored(panel.stored, KUBE_PATH, k) !== undefined,
  ).length;

  const choose = (next: KubeSource) => {
    setChosen(next);
    // The API server wins over a kubeconfig, which wins over in-cluster:
    // what would win over the choice has to go.
    if (next !== "server") panel.set(KUBE_PATH, "api server", "");
    if (next === "incluster") panel.set(KUBE_PATH, "kubeconfig", "");
  };

  return (
    <PanelShell
      title="Kubernetes"
      subheader="Checks a Kubernetes cluster's nodes, pods and workloads through its API."
      panel={panel}
      changes={changes}
      problem={kubeProblem(get, source)}
      ini={(stored, d) => tableIni(stored, KUBE_MODULE, KUBE_TABLE, d)}
    >
      <PanelSection title="Cluster">
        <RadioGroup value={source} onChange={(e) => choose(e.target.value as KubeSource)}>
          <FormControlLabel
            value="incluster"
            control={<Radio size="small" />}
            label="Inside the cluster: the pod's service account"
          />
          <FormControlLabel
            value="kubeconfig"
            control={<Radio size="small" />}
            label="A kubeconfig file"
          />
          <FormControlLabel
            value="server"
            control={<Radio size="small" />}
            label="An API server and a bearer token"
          />
        </RadioGroup>
        {source === "kubeconfig" && (
          <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
            {f({
              key: "kubeconfig",
              label: "Kubeconfig",
              kind: "text",
              help: "JSON only (kubectl config view --raw -o json); exec and auth-provider logins are not supported.",
            })}
            {f({
              key: "context",
              label: "Context",
              kind: "text",
              help: "Its current context when empty.",
            })}
          </Stack>
        )}
        {source === "server" &&
          f({
            key: "api server",
            label: "API server",
            kind: "text",
            help: "https://host:6443, with a path prefix if the API sits behind one.",
          })}
        {source !== "kubeconfig" && (
          <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
            {f({
              key: "token file",
              label: "Token file",
              kind: "text",
              help:
                source === "incluster"
                  ? "The mounted service account token when empty."
                  : "Read on every call; wins over the token.",
            })}
            {f({ key: "token", label: "Token", kind: "secret" })}
          </Stack>
        )}
      </PanelSection>

      <PanelSection title="Trust">
        {risks.map((r) => (
          <RiskBanner key={r.title} risk={r} />
        ))}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "verify mode",
            label: "Verify the API server",
            kind: {
              options: verifyOptions.includes(verify) ? verifyOptions : [...verifyOptions, verify],
            },
            help: "peer checks its certificate against the CA; none checks nothing.",
          })}
          {f({
            key: "ca",
            label: "CA",
            kind: "text",
            help:
              source === "server"
                ? "${ca-path} is the system's trusted roots."
                : "Left at ${ca-path}, the kubeconfig's or the mounted CA is used instead.",
          })}
        </Stack>
      </PanelSection>

      <AdvancedSection count={advancedSet}>
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "tls version",
            label: "TLS version",
            kind: { options: KUBE_TLS_VERSIONS },
            help: "Without + exactly that version.",
          })}
          {f({ key: "timeout", label: "Timeout (seconds)", kind: "number" })}
          {f({
            key: "max response size",
            label: "Largest response (MB)",
            kind: "number",
            help: "0 for no limit.",
          })}
        </Stack>
      </AdvancedSection>
    </PanelShell>
  );
}
