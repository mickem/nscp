// The NRPE server page: the reference for how a tailored module page is built.
//
// Everything it shows is read back from the stored settings through the
// recipes in nrpe.ts, and everything it saves is what those recipes return:
// the page keeps a draft of what the reader changed, shows the resulting
// changes, and writes them together.
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Box,
  Button,
  Card,
  CardContent,
  CardHeader,
  Divider,
  FormControlLabel,
  Stack,
  Switch,
  TextField,
  Typography,
} from "@mui/material";
import { ReactNode, useMemo, useState } from "react";
import { useGetSettingsQuery } from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { allowedHostsChange, readAllowedHosts } from "../kit/allowedHosts.ts";
import { describeChange, pendingChanges, SettingChange, useApplyChanges } from "../kit/changes.ts";
import Explainer from "../kit/Explainer.tsx";
import ModeSelector from "../kit/ModeSelector.tsx";
import { CUSTOM, Custom, ModeOption } from "../kit/modes.ts";
import RiskBanner from "../kit/RiskBanner.tsx";
import ValueField, { ValueKind } from "../kit/ValueField.tsx";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import TlsSection, { TlsFiles } from "../kit/TlsSection.tsx";
import type { ModulePanelProps } from "../types.ts";
import {
  ArgumentsMode,
  argumentsChanges,
  argumentsRisk,
  detectArguments,
  detectSecurity,
  enableNrpe,
  fieldChange,
  identityProblem,
  nastyCharactersChange,
  NRPE_PATH,
  portChange,
  readField,
  readNastyCharacters,
  readPort,
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

interface Draft {
  port: string;
  allowedHosts: string;
  security: SecurityMode | Custom;
  files: TlsFiles;
  args: ArgumentsMode;
  nasty: boolean;
}

function Section({ title, children }: { title: string; children: ReactNode }) {
  return (
    <Stack spacing={2}>
      <Typography variant="subtitle1">{title}</Typography>
      {children}
    </Stack>
  );
}

export default function NrpePage({ onExpertMode }: ModulePanelProps) {
  const { data: stored } = useGetSettingsQuery();
  const applyChanges = useApplyChanges();

  const current: Draft = useMemo(
    () => ({
      port: readPort(stored),
      allowedHosts: readAllowedHosts(stored, NRPE_PATH).value,
      security: detectSecurity(stored),
      files: readTlsFiles(stored),
      args: detectArguments(stored),
      nasty: readNastyCharacters(stored),
    }),
    [stored],
  );
  const storedIsCustom = current.security === CUSTOM;

  // Only what the reader changed; everything else follows the stored values.
  const [edits, setEdits] = useState<Partial<Draft>>({});
  const draft: Draft = { ...current, ...edits };
  const edit = (patch: Partial<Draft>) => setEdits((e) => ({ ...e, ...patch }));

  // Single-value keys the reader changed, by key.
  const [fieldEdits, setFieldEdits] = useState<Record<string, string>>({});
  const fieldValue = (key: string) => fieldEdits[key] ?? readField(stored, key).value;
  const field = (key: string, label: string, kind: ValueKind, help?: ReactNode) => (
    <ValueField
      key={key}
      label={label}
      kind={kind}
      help={help}
      value={fieldValue(key)}
      inherited={readField(stored, key)}
      onChange={(v) => setFieldEdits((f) => ({ ...f, [key]: v }))}
      disabled={busy}
    />
  );

  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saved, setSaved] = useState(false);

  // A control the reader has not touched contributes nothing, so a stored
  // value the page would spell differently is never rewritten behind their back.
  const touched = (field: keyof Draft) => edits[field] !== undefined;
  const problem =
    (draft.security === CUSTOM ? null : securityProblem(draft.security, draft.files)) ??
    identityProblem(draft.security, fieldValue("client identity source"));

  const changes: SettingChange[] = useMemo(() => {
    const out: SettingChange[] = [
      portChange(draft.port),
      allowedHostsChange(stored, NRPE_PATH, draft.allowedHosts),
    ];
    if ((touched("security") || touched("files")) && draft.security !== CUSTOM) {
      out.push(...securityChanges(draft.security, draft.files));
    }
    if (touched("args")) out.push(...argumentsChanges(draft.args));
    if (touched("nasty")) out.push(nastyCharactersChange(draft.nasty));
    for (const [key, value] of Object.entries(fieldEdits))
      out.push(fieldChange(stored, key, value));
    const pending = pendingChanges(stored, out);
    // Configuring NRPE switches it on.
    return pending.length > 0 ? pendingChanges(stored, [...pending, enableNrpe()]) : pending;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [stored, edits, fieldEdits]);

  const onSave = async () => {
    setError(null);
    setSaved(false);
    setBusy(true);
    try {
      await applyChanges(changes);
      setEdits({});
      setFieldEdits({});
      setSaved(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const secRisk = securityRisk(draft.security);
  const tls = draft.security === "two-way-tls" || draft.security === "one-way-tls";
  const versionRisk = tls ? tlsVersionRisk(fieldValue("tls version")) : null;
  const advancedSet = ADVANCED.filter((a) => readField(stored, a.key).source === "own").length;
  const argRisk = argumentsRisk(draft.args, draft.nasty);

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">NRPE server</Typography>}
        subheader="Who may connect, how the connection is secured, and what a request may pass. The same choices as `nscp nrpe install`."
      />
      <CardContent>
        <Stack spacing={3} divider={<Divider flexItem />}>
          <Section title="Connection">
            <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
              <TextField
                label="Port"
                value={draft.port}
                onChange={(e) => edit({ port: e.target.value })}
                variant="standard"
                sx={{ minWidth: 120 }}
                disabled={busy}
              />
              {field(
                "bind to",
                "Bind to address",
                "text",
                "A local IP address to listen on; empty for all.",
              )}
              {field("timeout", "Timeout (seconds)", "number")}
            </Stack>
            <AllowedHostsSection
              value={draft.allowedHosts}
              inherited={readAllowedHosts(stored, NRPE_PATH)}
              onChange={(allowedHosts) => edit({ allowedHosts })}
              disabled={busy}
            />
            {field(
              "cache allowed hosts",
              "Cache allowed hosts",
              "bool",
              "Resolve host names in the list once; turn off for monitoring servers with dynamic addresses.",
            )}
          </Section>

          <Section title="Security">
            {secRisk && <RiskBanner risk={secRisk} />}
            <ModeSelector
              label="How connections are secured"
              options={SECURITY_OPTIONS}
              value={draft.security}
              storedIsCustom={storedIsCustom}
              onChange={(security) => edit({ security })}
              onExpertMode={onExpertMode}
              disabled={busy}
            />
            {tls && (
              <TlsSection
                value={draft.files}
                onChange={(files) => edit({ files })}
                showCa={draft.security === "two-way-tls"}
                disabled={busy}
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
                {field(
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
            {problem && <NscpAlert severity="warning" text={problem} />}
          </Section>

          <Section title="Arguments">
            {argRisk && <RiskBanner risk={argRisk} />}
            <ModeSelector
              label="What a request may pass to the command it runs"
              options={ARGUMENT_OPTIONS}
              value={draft.args}
              storedIsCustom={false}
              onChange={(args) => edit({ args: args as ArgumentsMode })}
              disabled={busy}
            />
            <Explainer to="/modules/CheckExternalScripts" linkText="Open External scripts">
              The safe way to vary a check per host or service is to keep arguments off and give
              each variant a name: an alias in External scripts with the arguments fixed, such as{" "}
              <code>alias_cpu = check_cpu &quot;warn=load &gt; 80%&quot;</code>, which the
              monitoring server then calls without arguments. Whether your scripts accept arguments
              at all is External scripts&apos; own <code>allow arguments</code>, which the last two
              choices set.
            </Explainer>
            {draft.args !== "none" && (
              <FormControlLabel
                control={
                  <Switch
                    size="small"
                    checked={draft.nasty}
                    onChange={(e) => edit({ nasty: e.target.checked })}
                    disabled={busy}
                  />
                }
                label={
                  <span>
                    Allow shell metacharacters in arguments (<code>{"|`&><'\"\\[]{}"}</code>)
                  </span>
                }
              />
            )}
          </Section>

          <Accordion
            disableGutters
            elevation={0}
            sx={{ bgcolor: "transparent", "&:before": { display: "none" } }}
          >
            <AccordionSummary expandIcon={<ExpandMoreIcon />} sx={{ px: 0 }}>
              <Typography variant="subtitle1">
                Advanced{advancedSet > 0 ? ` (${advancedSet} set)` : ""}
              </Typography>
            </AccordionSummary>
            <AccordionDetails sx={{ px: 0 }}>
              <Stack spacing={2}>
                {ADVANCED.map((a) => field(a.key, a.label, a.kind, a.help))}
              </Stack>
            </AccordionDetails>
          </Accordion>

          <Stack spacing={1.5}>
            {changes.length > 0 ? (
              <Box>
                <Typography variant="subtitle2">Saving will make these changes:</Typography>
                <Box
                  component="ul"
                  sx={{ fontFamily: "monospace", fontSize: 13, my: 0.5, pl: 3 }}
                  aria-label="Pending changes"
                >
                  {changes.map((c) => (
                    <li key={`${c.path}/${c.key}`}>{describeChange(c)}</li>
                  ))}
                </Box>
              </Box>
            ) : (
              <Typography variant="body2" color="text.secondary">
                {saved
                  ? "Changed. Use Save and reload to store the settings and apply them."
                  : "No changes."}
              </Typography>
            )}
            {error && <NscpAlert severity="error" text={error} />}
            <Stack direction="row" spacing={1}>
              <Button
                variant="contained"
                onClick={() => void onSave()}
                disabled={busy || changes.length === 0 || !!problem}
                loading={busy}
              >
                Save
              </Button>
              <Button
                onClick={() => {
                  setEdits({});
                  setFieldEdits({});
                }}
                disabled={busy || changes.length === 0}
              >
                Discard
              </Button>
            </Stack>
          </Stack>
        </Stack>
      </CardContent>
    </Card>
  );
}
