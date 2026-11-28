// The External scripts page: whether callers may pass arguments to scripts,
// and how scripts are run. The scripts, aliases and wrappings follow it.
import { Stack } from "@mui/material";
import { readStored } from "../kit/changes.ts";
import Explainer from "../kit/Explainer.tsx";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import { argumentsRisk, EXT_MODULE, EXT_PATH, EXT_TABLE, extIni } from "./ext.ts";

const isOn = (v: string) => v.toLowerCase() === "true" || v === "1";

const ADVANCED: FieldSpec[] = [
  {
    key: "script path",
    label: "Add every file in",
    kind: "text",
    help: "A folder (or folder/*.bat) whose files all become commands, named after the file with its extension. Every file, so keep the folder to scripts.",
  },
  {
    key: "script root",
    label: "Script root",
    kind: "text",
    help: "The folder scripts run in: empty for ${scripts}, fleet, shared, or a folder.",
  },
];

export default function ExtScriptsPage() {
  const panel = usePanel(EXT_MODULE, EXT_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(EXT_PATH, key);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={EXT_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const risk = argumentsRisk(value("allow arguments"), value("allow nasty characters"));
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, EXT_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="External scripts"
      subheader="Runs programs and scripts on this host as checks."
      panel={panel}
      changes={changes}
      problem={null}
      ini={extIni}
    >
      <PanelSection title="Arguments">
        <Explainer to="/modules/CheckHelpers" linkText="Open Check helpers">
          To vary a check, add a fixed alias for each variant in Check helpers: it runs no program,
          and no argument has to travel.
        </Explainer>
        {risk && <RiskBanner risk={risk} />}
        {f({
          key: "allow arguments",
          label: "Let callers pass arguments to scripts",
          kind: "bool",
          help: "Off refuses every request that carries arguments; $ARG1$ and the rest then reach a script as written.",
        })}
        {isOn(value("allow arguments")) &&
          f({
            key: "allow nasty characters",
            label: "Allow shell characters in arguments",
            kind: "bool",
            help: "Off refuses an argument holding | ` & > < ' \" \\ [ ] { }.",
          })}
      </PanelSection>

      <PanelSection title="Running scripts">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "timeout",
            label: "Kill after (seconds)",
            kind: "number",
            help: "A script still running then is killed and the check is UNKNOWN.",
          })}
          {f({
            key: "kill tree",
            label: "Kill what it started too",
            kind: "bool",
            help: "On timeout, also kill the processes the script started.",
          })}
        </Stack>
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
