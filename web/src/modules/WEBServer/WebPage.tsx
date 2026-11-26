// The web server page: the REST API and this UI. Its users and roles are the
// lists below the page.
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
  anonymousRisk,
  WEB_LOG,
  WEB_MODULE,
  WEB_PATH,
  WEB_RESULTS,
  WEB_TABLE,
  webIni,
} from "./web.ts";

type Spec = FieldSpec & { path: string };

const RESULTS: Spec[] = [
  {
    path: WEB_RESULTS,
    key: "enabled",
    label: "Keep passive results for polling",
    kind: "bool",
    help: "Serves them on /api/v2/results. Turning it on takes a service restart.",
  },
  {
    path: WEB_RESULTS,
    key: "channel",
    label: "Channel",
    kind: "text",
    help: "The submission channel results are taken from.",
  },
  {
    path: WEB_RESULTS,
    key: "mode",
    label: "Result kept per key",
    kind: { options: ["last", "worst"] },
    help: "last keeps the newest; worst keeps the most severe since the last poll.",
  },
  {
    path: WEB_RESULTS,
    key: "primary index",
    label: "Key",
    kind: "text",
    help: "What a result is stored under: ${host}, ${source}, ${channel}, ${command}, ${alias}, ${alias-or-command}.",
  },
  {
    path: WEB_RESULTS,
    key: "clear on poll",
    label: "Drop results once polled",
    kind: "bool",
    help: "Turn off when several consumers poll the same agent.",
  },
  {
    path: WEB_RESULTS,
    key: "max entries",
    label: "Keys kept",
    kind: "number",
    help: "The least recently updated key goes first.",
  },
  {
    path: WEB_RESULTS,
    key: "max age",
    label: "Drop results older than (seconds)",
    kind: "number",
    help: "0 keeps them until replaced.",
  },
];

const ADVANCED: Spec[] = [
  {
    path: WEB_PATH,
    key: "cache allowed hosts",
    label: "Cache allowed hosts",
    kind: "bool",
    help: "Resolve host names in the allowed-hosts list once; turn off for callers with dynamic addresses.",
  },
  {
    path: WEB_PATH,
    key: "allowed ciphers",
    label: "Allowed ciphers",
    kind: "text",
    help: "OpenSSL cipher list; empty for the library's own.",
  },
  {
    path: WEB_PATH,
    key: "legacy query auth user agents",
    label: "Clients allowed to log in through the URL",
    kind: "text",
    help: "User-Agent substrings allowed the legacy ?password= login, which leaks into logs. The default admits Icinga's check_nscp_api.",
  },
  {
    path: WEB_PATH,
    key: "max request size",
    label: "Largest request",
    kind: "text",
    help: "A byte count with an optional k, m or g unit (512k, 3MiB).",
  },
  {
    path: WEB_PATH,
    key: "openmetrics format",
    label: "OpenMetrics format",
    kind: { options: ["openmetrics", "legacy"] },
    help: "legacy reproduces the earlier /api/v2/openmetrics body byte for byte.",
  },
  { path: WEB_LOG, key: "error", label: "Log errors", kind: "bool" },
  { path: WEB_LOG, key: "info", label: "Log info messages", kind: "bool" },
  { path: WEB_LOG, key: "debug", label: "Log debug messages", kind: "bool" },
];

export default function WebPage() {
  const panel = usePanel(WEB_MODULE, WEB_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(WEB_PATH, key);
  const f = (s: FieldSpec, path = WEB_PATH) => (
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
  const hosts = panel.get(WEB_PATH, "allowed hosts");
  const anonRisk = anonymousRisk(value("allow anonymous access"));
  const rRisk = rateLimitRisk(value("auth rate limit max failures"));
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, a.path, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="Web server"
      subheader="The REST API and this web interface."
      panel={panel}
      changes={changes}
      problem={null}
      ini={webIni}
    >
      <Explainer>
        This is the server you are using now: a new port, certificate or allowed-hosts list applies
        after Save and reload, and can end this session. Make sure you can reach it the new way
        first.
      </Explainer>

      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{f({ key: "port", label: "Port", kind: "number" })}</Stack>
          {f({
            key: "threads",
            label: "Worker threads",
            kind: "number",
            help: "Requests served at once; a slow check holds its thread.",
          })}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(WEB_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(WEB_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Transport security">
        <HttpTlsControls panel={panel} path={WEB_PATH} exposes="passwords and session keys" />
      </PanelSection>

      <PanelSection title="Logins">
        {f({
          key: "password",
          label: "Admin password",
          kind: "secret",
          help: "The built-in admin user's password; other users are in the list below.",
        })}
        {f({
          key: "disable admin user",
          label: "Disable the admin user",
          kind: "bool",
          help: "No built-in admin at all, for an agent nobody should reconfigure remotely.",
        })}
        {anonRisk && <RiskBanner risk={anonRisk} />}
        {f({
          key: "allow anonymous access",
          label: "Allow anonymous access",
          kind: "bool",
          help: "Answer requests without credentials with what the anonymous role grants.",
        })}
        {f({
          key: "persist sessions",
          label: "Keep sessions across restarts",
          kind: "bool",
          help: "Only a hash of each session key is stored.",
        })}
        {rRisk && <RiskBanner risk={rRisk} />}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "auth rate limit max failures",
            label: "Failed logins before blocking",
            kind: "number",
            help: "Consecutive failures from one address; 0 turns the limit off.",
          })}
          {f({
            key: "auth rate limit block seconds",
            label: "Block for (seconds)",
            kind: "number",
            help: "Doubles for an address guessing at machine speed, up to an hour.",
          })}
        </Stack>
      </PanelSection>

      <PanelSection title="Passive results">{RESULTS.map((r) => f(r, r.path))}</PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map((a) => f(a, a.path))}</AdvancedSection>
    </PanelShell>
  );
}
