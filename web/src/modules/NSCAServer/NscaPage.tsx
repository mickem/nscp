// The NSCA server page: where passive results arrive, how they are
// protected, and how they are read.
import { Stack } from "@mui/material";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { readStored } from "../kit/changes.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import {
  CORE_ADVANCED,
  FieldSpec,
  SOCKET_TLS_ADVANCED,
  socketTlsView,
  useSocketTlsDraft,
} from "../kit/socketServer.ts";
import SocketTlsControls from "../kit/SocketTlsControls.tsx";
import { usePanel } from "../kit/usePanel.ts";
import type { ModulePanelProps } from "../types.ts";
import {
  ENCRYPTIONS,
  encryptionRisk,
  NSCA_MODULE,
  NSCA_PATH,
  NSCA_TABLE,
  NSCA_TLS,
  nscaIni,
  passwordProblem,
} from "./nsca.ts";

const ADVANCED: FieldSpec[] = [
  {
    key: "payload length",
    label: "Payload length",
    kind: "number",
    help: "Bytes per packet. Must match send_nsca's compiled-in value; 512 for standard NSCA.",
  },
  ...CORE_ADVANCED,
  ...SOCKET_TLS_ADVANCED,
];

export default function NscaPage({ onExpertMode }: ModulePanelProps) {
  const tlsDraft = useSocketTlsDraft();
  const panel = usePanel(NSCA_MODULE, NSCA_TABLE, tlsDraft.reset);
  const tls = socketTlsView(tlsDraft.draft, panel.stored, NSCA_PATH, NSCA_TLS);
  const changes = panel.pending(tls.changes);
  const value = (key: string) => panel.value(NSCA_PATH, key);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={NSCA_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );

  const encryption = value("encryption");
  const usesTls = tls.mode === "one-way-tls" || tls.mode === "two-way-tls";
  const encRisk = encryptionRisk(encryption, usesTls);
  const problem = tls.problem ?? passwordProblem(encryption, value("password"));
  const hosts = panel.get(NSCA_PATH, "allowed hosts");
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, NSCA_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="NSCA server"
      subheader="Receives passive check results from send_nsca and other NSCA clients, and hands them on to the inbox channel."
      panel={panel}
      changes={changes}
      problem={problem}
      ini={nscaIni}
    >
      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{f({ key: "port", label: "Port", kind: "number" })}</Stack>
          {f({ key: "timeout", label: "Timeout (seconds)", kind: "number" })}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(NSCA_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(NSCA_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Encryption">
        {encRisk && <RiskBanner risk={encRisk} />}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "encryption",
            label: "Encryption",
            kind: { options: ENCRYPTIONS },
            help: "The NSCA payload cipher. Every sending host has to use the same one.",
          })}
          {f({
            key: "password",
            label: "Password",
            kind: "secret",
            help: "The shared key every sending host uses. Not taken from [/settings/default].",
          })}
        </Stack>
      </PanelSection>

      <PanelSection title="Transport security">
        <SocketTlsControls
          panel={panel}
          path={NSCA_PATH}
          view={tls}
          setDraft={tlsDraft.setDraft}
          plainDescription="Plain TCP. The usual NSCA setup: the results are protected by the NSCA encryption above, not by the connection."
          risk={null}
          onExpertMode={onExpertMode}
        />
      </PanelSection>

      <PanelSection title="Results">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "inbox",
            label: "Inbox channel",
            kind: "text",
            help: "The channel received results are posted on.",
          })}
          {f({
            key: "timezone",
            label: "Timezone",
            kind: "text",
            help: "Of the timestamp in the packet: utc as the protocol says, or local for a legacy sender. Both ends must agree.",
          })}
        </Stack>
        {f({
          key: "performance data",
          label: "Keep performance data",
          kind: "bool",
          help: "Turn off to drop the performance data of every received result.",
        })}
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
