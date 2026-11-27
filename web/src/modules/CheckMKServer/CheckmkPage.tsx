// The check_mk server page. The services it reports (local and MRPE entries)
// are key/value lists shown below the page.
import { Stack } from "@mui/material";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { readStored } from "../kit/changes.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
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
  CHECKMK_MODULE,
  CHECKMK_PATH,
  CHECKMK_TABLE,
  CHECKMK_TLS,
  checkmkIni,
  transportRisk,
} from "./checkmk.ts";

const ADVANCED: FieldSpec[] = [
  // The agent ships the check_mk script (default_check_mk.lua); moving where
  // the Lua scripts are read from is rarely needed.
  {
    key: "script root",
    label: "Script root",
    kind: "text",
    help: "Where the Lua scripts live: empty for ${scripts}, fleet for scripts a fleet bundle stages, shared, or a folder.",
  },
  ...CORE_ADVANCED,
  ...SOCKET_TLS_ADVANCED,
];

export default function CheckmkPage({ onExpertMode }: ModulePanelProps) {
  const tlsDraft = useSocketTlsDraft();
  const panel = usePanel(CHECKMK_MODULE, CHECKMK_TABLE, tlsDraft.reset);
  const tls = socketTlsView(tlsDraft.draft, panel.stored, CHECKMK_PATH, CHECKMK_TLS);
  const changes = panel.pending(tls.changes);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={CHECKMK_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const hosts = panel.get(CHECKMK_PATH, "allowed hosts");
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, CHECKMK_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="check_mk server"
      subheader="Answers the Checkmk server's agent queries with the sections, local checks and MRPE checks configured below."
      panel={panel}
      changes={changes}
      problem={tls.problem}
      ini={checkmkIni}
    >
      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{f({ key: "port", label: "Port", kind: "number" })}</Stack>
          {f({ key: "timeout", label: "Timeout (seconds)", kind: "number" })}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(CHECKMK_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(CHECKMK_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Transport security">
        <SocketTlsControls
          panel={panel}
          path={CHECKMK_PATH}
          view={tls}
          setDraft={tlsDraft.setDraft}
          plainDescription="Plain TCP, as the Checkmk agent protocol is usually spoken."
          risk={transportRisk(tls.mode)}
          onExpertMode={onExpertMode}
        />
      </PanelSection>

      <PanelSection title="Passive results">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "local channel",
            label: "Local channel",
            kind: "text",
            help: "Results submitted here are reported as cached <<<local>>> entries.",
          })}
          {f({
            key: "mrpe channel",
            label: "MRPE channel",
            kind: "text",
            help: "Results submitted here are reported as cached <<<mrpe>>> entries.",
          })}
        </Stack>
        {f({
          key: "submission ttl",
          label: "Result lifetime (seconds)",
          kind: "number",
          help: "How long a submitted result is reported as fresh; at least the interval that submits it.",
        })}
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
