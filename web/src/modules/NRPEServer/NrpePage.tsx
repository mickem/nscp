// The NRPE server page: the reference for how a tailored module page is built.
//
// Everything it shows is read back from the stored settings through the
// recipes in nrpe.ts and the kit, and everything it saves is what those
// recipes return: the page keeps a draft of what the reader changed, shows
// the resulting changes, and writes them together.
import { FormControlLabel, Stack, Switch } from "@mui/material";
import { useCallback, useMemo, useState } from "react";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { readStored, SettingChange } from "../kit/changes.ts";
import Explainer from "../kit/Explainer.tsx";
import ModeSelector from "../kit/ModeSelector.tsx";
import { CUSTOM, Custom, ModeOption } from "../kit/modes.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import TlsSection, { TlsFiles } from "../kit/TlsSection.tsx";
import { usePanel } from "../kit/usePanel.ts";
import type { ValueKind } from "../kit/ValueField.tsx";
import type { ModulePanelProps } from "../types.ts";
import {
  ArgumentsMode,
  argumentsChanges,
  argumentsRisk,
  detectArguments,
  detectSecurity,
  identityProblem,
  nastyCharactersChange,
  NRPE_MODULE,
  NRPE_PATH,
  NRPE_TABLE,
  nrpeIni,
  readNastyCharacters,
  readTlsFiles,
  SecurityMode,
  securityChanges,
  securityProblem,
  securityRisk,
  TLS_VERSIONS,
  tlsVersionRisk,
} from "./nrpe.ts";

// The keys behind the Advanced section, in the order shown.
const ADVANCED: Array<{ key: string; label: string; kind: ValueKind; help?: string }> = [
  {
    key: "bind to",
    label: "Bind to address",
    kind: "text",
    help: "A local IP address to listen on; empty for all.",
  },
  {
    key: "cache allowed hosts",
    label: "Cache allowed hosts",
    kind: "bool",
    help: "Resolve host names in the allowed-hosts list once; turn off for monitoring servers with dynamic addresses.",
  },
  {
    key: "payload length",
    label: "Payload length",
    kind: "number",
    help: "Bytes per packet. Must match check_nrpe's compiled-in value; 1024 for standard NRPE 2.",
  },
  {
    key: "extended response",
    label: "Extended response",
    kind: "bool",
    help: "Send results longer than one packet as several (needs a client that reads them).",
  },
  {
    key: "performance data",
    label: "Performance data",
    kind: "bool",
    help: "Return perfdata with results.",
  },
  {
    key: "thread pool",
    label: "Thread pool",
    kind: "number",
    help: "Worker threads serving connections.",
  },
  {
    key: "socket queue size",
    label: "Listen queue",
    kind: "number",
    help: "Connections queued before new ones are refused; 0 is the system default.",
  },
  {
    key: "encoding",
    label: "Payload encoding",
    kind: "text",
    help: "Character set of requests and results; empty for the system's.",
  },
  { key: "certificate format", label: "Certificate format", kind: { options: ["PEM", "asn1"] } },
  {
    key: "dh",
    label: "DH parameters",
    kind: "text",
    help: "File with the Diffie-Hellman parameters.",
  },
  {
    key: "ssl options",
    label: "SSL options",
    kind: "text",
    help: "Extra OpenSSL flags, comma separated (default-workarounds, no-sslv2, ...).",
  },
  {
    key: "debug verify",
    label: "Debug certificate verification",
    kind: "bool",
    help: "Print certificate verification errors to stdout.",
  },
];

const SECURITY_OPTIONS: ModeOption<SecurityMode>[] = [
  {
    value: "two-way-tls",
    label: "Two-way TLS (recommended)",
    description:
      "The agent presents a certificate and only accepts clients with a certificate issued by your CA.",
  },
  {
    value: "one-way-tls",
    label: "One-way TLS",
    description:
      "The agent presents a certificate and the traffic is encrypted, but clients are not authenticated.",
  },
  {
    value: "insecure",
    label: "Insecure (legacy check_nrpe)",
    description:
      "No server certificate and weak ciphers, for check_nrpe builds that cannot do TLS properly.",
  },
];

const ARGUMENT_OPTIONS: ModeOption<ArgumentsMode>[] = [
  {
    value: "none",
    label: "No arguments (recommended)",
    description: "Commands run exactly as they are defined on the agent.",
  },
  {
    value: "built-ins",
    label: "Arguments for built-in checks",
    description:
      "The monitoring server may pass thresholds and filters to checks such as check_cpu. External scripts still get none.",
  },
  {
    value: "all",
    label: "Arguments for built-in checks and external scripts",
    description: "Requests can also pass arguments to your own scripts.",
  },
];

// What the page's own controls (the modes) hold; single keys live in the
// panel's draft.
interface ModeDraft {
  security: SecurityMode | Custom;
  files: TlsFiles;
  args: ArgumentsMode;
  nasty: boolean;
}

