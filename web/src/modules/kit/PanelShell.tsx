// The frame every module page shares: a card with its sections, the changes
// a save would make, Save / Discard, and Show config.
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
  Stack,
  Typography,
} from "@mui/material";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import { ReactNode, useState } from "react";
import { Settings } from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import { applyToStored, describeChange, SettingChange } from "./changes.ts";
import ConfigDialog from "./ConfigDialog.tsx";
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
  children: ReactNode;
}

export default function PanelShell({
  title,
  subheader,
  panel,
  changes,
  problem,
  ini,
  children,
}: ShellProps) {
  const [showConfig, setShowConfig] = useState(false);
  return (
    <Card>
      <CardHeader title={<Typography variant="h6">{title}</Typography>} subheader={subheader} />
      <CardContent>
        <Stack spacing={3} divider={<Divider flexItem />}>
          {children}
          <Stack spacing={1.5}>
            {problem && <NscpAlert severity="warning" text={problem} />}
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
                {panel.saved
                  ? "Changed. Use Save and reload to store the settings and apply them."
                  : "No changes."}
              </Typography>
            )}
            {panel.error && <NscpAlert severity="error" text={panel.error} />}
            <Stack direction="row" spacing={1}>
              <Button
                variant="contained"
                onClick={() => void panel.save(changes)}
                disabled={panel.busy || changes.length === 0 || !!problem}
                loading={panel.busy}
              >
                Save
              </Button>
              <Button onClick={panel.discard} disabled={panel.busy || changes.length === 0}>
                Discard
              </Button>
              <Box sx={{ flexGrow: 1 }} />
              <Button onClick={() => setShowConfig(true)}>Show config</Button>
            </Stack>
            <ConfigDialog
              open={showConfig}
              onClose={() => setShowConfig(false)}
              title={`${title} configuration`}
              note={
                changes.length > 0
                  ? "Includes the changes not saved yet. Paste it into nsclient.ini on another agent, or into a fleet bundle."
                  : "Paste it into nsclient.ini on another agent, or into a fleet bundle."
              }
              render={(includeDefaults) =>
                ini(applyToStored(panel.stored, changes), includeDefaults)
              }
            />
          </Stack>
        </Stack>
      </CardContent>
    </Card>
  );
}
