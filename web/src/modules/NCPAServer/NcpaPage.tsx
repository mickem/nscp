// The NCPA server page: the Nagios NCPA HTTP API, polled with check_ncpa.py
// and the Nagios XI NCPA wizard.
import { Stack } from "@mui/material";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { readStored } from "../kit/changes.ts";
import Explainer from "../kit/Explainer.tsx";
import { rateLimitRisk } from "../kit/httpTls.ts";
import HttpTlsControls from "../kit/HttpTlsControls.tsx";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import {
  argumentsRisk,
  NCPA_LOG,
  NCPA_MODULE,
  NCPA_PATH,
  NCPA_TABLE,
  ncpaIni,
  tokenRisk,
} from "./ncpa.ts";

const ADVANCED: Array<FieldSpec & { path: string }> = [
  {
    path: NCPA_PATH,
    key: "bind to",
    label: "Bind to address",
    kind: "text",
    help: "A local IP address to listen on; empty for all.",
  },
  {
    path: NCPA_PATH,
    key: "cache allowed hosts",
    label: "Cache allowed hosts",
    kind: "bool",
    help: "Resolve host names in the allowed-hosts list once; turn off for pollers with dynamic addresses.",
  },
  {
    path: NCPA_PATH,
    key: "allowed ciphers",
    label: "Allowed ciphers",
    kind: "text",
    help: "OpenSSL cipher list; empty for the library's own.",
  },
  { path: NCPA_LOG, key: "error", label: "Log errors", kind: "bool" },
  { path: NCPA_LOG, key: "info", label: "Log info messages", kind: "bool" },
  { path: NCPA_LOG, key: "debug", label: "Log debug messages", kind: "bool" },
];

export default function NcpaPage() {
  const panel = usePanel(NCPA_MODULE, NCPA_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(NCPA_PATH, key);
  const f = (s: FieldSpec, path = NCPA_PATH) => (
    <KeyField
      key={`${path}/${s.key}`}
      panel={panel}
      path={path}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const hosts = panel.get(NCPA_PATH, "allowed hosts");
  const tRisk = tokenRisk(value("token"));
  const aRisk = argumentsRisk(value("allow arguments"), value("plugins"));
  const rRisk = rateLimitRisk(value("auth rate limit max failures"));
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, a.path, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="NCPA server"
      subheader="The Nagios NCPA API, polled with check_ncpa.py and the Nagios XI NCPA wizard."
      panel={panel}
      changes={changes}
      problem={null}
      ini={ncpaIni}
    >
      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{f({ key: "port", label: "Port", kind: "number" })}</Stack>
          {f({
            key: "threads",
            label: "Worker threads",
            kind: "number",
            help: "Requests answered at once; a slow check holds its thread.",
          })}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(NCPA_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(NCPA_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Access">
        {tRisk && <RiskBanner risk={tRisk} />}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "token",
            label: "Token",
            kind: "secret",
            help: "What check_ncpa.py sends with -t.",
          })}
          {f({
            key: "backup token",
            label: "Backup token",
            kind: "secret",
            help: "A second accepted token, to rotate the first without a gap.",
          })}
        </Stack>
        {rRisk && <RiskBanner risk={rRisk} />}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "auth rate limit max failures",
            label: "Failed tokens before blocking",
            kind: "number",
            help: "Consecutive wrong tokens from one address; 0 turns the limit off.",
          })}
          {f({
            key: "auth rate limit block seconds",
            label: "Block for (seconds)",
            kind: "number",
            help: "Doubles for an address guessing at machine speed, up to an hour.",
          })}
        </Stack>
      </PanelSection>

      <PanelSection title="Checks">
        {aRisk && <RiskBanner risk={aRisk} />}
        {f({
          key: "plugins",
          label: "Exposed queries",
          kind: "text",
          help: "any (every query, external scripts included), scripts (only external scripts), or a comma-separated list of query names.",
        })}
        {f({
          key: "allow arguments",
          label: "Allow arguments",
          kind: "bool",
          help: "Let a poll pass arguments to the query it runs (check_ncpa -a).",
        })}
        <Explainer to="/modules/CheckHelpers" linkText="Open Check helpers">
          To vary a check without accepting arguments, give each variant a name: an alias in Check
          helpers with the arguments fixed, which the poller then runs without arguments.
        </Explainer>
      </PanelSection>

      <PanelSection title="Transport security">
        <HttpTlsControls panel={panel} path={NCPA_PATH} exposes="the token" />
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map((a) => f(a, a.path))}</AdvancedSection>
    </PanelShell>
  );
}
