// A script host's scripts (Lua, Python, .NET): each an alias and the file it
// loads. Written as `alias = file`, which every host reads the same way.
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
import { useMemo, useState } from "react";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import { useStagedSettings } from "./staging.ts";

interface Props {
  collectionPath: string;
  title: string;
  subheader: string;
  noun: string;
  fileLabel: string;
  fileHelp: string;
  // What an empty value means (the alias names the file), for the list.
  emptyMeans?: (alias: string) => string;
}

interface Editing {
  original: string | null;
  alias: string;
  file: string;
}

const baseName = (file: string) => file.replace(/^.*[\\/]/, "").replace(/\.(lua|py|dll|exe)$/i, "");

export default function ScriptListEditor({
  collectionPath,
  title,
  subheader,
  noun,
  fileLabel,
  fileHelp,
  emptyMeans = (alias) => alias,
}: Props) {
  const { stored, write } = useStagedSettings();
  const [editing, setEditing] = useState<Editing | null>(null);
  const [deleting, setDeleting] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const entries = useMemo(() => {
    const byAlias = new Map<string, string>();
    for (const s of stored ?? [])
      if (s.path === collectionPath && s.key !== "") byAlias.set(s.key, s.value.trim());
    return [...byAlias.entries()].sort(([a], [b]) => a.localeCompare(b));
  }, [stored, collectionPath]);

  const alias = editing ? editing.alias.trim() || baseName(editing.file.trim()) : "";
  const problem = !editing
    ? null
    : !editing.file.trim()
      ? `Give the ${fileLabel.toLowerCase()}.`
      : /[\s/=]/.test(alias)
        ? "An alias cannot hold spaces, '/' or '='."
        : alias !== editing.original && entries.some(([a]) => a === alias)
          ? `A ${noun} with that alias exists.`
          : null;

  const act = async (fn: () => Promise<void>, done: () => void) => {
    setBusy(true);
    setError(null);
    try {
      await fn();
      done();
    } catch (e) {
      setError(e instanceof Error ? e.message : JSON.stringify(e));
    } finally {
      setBusy(false);
    }
  };
  const save = () =>
    act(
      async () => {
        if (!editing) return;
        await write([{ kind: "set", path: collectionPath,
          key: alias,
          value: editing.file.trim() }]);
        if (editing.original !== null && editing.original !== alias)
          await write([{ kind: "delete", path: collectionPath, key: editing.original }]);
      },
      () => setEditing(null),
    );

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">{title}</Typography>}
        subheader={subheader}
        action={
          <Button
            startIcon={<AddIcon />}
            onClick={() => {
              setError(null);
              setEditing({ original: null, alias: "", file: "" });
            }}
          >
            Add {noun}
          </Button>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {entries.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ px: 2, pb: 2 }}>
            None is loaded.
          </Typography>
        ) : (
          <Table size="small" aria-label={title}>
            <TableHead>
              <TableRow>
                <TableCell>Alias</TableCell>
                <TableCell>{fileLabel}</TableCell>
                <TableCell />
              </TableRow>
            </TableHead>
            <TableBody>
              {entries.map(([a, file]) => (
                <TableRow key={a} hover>
                  <TableCell sx={{ fontFamily: "monospace" }}>{a}</TableCell>
                  <TableCell sx={{ fontFamily: "monospace", wordBreak: "break-all" }}>
                    {file || emptyMeans(a)}
                  </TableCell>
                  <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
                    <Tooltip title="Edit">
                      <IconButton
                        size="small"
                        onClick={() => {
                          setError(null);
                          setEditing({ original: a, alias: a, file: file || emptyMeans(a) });
                        }}
                      >
                        <EditIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                    <Tooltip title="Delete">
                      <IconButton size="small" onClick={() => setDeleting(a)}>
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

      <Dialog open={editing !== null} onClose={() => !busy && setEditing(null)} fullWidth>
        <DialogTitle>
          {editing?.original ? `${title}: ${editing.original}` : `Add ${noun}`}
        </DialogTitle>
        <DialogContent>
          {editing && (
            <Stack spacing={2} sx={{ pt: 1 }}>
              <TextField
                variant="standard"
                label={fileLabel}
                value={editing.file}
                onChange={(e) => setEditing({ ...editing, file: e.target.value })}
                helperText={fileHelp}
                autoFocus
                slotProps={{ htmlInput: { style: { fontFamily: "monospace" } } }}
              />
              <TextField
                variant="standard"
                label="Alias"
                value={editing.alias}
                placeholder={baseName(editing.file)}
                onChange={(e) => setEditing({ ...editing, alias: e.target.value })}
                helperText="Tells two loads of the same file apart; the file's name when empty."
              />
              {problem && <NscpAlert severity="warning" text={problem} />}
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
            onClick={() => void save()}
            disabled={busy || !!problem}
            loading={busy}
          >
            Save
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog open={deleting !== null} onClose={() => !busy && setDeleting(null)}>
        <DialogTitle>Stop loading {deleting}?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            Its commands go away after a reload. The file itself stays.
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
              void act(
                async () => {
                  if (deleting)
                    await write([{ kind: "delete", path: collectionPath, key: deleting }]);
                },
                () => setDeleting(null),
              )
            }
          >
            Remove
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}
