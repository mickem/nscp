// A list of aliases (name = command line): each with the check it runs, and
// a dialog that picks the check and its arguments, starting from a preset.
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Chip,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  IconButton,
  ListItemText,
  Menu,
  MenuItem,
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
import { ReactNode, useMemo, useState } from "react";
import {
  useDeleteSettingsMutation,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import type { KeyValueTemplate } from "../types.ts";
import { commandLineProblem, placeholders, splitCommand } from "./commandLine.ts";
import CommandLineField from "./CommandLineField.tsx";

interface Props {
  collectionPath: string;
  title: string;
  subheader: string;
  presets?: KeyValueTemplate[];
  // Shown in the dialog above the command, e.g. what $ARG1$ needs.
  note?: ReactNode;
  argumentsHelp?: string;
}

interface Editing {
  original: string | null;
  name: string;
  line: string;
}

export default function AliasEditor({
  collectionPath,
  title,
  subheader,
  presets = [],
  note,
  argumentsHelp,
}: Props) {
  const { data: stored } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  const [editing, setEditing] = useState<Editing | null>(null);
  const [deleting, setDeleting] = useState<string | null>(null);
  const [menu, setMenu] = useState<HTMLElement | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const aliases = useMemo(() => {
    const byName = new Map<string, string>();
    for (const s of stored ?? [])
      if (s.path === collectionPath && s.key !== "") byName.set(s.key, s.value);
    return [...byName.entries()].sort(([a], [b]) => a.localeCompare(b));
  }, [stored, collectionPath]);
  const taken = new Set(aliases.map(([n]) => n));

  const open = (e: Editing) => {
    setError(null);
    setEditing(e);
  };
  const fromPreset = (p: KeyValueTemplate) => {
    let name = p.defaultKey;
    for (let i = 2; taken.has(name); ++i) name = `${p.defaultKey}_${i}`;
    open({ original: null, name, line: p.defaultValue });
  };

  const save = async () => {
    if (!editing) return;
    setBusy(true);
    setError(null);
    try {
      const name = editing.name.trim();
      await updateSettings({
        path: collectionPath,
        key: name,
        value: editing.line.trim(),
      }).unwrap();
      // A rename writes the new key, then drops the old one.
      if (editing.original !== null && editing.original !== name)
        await deleteSettings({ path: collectionPath, key: editing.original }).unwrap();
      setEditing(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : JSON.stringify(e));
    } finally {
      setBusy(false);
    }
  };
  const remove = async () => {
    if (!deleting) return;
    setBusy(true);
    try {
      await deleteSettings({ path: collectionPath, key: deleting }).unwrap();
      setDeleting(null);
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
      ? "Give the alias a name."
      : /[\s/=]/.test(name)
        ? "An alias name cannot hold spaces, '/' or '='."
        : name !== editing.original && taken.has(name)
          ? "An alias with that name exists."
          : !splitCommand(editing.line).command
            ? "Pick the command it runs."
            : commandLineProblem(editing.line);

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">{title}</Typography>}
        subheader={subheader}
        action={
          <>
            <Button startIcon={<AddIcon />} onClick={(e) => setMenu(e.currentTarget)}>
              Add alias
            </Button>
            <Menu anchorEl={menu} open={menu !== null} onClose={() => setMenu(null)}>
              <MenuItem
                onClick={() => {
                  setMenu(null);
                  open({ original: null, name: "", line: "" });
                }}
              >
                <ListItemText primary="Empty alias" />
              </MenuItem>
              {presets.map((p) => (
                <MenuItem
                  key={p.label}
                  onClick={() => {
                    setMenu(null);
                    fromPreset(p);
                  }}
                >
                  <ListItemText primary={p.label} secondary={p.description} />
                </MenuItem>
              ))}
            </Menu>
          </>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {aliases.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ px: 2, pb: 2 }}>
            No aliases are defined.
          </Typography>
        ) : (
          <Table size="small" aria-label="Aliases">
            <TableHead>
              <TableRow>
                <TableCell>Alias</TableCell>
                <TableCell>Runs</TableCell>
                <TableCell />
              </TableRow>
            </TableHead>
            <TableBody>
              {aliases.map(([alias, line]) => {
                const holes = placeholders(line);
                return (
                  <TableRow key={alias} hover>
                    <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>
                      {alias}
                    </TableCell>
                    <TableCell>
                      <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
                        <Typography
                          variant="body2"
                          sx={{ fontFamily: "monospace", wordBreak: "break-all" }}
                        >
                          {line}
                        </Typography>
                        {holes.length > 0 && (
                          <Chip size="small" variant="outlined" label="takes arguments" />
                        )}
                      </Stack>
                    </TableCell>
                    <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
                      <Tooltip title="Edit">
                        <IconButton
                          size="small"
                          onClick={() => open({ original: alias, name: alias, line })}
                        >
                          <EditIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                      <Tooltip title="Delete">
                        <IconButton size="small" onClick={() => setDeleting(alias)}>
                          <DeleteOutlineIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                    </TableCell>
                  </TableRow>
                );
              })}
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
          {editing?.original === null ? "Add alias" : `Alias ${editing?.original}`}
        </DialogTitle>
        <DialogContent>
          {editing && (
            <Stack spacing={2} sx={{ pt: 1 }}>
              <TextField
                variant="standard"
                label="Name"
                value={editing.name}
                onChange={(e) => setEditing({ ...editing, name: e.target.value })}
                helperText="What the monitoring server calls, like any other check."
                autoFocus={editing.original === null}
              />
              {note}
              <CommandLineField
                value={editing.line}
                onChange={(line) => setEditing({ ...editing, line })}
                disabled={busy}
                argumentsHelp={argumentsHelp}
              />
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
            onClick={() => void save()}
            disabled={busy || !!problem}
            loading={busy}
          >
            Save
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog open={deleting !== null} onClose={() => !busy && setDeleting(null)}>
        <DialogTitle>Delete alias {deleting}?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            A monitoring server that still calls it gets an unknown-command answer.
          </Typography>
          {error && <NscpAlert severity="error" text={error} />}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setDeleting(null)} disabled={busy}>
            Cancel
          </Button>
          <Button color="error" variant="contained" onClick={() => void remove()} loading={busy}>
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}
