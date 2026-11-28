// External scripts' scripts: each a command line run as a check. A script
// with nothing but its command is a one-line entry; one with options (run as
// another user, output handling) is a section of its own.
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  IconButton,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  TextField,
  Tooltip,
  Typography,
} from "@mui/material";
import AddIcon from "@mui/icons-material/Add";
import DeleteOutlineIcon from "@mui/icons-material/DeleteOutlined";
import EditIcon from "@mui/icons-material/Edit";
import { useState } from "react";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import { placeholders } from "../kit/commandLine.ts";
import { AdvancedSection } from "../kit/PanelShell.tsx";
import SecretField from "../kit/SecretField.tsx";
import ValueField from "../kit/ValueField.tsx";
import { deleteEntry } from "../kit/entries.ts";
import { EXT_SCRIPTS, readScripts, Script, scriptWrites, Write } from "./ext.ts";
import { useStagedSettings } from "../kit/staging.ts";

interface Editing {
  original: Script | null;
  name: string;
  values: Record<string, string>;
}

export default function ScriptsEditor() {
  const { stored, write } = useStagedSettings();
  const [editing, setEditing] = useState<Editing | null>(null);
  const [deleting, setDeleting] = useState<Script | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const scripts = readScripts(stored);

  const apply = async (writes: Write[]) => {
    for (const w of writes) {
      if (w.kind === "put")
        await write([{ kind: "set", path: w.path, key: w.key ?? "", value: w.value ?? "" }]);
      else if (w.kind === "delete") await write([
          w.key === undefined
            ? { kind: "deletePath", path: w.path }
            : { kind: "delete", path: w.path, key: w.key },
        ]);
      else await write([{ kind: "deletePath", path: w.path }]);
    }
  };
  const run = async (writes: Write[], done: () => void) => {
    setBusy(true);
    setError(null);
    try {
      await apply(writes);
      done();
    } catch (e) {
      setError(e instanceof Error ? e.message : JSON.stringify(e));
    } finally {
      setBusy(false);
    }
  };

  const name = editing?.name.trim() ?? "";
  const problem = !editing
    ? null
    : !name
      ? "Give the script a name."
      : /[\s/=]/.test(name)
        ? "A script name cannot hold spaces, '/' or '='."
        : name !== editing.original?.name && scripts.some((s) => s.name === name)
          ? "A script with that name exists."
          : !(editing.values.command ?? "").trim()
            ? "Give the command line it runs."
            : null;
  const v = (k: string) => editing?.values[k] ?? "";
  const set = (k: string, x: string) =>
    editing && setEditing({ ...editing, values: { ...editing.values, [k]: x } });
  const optionsSet = ["user", "domain", "password", "session", "display", "encoding"].filter(
    (k) => v(k) !== "",
  ).length;

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">Scripts</Typography>}
        subheader="Programs run as checks, each under the name the monitoring server calls."
        action={
          <Button
            startIcon={<AddIcon />}
            onClick={() => {
              setError(null);
              setEditing({ original: null, name: "", values: { command: "" } });
            }}
          >
            Add script
          </Button>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {scripts.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ px: 2, pb: 2 }}>
            No scripts are defined.
          </Typography>
        ) : (
          <Table size="small" aria-label="Scripts">
            <TableHead>
              <TableRow>
                <TableCell>Script</TableCell>
                <TableCell>Runs</TableCell>
                <TableCell />
              </TableRow>
            </TableHead>
            <TableBody>
              {scripts.map((s) => (
                <TableRow key={s.name} hover>
                  <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>
                    {s.name}
                  </TableCell>
                  <TableCell>
                    <Typography
                      variant="body2"
                      sx={{ fontFamily: "monospace", wordBreak: "break-all" }}
                    >
                      {s.values.command ?? ""}
                    </Typography>
                    {s.values.user && (
                      <Typography variant="caption" color="text.secondary">
                        as {s.values.domain ? `${s.values.domain}\\` : ""}
                        {s.values.user}
                      </Typography>
                    )}
                  </TableCell>
                  <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
                    <Tooltip title="Edit">
                      <IconButton
                        size="small"
                        onClick={() => {
                          setError(null);
                          setEditing({ original: s, name: s.name, values: { ...s.values } });
                        }}
                      >
                        <EditIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                    <Tooltip title="Delete">
                      <IconButton size="small" onClick={() => setDeleting(s)}>
                        <DeleteOutlineIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        )}
      </CardContent>

      <Dialog
        open={editing !== null}
        onClose={() => !busy && setEditing(null)}
        maxWidth="md"
        fullWidth
      >
        <DialogTitle>
          {editing?.original ? `Script ${editing.original.name}` : "Add script"}
        </DialogTitle>
        <DialogContent>
          {editing && (
            <Stack spacing={2} sx={{ pt: 1 }}>
              <TextField
                variant="standard"
                label="Name"
                value={editing.name}
                onChange={(e) => setEditing({ ...editing, name: e.target.value })}
                helperText="What the monitoring server calls."
                autoFocus={!editing.original}
              />
              <TextField
                variant="standard"
                label="Command line"
                value={v("command")}
                onChange={(e) => set("command", e.target.value)}
                multiline
                helperText='Run as written, from the script root: scripts\check_foo.bat $ARG1$. $ARG1$, $ARGS$ and $ARGS"$ take what the caller sends, only while arguments are allowed above.'
                slotProps={{ htmlInput: { style: { fontFamily: "monospace" } } }}
              />
              {placeholders(v("command")).length > 0 && (
                <Typography variant="caption" color="text.secondary">
                  Takes {placeholders(v("command")).join(", ")} from its caller.
                </Typography>
              )}
              <AdvancedSection count={optionsSet}>
                <Typography variant="subtitle2">Run as (Windows)</Typography>
                <Typography variant="caption" color="text.secondary">
                  On Linux a script with any of these set is refused.
                </Typography>
                <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
                  <ValueField
                    label="User"
                    kind="text"
                    value={v("user")}
                    onChange={(x) => set("user", x)}
                  />
                  <ValueField
                    label="Domain"
                    kind="text"
                    value={v("domain")}
                    onChange={(x) => set("domain", x)}
                  />
                </Stack>
                <SecretField
                  label="Password"
                  current={editing.original?.values.password ?? ""}
                  value={v("password")}
                  onChange={(x) => set("password", x)}
                />
                <ValueField
                  label="Session"
                  kind="text"
                  value={v("session")}
                  onChange={(x) => set("session", x)}
                  help="A session number, or current: run in a logged-in user's session."
                />
                <ValueField
                  label="Show its window"
                  kind="bool"
                  value={v("display") || "false"}
                  onChange={(x) => set("display", x === "false" ? "" : x)}
                />
                <Typography variant="subtitle2">Output</Typography>
                <ValueField
                  label="Read its output"
                  kind="bool"
                  value={v("capture output") || "true"}
                  onChange={(x) => set("capture output", x === "true" ? "" : x)}
                  help="Off starts it and does not wait for what it prints."
                />
                <ValueField
                  label="Keep | in the output as text"
                  kind="bool"
                  value={v("ignore perfdata") || "false"}
                  onChange={(x) => set("ignore perfdata", x === "false" ? "" : x)}
                  help="On: no performance data is split off the output."
                />
                <ValueField
                  label="Output encoding"
                  kind="text"
                  value={v("encoding")}
                  onChange={(x) => set("encoding", x)}
                  help="How to read what it prints, when not the system's code page (utf-8, ...)."
                />
              </AdvancedSection>
              {problem && editing.name !== "" && <NscpAlert severity="warning" text={problem} />}
              {error && <NscpAlert severity="error" text={error} />}
            </Stack>
          )}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setEditing(null)} disabled={busy}>
            Cancel
          </Button>
          <Button
            variant="contained"
            disabled={busy || !!problem}
            loading={busy}
            onClick={() =>
              editing &&
              void run(scriptWrites(editing.original, name, editing.values), () => setEditing(null))
            }
          >
            Save
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog open={deleting !== null} onClose={() => !busy && setDeleting(null)}>
        <DialogTitle>Delete script {deleting?.name}?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            A monitoring server that still calls it gets an unknown-command answer. The file itself
            stays.
          </Typography>
          {error && <NscpAlert severity="error" text={error} />}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setDeleting(null)} disabled={busy}>
            Cancel
          </Button>
          <Button
            color="error"
            variant="contained"
            loading={busy}
            onClick={() =>
              deleting && void run([deleteEntry(EXT_SCRIPTS, deleting)], () => setDeleting(null))
            }
          >
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}
