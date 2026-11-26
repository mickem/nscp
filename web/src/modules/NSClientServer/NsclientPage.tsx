// The NSClient (check_nt) server page.
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
  COMMAND_GROUPS,
  legacyRisk,
  NSCLIENT_MODULE,
  NSCLIENT_PATH,
  NSCLIENT_TABLE,
  NSCLIENT_TLS,
  nsclientIni,
  passwordRisk,
  transportRisk,
} from "./nsclient.ts";

const ADVANCED: FieldSpec[] = [...CORE_ADVANCED, ...SOCKET_TLS_ADVANCED];

export default function NsclientPage({ onExpertMode }: ModulePanelProps) {
  const tlsDraft = useSocketTlsDraft();
  const panel = usePanel(NSCLIENT_MODULE, NSCLIENT_TABLE, tlsDraft.reset);
  const tls = socketTlsView(tlsDraft.draft, panel.stored, NSCLIENT_PATH, NSCLIENT_TLS);
  const changes = panel.pending(tls.changes);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={NSCLIENT_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const hosts = panel.get(NSCLIENT_PATH, "allowed hosts");
  const pwRisk = passwordRisk(panel.value(NSCLIENT_PATH, "password"));
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, NSCLIENT_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="NSClient server"
      subheader="Answers check_nt, the NSClient protocol of the Nagios check_nt plugin."
      panel={panel}
      changes={changes}
      problem={tls.problem}
      ini={nsclientIni}
    >
      <RiskBanner risk={legacyRisk()} />
      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{f({ key: "port", label: "Port", kind: "number" })}</Stack>
          {f({ key: "timeout", label: "Timeout (seconds)", kind: "number" })}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(NSCLIENT_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(NSCLIENT_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Access">
        {pwRisk && <RiskBanner risk={pwRisk} />}
        {f({
          key: "password",
          label: "Password",
          kind: "secret",
          help: "What check_nt has to send (-s).",
        })}
        {f({
          key: "allow",
          label: "Allowed commands",
          kind: "text",
          help: `Comma separated: any, a group (${COMMAND_GROUPS.join(", ")}) or a command name such as cpuload.`,
        })}
        {f({
          key: "performance data",
          label: "Performance data",
          kind: "bool",
          help: "Return perfdata with results.",
        })}
      </PanelSection>

      <PanelSection title="Transport security">
        <SocketTlsControls
          panel={panel}
          path={NSCLIENT_PATH}
          view={tls}
          setDraft={tlsDraft.setDraft}
          plainDescription="Plain TCP, for check_nt clients that cannot do TLS (many cannot)."
          risk={transportRisk(tls.mode)}
          onExpertMode={onExpertMode}
        />
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
