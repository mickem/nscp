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
  Paper,
  Stack,
  Tooltip,
  Typography,
} from "@mui/material";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import { ReactNode, useContext, useState } from "react";
import { createPortal } from "react-dom";
import { Settings } from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import { applyToStored, describeChange, SettingChange, toIni } from "./changes.ts";
import { PageFrameContext } from "./pageFrame.ts";
import ConfigDialog from "./ConfigDialog.tsx";
import RunCheckButton from "./RunCheckButton.tsx";
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

// The stored sections under the page's lists, as INI rows.
function listSections(
  stored: Settings[],
  paths: string[],
): Array<[string, Array<[string, string]>]> {
  const byPath = new Map<string, Array<[string, string]>>();
  for (const s of stored) {
    if (s.key === "" || !paths.some((p) => s.path === p || s.path.startsWith(p + "/"))) continue;
    byPath.set(s.path, [...(byPath.get(s.path) ?? []), [s.key, s.value]]);
  }
  return [...byPath.entries()].sort(([a], [b]) => a.localeCompare(b));
}

// The panel's unsaved changes, Save and Discard: a bar pinned to the bottom
// of the window, always there so Save is never hunted for, and in reach
// while the reader scrolls through the lists below the panel.
interface PanelTest {
  query: string;
  args?: string[];
  what: string;
}

function SaveBar({
  panel,
  changes,
  problem,
  test,
}: {
  panel: Panel;
  changes: SettingChange[];
  problem: string | null;
  test?: PanelTest;
}) {
  const [details, setDetails] = useState(true);
  const pending = changes.length > 0;
  return (
    <Paper
      elevation={6}
      role="region"
      aria-label="Unsaved changes"
      sx={{ p: 1.5, mt: 1, borderTop: 2, borderColor: pending ? "warning.main" : "divider" }}
    >
      <Stack spacing={1}>
        {problem && <NscpAlert severity="warning" text={problem} />}
        {panel.error && <NscpAlert severity="error" text={panel.error} />}
        <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
          <Typography variant="body2" sx={{ flexGrow: 1 }}>
            {pending
              ? `${changes.length} unsaved ${changes.length === 1 ? "change" : "changes"}`
              : panel.saved
                ? "Changed. Use Save and reload to store the settings and apply them."
                : "No unsaved changes."}
          </Typography>
          {test && (
            <Tooltip
              title={
                pending
                  ? "Save first: the test runs against the settings the agent has loaded."
                  : test.what
              }
              describeChild
            >
              <span>
                <RunCheckButton
                  query={test.query}
                  args={test.args ?? []}
                  label="Test connection"
                  above
                  disabled={pending}
                  note={`Runs ${test.query} with the settings the agent has loaded: after a save, reload the agent first.`}
                />
              </span>
            </Tooltip>
          )}
          {pending && (
            <Button size="small" onClick={() => setDetails(!details)}>
              {details ? "Hide changes" : "Show changes"}
            </Button>
          )}
          <Button onClick={panel.discard} disabled={panel.busy || !pending}>
            Discard
          </Button>
          <Button
            variant="contained"
            onClick={() => void panel.save(changes)}
            disabled={panel.busy || !pending || !!problem}
            loading={panel.busy}
          >
            Save
          </Button>
        </Stack>
        {pending && details && (
          <Box
            component="ul"
            sx={{ fontFamily: "monospace", fontSize: 13, my: 0, pl: 3 }}
            aria-label="Pending changes"
          >
            {changes.map((c) => (
              <li key={`${c.path}/${c.key}`}>{describeChange(c)}</li>
            ))}
          </Box>
        )}
      </Stack>
    </Paper>
  );
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
  const bar = <SaveBar panel={panel} changes={changes} problem={problem} test={test} />;
  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">{title}</Typography>}
        subheader={subheader}
        action={
          <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
            <Button onClick={() => setShowConfig(true)}>Show config</Button>
          </Stack>
        }
      />
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
            const lists = toIni(listSections(stored, frame?.collectionPaths ?? []));
            const own = ini(stored, includeDefaults);
            return lists ? `${own}\n\n${lists}` : own;
          }}
        />
      </CardContent>
    </Card>
  );
}