export default function NrpePage({ onExpertMode }: ModulePanelProps) {
  const [edits, setEdits] = useState<Partial<ModeDraft>>({});
  const reset = useCallback(() => setEdits({}), []);
  const panel = usePanel(NRPE_MODULE, NRPE_TABLE, reset);
  const { stored } = panel;

  const current: ModeDraft = useMemo(
    () => ({
      security: detectSecurity(stored),
      files: readTlsFiles(stored),
      args: detectArguments(stored),
      nasty: readNastyCharacters(stored),
    }),
    [stored],
  );
  const draft: ModeDraft = { ...current, ...edits };
  const edit = (patch: Partial<ModeDraft>) => setEdits((e) => ({ ...e, ...patch }));
  // A control the reader has not touched contributes nothing, so a stored
  // value the page would spell differently is never rewritten behind their back.
  const touched = (field: keyof ModeDraft) => edits[field] !== undefined;

  const modeChanges: SettingChange[] = [];
  if ((touched("security") || touched("files")) && draft.security !== CUSTOM)
    modeChanges.push(...securityChanges(draft.security, draft.files));
  if (touched("args")) modeChanges.push(...argumentsChanges(draft.args));
  if (touched("nasty")) modeChanges.push(nastyCharactersChange(draft.nasty));
  const changes = panel.pending(modeChanges);

  const value = (key: string) => panel.value(NRPE_PATH, key);
  const problem =
    (draft.security === CUSTOM ? null : securityProblem(draft.security, draft.files)) ??
    identityProblem(draft.security, value("client identity source"));
  const field = (key: string, label: string, kind: ValueKind, help?: string) => (
    <KeyField
      key={key}
      panel={panel}
      path={NRPE_PATH}
      k={key}
      label={label}
      kind={kind}
      help={help}
    />
  );

  const secRisk = securityRisk(draft.security);
  const argRisk = argumentsRisk(draft.args, draft.nasty);
  const tls = draft.security === "two-way-tls" || draft.security === "one-way-tls";
  const versionRisk = tls ? tlsVersionRisk(value("tls version")) : null;
  const advancedSet = ADVANCED.filter(
    (a) => readStored(stored, NRPE_PATH, a.key) !== undefined,
  ).length;
  const hosts = panel.get(NRPE_PATH, "allowed hosts");

  return (
    <PanelShell
      title="NRPE server"
      subheader="Who may connect, how the connection is secured, and what a request may pass. The same choices as `nscp nrpe install`."
      panel={panel}
      changes={changes}
      problem={problem}
      ini={nrpeIni}
    >
      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{field("port", "Port", "number")}</Stack>
          {field("timeout", "Timeout (seconds)", "number")}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(NRPE_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(NRPE_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Security">
        {secRisk && <RiskBanner risk={secRisk} />}
        <ModeSelector
          label="How connections are secured"
          options={SECURITY_OPTIONS}
          value={draft.security}
          storedIsCustom={current.security === CUSTOM}
          onChange={(security) => edit({ security })}
          onExpertMode={onExpertMode}
          disabled={panel.busy}
        />
        {tls && (
          <TlsSection
            value={draft.files}
            onChange={(files) => edit({ files })}
            showCa={draft.security === "two-way-tls"}
            disabled={panel.busy}
          />
        )}
        {tls && (
          <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
            {field(
              "tls version",
              "TLS version",
              { options: TLS_VERSIONS },
              "A + allows that version and newer.",
            )}
            {/* Only two-way TLS verifies a client certificate to take an
                identity from; a value set without it stays visible so it can
                be put right. */}
            {(draft.security === "two-way-tls" || value("client identity source") !== "none") &&
              field(
                "client identity source",
                "Client identity",
                { options: ["none", "cn"] },
                "cn names the caller after its certificate's Common Name, for the permission system. Needs two-way TLS.",
              )}
          </Stack>
        )}
        {versionRisk && <RiskBanner risk={versionRisk} />}
        {field(
          "expose version",
          "Report the agent version to pings",
          "bool",
          "The unauthenticated check_nrpe ping answers with the exact NSClient++ version. Turn off to not disclose it.",
        )}
      </PanelSection>

      <PanelSection title="Arguments">
        {argRisk && <RiskBanner risk={argRisk} />}
        <ModeSelector
          label="What a request may pass to the command it runs"
          options={ARGUMENT_OPTIONS}
          value={draft.args}
          storedIsCustom={false}
          onChange={(args) => edit({ args: args as ArgumentsMode })}
          disabled={panel.busy}
        />
        <Explainer to="/modules/CheckHelpers" linkText="Open Check helpers">
          The safe way to vary a check per host or service is to keep arguments off and give each
          variant a name: an alias in Check helpers with the arguments fixed, such as{" "}
          <code>alias_cpu = check_cpu &quot;warn=load &gt; 80%&quot;</code>, which the monitoring
          server then calls without arguments. Check helpers gives you aliases without enabling
          External scripts, which runs programs and has its own <code>allow arguments</code> for
          what reaches them, set by the last two choices.
        </Explainer>
        {draft.args !== "none" && (
          <FormControlLabel
            control={
              <Switch
                size="small"
                checked={draft.nasty}
                onChange={(e) => edit({ nasty: e.target.checked })}
                disabled={panel.busy}
              />
            }
            label={
              <span>
                Allow shell metacharacters in arguments (<code>{"|`&><'\"\\[]{}"}</code>)
              </span>
            }
          />
        )}
      </PanelSection>

      <AdvancedSection count={advancedSet}>
        {ADVANCED.map((a) => field(a.key, a.label, a.kind, a.help))}
      </AdvancedSection>
    </PanelShell>
  );
}
