// A key/value list a page edits with fields of its own: rows of name and
// value with a line about each, and a dialog the page fills in. The page
// decides what a value means; the card lists, adds, renames and removes.
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
import { ReactNode, useState } from "react";
import { useDeleteSettingsMutation, useUpdateSettingsMutation } from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";

export interface KeyValueRow {
  key: string;
  value: string;
  // Shown under the value: what it resolves to, or why it does not work.
  detail?: ReactNode;
  chip?: { label: string; color?: "default" | "warning" | "error" };
  // A row the agent supplies itself: edited (which stores it), never deleted.
  builtIn?: boolean;
}

interface Props {
  collectionPath: string;
  title: string;
  subheader: string;
  noun: string;
  keyLabel: string;
  keyHelp: string;
  valueLabel: string;
  rows: KeyValueRow[];
  // The dialog's fields for the value.
  renderValue: (value: string, onChange: (v: string) => void) => ReactNode;
  // Why a key and value cannot be saved, or null.
  problem?: (key: string, value: string) => string | null;
  emptyText?: string;
}

interface Editing {
  original: string | null;
  key: string;
  value: string;
}

export default function KeyValueCard({
  collectionPath,
  title,
  subheader,
  noun,
  keyLabel,
  keyHelp,
  valueLabel,
  rows,
  renderValue,
  problem: valueProblem,
  emptyText = "None is defined.",
}: Props) {
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  const [editing, setEditing] = useState<Editing | null>(null);
  const [deleting, setDeleting] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const key = editing?.key.trim() ?? "";
  const problem = !editing
    ? null
    : !key
      ? `Give the ${keyLabel.toLowerCase()}.`
      : /[\s/=]/.test(key)
        ? `A ${keyLabel.toLowerCase()} cannot hold spaces, '/' or '='.`
        : key !== editing.original && rows.some((r) => r.key === key && !r.builtIn)
          ? `A ${noun} named ${key} exists.`
          : (valueProblem?.(key, editing.value) ?? null);

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
              setEditing({ original: null, key: "", value: "" });
            }}
          >
            Add {noun}
          </Button>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {rows.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ px: 2, pb: 2 }}>
            {emptyText}
          </Typography>
        ) : (
          <Table size="small" aria-label={title}>
            <TableHead>
              <TableRow>
                <TableCell>{keyLabel}</TableCell>
                <TableCell>{valueLabel}</TableCell>
                <TableCell />
              </TableRow>
            </TableHead>
            <TableBody>
              {rows.map((r) => (
                <TableRow key={r.key} hover>
                  <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>
                    {r.key}
                  </TableCell>
                  <TableCell>
                    <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
                      <Typography
                        variant="body2"
                        sx={{ fontFamily: "monospace", wordBreak: "break-all" }}
                      >
                        {r.value}
                      </Typography>
                      {r.chip && (
                        <Chip
                          size="small"
                          variant="outlined"
                          color={r.chip.color ?? "default"}
                          label={r.chip.label}
                        />
                      )}
                    </Stack>
                    {r.detail && (
                      <Typography
                        variant="caption"
                        color="text.secondary"
                        component="div"
                        sx={{ wordBreak: "break-all" }}
                      >
                        {r.detail}
                      </Typography>
                    )}
                  </TableCell>
                  <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
                    <Tooltip title="Edit">
                      <IconButton
                        size="small"
                        onClick={() => {
                          setError(null);
                          setEditing({
                            original: r.builtIn ? null : r.key,
                            key: r.key,
                            value: r.value,
                          });
                        }}
                      >
                        <EditIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                    {!r.builtIn && (
                      <Tooltip title="Delete">
                        <IconButton size="small" onClick={() => setDeleting(r.key)}>
                          <DeleteOutlineIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                    )}
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
          {editing?.original ? `${title}: ${editing.original}` : `Add ${noun}`}
        </DialogTitle>
        <DialogContent>
          {editing && (
            <Stack spacing={2} sx={{ pt: 1 }}>
              <TextField
                variant="standard"
                label={keyLabel}
                value={editing.key}
                onChange={(e) => setEditing({ ...editing, key: e.target.value })}
                helperText={keyHelp}
                autoFocus={editing.original === null && editing.key === ""}
              />
              {renderValue(editing.value, (value) => setEditing({ ...editing, value }))}
              {problem && editing.key !== "" && <NscpAlert severity="warning" text={problem} />}
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
              void act(
                async () => {
                  await updateSettings({
                    path: collectionPath,
                    key,
                    value: editing.value.trim(),
                  }).unwrap();
                  if (editing.original !== null && editing.original !== key)
                    await deleteSettings({ path: collectionPath, key: editing.original }).unwrap();
                },
                () => setEditing(null),
              )
            }
          >
            Save
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog open={deleting !== null} onClose={() => !busy && setDeleting(null)}>
        <DialogTitle>
          Delete {noun} {deleting}?
        </DialogTitle>
        <DialogContent>{error && <NscpAlert severity="error" text={error} />}</DialogContent>
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
                    await deleteSettings({ path: collectionPath, key: deleting }).unwrap();
                },
                () => setDeleting(null),
              )
            }
          >
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}
