// The bar pinned to the bottom of a module page. Two steps, two rows:
//
//  - This page's changes - its fields and the rows its lists have queued -
//    with Save, which writes them to the running agent, and Discard.
//  - Below it, while the agent holds changes its configuration file does not
//    have yet (from this page or any other): Show diff, and Save and reload,
//    which stores them in the file and reloads the agent.
import { Box, Button, Divider, Paper, Stack, Tooltip, Typography } from "@mui/material";
import { useContext, useState } from "react";
import { useGetSettingsQuery } from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import SettingsDiffDialog from "../../components/atoms/SettingsDiffDialog.tsx";
import { describeChange, enableModuleChange, pendingChanges, SettingChange } from "./changes.ts";
import RunCheckButton from "./RunCheckButton.tsx";
import { describeOp, StagingContext, useWriteOps } from "./staging.ts";
import { useStoreSettings } from "./useStoreSettings.ts";

export interface PanelTest {
  query: string;
  args?: string[];
  what: string;
}

interface Props {
  moduleId: string;
  // The page's own fields' changes; none on a page that is only lists.
  changes: SettingChange[];
  // Writes the page's own changes (the panel keeps its draft and errors).
  onSaveChanges: (changes: SettingChange[]) => Promise<void>;
  onDiscard?: () => void;
  busy?: boolean;
  error?: string | null;
  // Why the page's changes cannot be saved, or null.
  problem?: string | null;
  saved?: boolean;
  onShowConfig: () => void;
  test?: PanelTest;
}

export default function SaveBar({
  moduleId,
  changes,
  onSaveChanges,
  onDiscard,
  busy: pageBusy = false,
  error: pageError = null,
  problem = null,
  saved: pageSaved = false,
  onShowConfig,
  test,
}: Props) {
  const staging = useContext(StagingContext);
  const ops = staging?.ops ?? [];
  const writeOps = useWriteOps();
  const { data: stored } = useGetSettingsQuery();
  const agent = useStoreSettings();
  const [details, setDetails] = useState(true);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saved, setSaved] = useState(false);
  const [diffOpen, setDiffOpen] = useState(false);

  // Configuring a module switches it on, a list row as much as a field.
  const own =
    changes.length > 0 || ops.length === 0
      ? changes
      : pendingChanges(stored, [enableModuleChange(moduleId)]);
  const count = own.length + ops.length;
  const pending = count > 0;
  const working = busy || pageBusy;

  const save = async () => {
    setBusy(true);
    setError(null);
    setSaved(false);
    try {
      if (ops.length > 0) {
        await writeOps(ops);
        staging?.clear();
      }
      await onSaveChanges(own);
      setSaved(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : JSON.stringify(e));
    } finally {
      setBusy(false);
    }
  };
  const discard = () => {
    staging?.clear();
    onDiscard?.();
    setSaved(false);
  };

  return (
    <Paper
      elevation={6}
      role="region"
      aria-label="Unsaved changes"
      sx={{ p: 1.5, mt: 1, borderTop: 2, borderColor: pending ? "warning.main" : "divider" }}
    >
      <Stack spacing={1}>
        {problem && <NscpAlert severity="warning" text={problem} />}
        {pageError && <NscpAlert severity="error" text={pageError} />}
        {error && <NscpAlert severity="error" text={error} />}
        <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
          <Typography variant="body2" sx={{ flexGrow: 1 }}>
            {pending
              ? `${count} unsaved ${count === 1 ? "change" : "changes"}`
              : (saved || pageSaved) && !pageError
                ? "Saved to the agent."
                : "No unsaved changes."}
          </Typography>
          <Button size="small" onClick={onShowConfig}>
            Show config
          </Button>
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
          <Button onClick={discard} disabled={working || !pending}>
            Discard
          </Button>
          <Button
            variant="contained"
            onClick={() => void save()}
            disabled={working || !pending || !!problem}
            loading={working}
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
            {own.map((c) => (
              <li key={`c ${c.path}/${c.key}`}>{describeChange(c)}</li>
            ))}
            {ops.map((op, i) => (
              <li key={`o ${i}`}>{describeOp(op)}</li>
            ))}
          </Box>
        )}
        {agent.unstored && (
          <>
            <Divider />
            {agent.error && <NscpAlert severity="error" text={agent.error} />}
            <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
              <Typography variant="body2" sx={{ flexGrow: 1 }}>
                The agent has saved changes that are not in its configuration file yet.
              </Typography>
              <Button size="small" onClick={() => setDiffOpen(true)}>
                Show diff
              </Button>
              <Tooltip
                title={
                  pending
                    ? "Save or discard this page's changes first: only what the agent holds is written."
                    : "Write the agent's settings to its configuration file and reload it."
                }
                describeChild
              >
                <span>
                  <Button
                    variant="outlined"
                    onClick={() => void agent.store()}
                    disabled={pending || agent.busy}
                    loading={agent.busy}
                  >
                    Save and reload
                  </Button>
                </span>
              </Tooltip>
            </Stack>
          </>
        )}
      </Stack>
      <SettingsDiffDialog open={diffOpen} onClose={() => setDiffOpen(false)} />
    </Paper>
  );
}
