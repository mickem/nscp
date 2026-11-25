// The NRPE server page: the reference for how a tailored module page is built.
//
// Everything it shows is read back from the stored settings through the
// recipes in nrpe.ts, and everything it saves is what those recipes return:
// the page keeps a draft of what the reader changed, shows the resulting
// changes, and writes them together.
import {
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
import TlsSection, { TlsFiles } from "../kit/TlsSection.tsx";
import type { ModulePanelProps } from "../types.ts";
import {
  ArgumentsMode,
  argumentsChanges,
  argumentsRisk,
  detectArguments,
  detectSecurity,
  enableNrpe,
  nastyCharactersChange,
  NRPE_PATH,
  portChange,
  readNastyCharacters,
  readPort,
  readTlsFiles,
  SecurityMode,
  securityChanges,
  securityProblem,
  securityRisk,
} from "./nrpe.ts";

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

  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saved, setSaved] = useState(false);

  // A control the reader has not touched contributes nothing, so a stored
  // value the page would spell differently is never rewritten behind their back.
  const touched = (field: keyof Draft) => edits[field] !== undefined;
  const problem = draft.security === CUSTOM ? null : securityProblem(draft.security, draft.files);

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
    const pending = pendingChanges(stored, out);
    // Configuring NRPE switches it on.
    return pending.length > 0 ? pendingChanges(stored, [...pending, enableNrpe()]) : pending;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [stored, edits]);

  const onSave = async () => {
    setError(null);
    setSaved(false);
    setBusy(true);
    try {
      await applyChanges(changes);
      setEdits({});
      setSaved(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const secRisk = securityRisk(draft.security);
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
              <AllowedHostsSection
                value={draft.allowedHosts}
                inherited={readAllowedHosts(stored, NRPE_PATH)}
                onChange={(allowedHosts) => edit({ allowedHosts })}
                disabled={busy}
              />
            </Stack>
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
            {(draft.security === "two-way-tls" || draft.security === "one-way-tls") && (
              <TlsSection
                value={draft.files}
                onChange={(files) => edit({ files })}
                showCa={draft.security === "two-way-tls"}
                disabled={busy}
              />
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
              <Button onClick={() => setEdits({})} disabled={busy || changes.length === 0}>
                Discard
              </Button>
            </Stack>
          </Stack>
        </Stack>
      </CardContent>
    </Card>
  );
}
