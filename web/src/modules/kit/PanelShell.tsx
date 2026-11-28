// The frame every module page shares: a card with its sections, the changes
// a save would make, Save / Discard, and Show config - the last three in a
// bar the page pins to the bottom of the window.
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Card,
  CardContent,
  CardHeader,
  Divider,
  Stack,
  Typography,
} from "@mui/material";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import { ReactNode, useContext, useState } from "react";
import { createPortal } from "react-dom";
import { Settings } from "../../api/api.ts";
import { applyToStored, SettingChange } from "./changes.ts";
import { listsIni } from "./listsIni.ts";
import { PageFrameContext } from "./pageFrame.ts";
import ConfigDialog from "./ConfigDialog.tsx";
import SaveBar, { PanelTest } from "./SaveBar.tsx";
import SecretField from "./SecretField.tsx";
import type { Panel } from "./usePanel.ts";
import ValueField, { ValueKind } from "./ValueField.tsx";

export function PanelSection({ title, children }: { title: string; children: ReactNode }) {
  return (
    <Stack spacing={2}>
      <Typography variant="subtitle1">{title}</Typography>
      {children}
    </Stack>
  );
}

// The rarely changed keys, collapsed. `count` says how many the module sets
// itself, so a set value is not hidden without a hint.
export function AdvancedSection({ count, children }: { count: number; children: ReactNode }) {
  return (
    <Accordion
      disableGutters
      elevation={0}
      sx={{ bgcolor: "transparent", "&:before": { display: "none" } }}
    >
      <AccordionSummary expandIcon={<ExpandMoreIcon />} sx={{ px: 0 }}>
        <Typography variant="subtitle1">Advanced{count > 0 ? ` (${count} set)` : ""}</Typography>
      </AccordionSummary>
      <AccordionDetails sx={{ px: 0 }}>
        <Stack spacing={2}>{children}</Stack>
      </AccordionDetails>
    </Accordion>
  );
}

interface KeyFieldProps {
  panel: Panel;
  path: string;
  k: string;
  label: string;
  kind: ValueKind | "secret";
  help?: ReactNode;
}

// One key of the page's table, with a Default / Local switch where the agent
// can inherit it.
export function KeyField({ panel, path, k, label, kind, help }: KeyFieldProps) {
  const state = panel.get(path, k);
  const onScopeChange = state.scope
    ? (s: "default" | "local") => panel.setScope(path, k, s)
    : undefined;
  if (kind === "secret") {
    return (
      <SecretField
        label={label}
        help={help}
        current={
          state.scope === panel.current(path, k).scope ? panel.current(path, k).value : state.value
        }
        value={state.value}
        onChange={(v) => panel.set(path, k, v)}
        scope={state.scope}
        onScopeChange={onScopeChange}
        disabled={panel.busy}
      />
    );
  }
  return (
    <ValueField
      label={label}
      kind={kind}
      help={help}
      value={state.value}
      onChange={(v) => panel.set(path, k, v)}
      scope={state.scope}
      onScopeChange={onScopeChange}
      disabled={panel.busy}
    />
  );
}

interface ShellProps {
  title: string;
  subheader: string;
  panel: Panel;
  changes: SettingChange[];
  // Why the changes cannot be saved, or null.
  problem: string | null;
  // The INI fragment for a stored state (the stored settings with the
  // unsaved changes applied).
  ini: (stored: Settings[], includeDefaults: boolean) => string;
  // A cheap, read-only check that shows the module reaches what it is
  // configured for (a server, an engine, a cluster). It runs against what the
  // agent has loaded, so not while there are unsaved changes.
  test?: PanelTest;
  children: ReactNode;
}

export default function PanelShell({
  title,
  subheader,
  panel,
  changes,
  problem,
  ini,
  test,
  children,
}: ShellProps) {
  const [showConfig, setShowConfig] = useState(false);
  const frame = useContext(PageFrameContext);
  const bar = (
    <SaveBar
      moduleId={panel.moduleId}
      changes={changes}
      onSaveChanges={panel.save}
      onDiscard={panel.discard}
      busy={panel.busy}
      error={panel.error}
      problem={problem}
      saved={panel.saved}
      test={test}
      onShowConfig={() => setShowConfig(true)}
    />
  );
  return (
    <Card>
      <CardHeader title={<Typography variant="h6">{title}</Typography>} subheader={subheader} />
      <CardContent>
        <Stack spacing={3} divider={<Divider flexItem />}>
          {children}
        </Stack>
        {/* Without a page around it (a test, a wizard step) the bar stays here. */}
        {frame?.saveBar ? createPortal(bar, frame.saveBar) : bar}
        <ConfigDialog
          open={showConfig}
          onClose={() => setShowConfig(false)}
          title={`${title} configuration`}
          note={
            changes.length > 0
              ? "Includes the changes not saved yet. Paste it into nsclient.ini on another agent, or into a fleet bundle."
              : "Paste it into nsclient.ini on another agent, or into a fleet bundle."
          }
          render={(includeDefaults) => {
            const stored = applyToStored(panel.stored, changes);
            const lists = listsIni(stored, frame?.collectionPaths ?? []);
            const own = ini(stored, includeDefaults);
            return lists ? `${own}\n\n${lists}` : own;
          }}
        />
      </CardContent>
    </Card>
  );
}
