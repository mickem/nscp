// The NSClient (check_nt) server page. check_nt has no TLS, so the page
// offers none; the agent's TLS keys are left to expert mode.
import { Button, Stack } from "@mui/material";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { readStored } from "../kit/changes.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import { CORE_ADVANCED, FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import type { ModulePanelProps } from "../types.ts";
import AllowedCommandsPicker from "./AllowedCommandsPicker.tsx";
import {
  allowRisk,
  allowValue,
  emptyAllowRisk,
  legacyRisk,
  NSCLIENT_MODULE,
  NSCLIENT_PATH,
  NSCLIENT_TABLE,
  nsclientIni,
  parseAllow,
  passwordRisk,
  tlsOn,
} from "./nsclient.ts";

const ADVANCED: FieldSpec[] = CORE_ADVANCED;

export default function NsclientPage({ onExpertMode }: ModulePanelProps) {
  const panel = usePanel(NSCLIENT_MODULE, NSCLIENT_TABLE);
  const changes = panel.pending();
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
  const allow = parseAllow(panel.value(NSCLIENT_PATH, "allow"));
  const commandsRisk = emptyAllowRisk(allow.commands) ?? allowRisk(allow.commands);
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, NSCLIENT_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="NSClient server"
      subheader="Answers check_nt, the NSClient protocol of the Nagios check_nt plugin."
      panel={panel}
      changes={changes}
      problem={null}
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
        <RiskBanner
          risk={
            tlsOn(panel.stored)
              ? {
                  severity: "warning",
                  title: "TLS is not supported by check_nt",
                  reason:
                    "The agent's TLS is on (its default for this module), which check_nt clients cannot connect to. Turn off use ssl in expert mode for check_nt to reach it.",
                }
              : {
                  severity: "warning",
                  title: "TLS is not supported by check_nt",
                  reason:
                    "Connections are plain TCP, so the password and every result can be read on the network path.",
                }
          }
        />
        {tlsOn(panel.stored) && (
          <Stack direction="row">
            <Button size="small" onClick={onExpertMode}>
              Open expert mode
            </Button>
          </Stack>
        )}
      </PanelSection>

      <PanelSection title="Access">
        {pwRisk && <RiskBanner risk={pwRisk} />}
        {f({
          key: "password",
          label: "Password",
          kind: "secret",
          help: "What check_nt has to send (-s).",
        })}
        {commandsRisk && <RiskBanner risk={commandsRisk} />}
        <AllowedCommandsPicker
          value={allow}
          onChange={(next) =>
            panel.set(
              NSCLIENT_PATH,
              "allow",
              allowValue(panel.current(NSCLIENT_PATH, "allow").value, next),
            )
          }
          disabled={panel.busy}
        />
        {f({
          key: "performance data",
          label: "Performance data",
          kind: "bool",
          help: "Return perfdata with results.",
        })}
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
