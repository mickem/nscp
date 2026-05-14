import {
  Settings,
  SettingsDescription,
  useDeleteSettingsMutation,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../api/api.ts";
import {
  Button,
  ButtonGroup,
  Card,
  CardContent,
  CardHeader,
  ClickAwayListener,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Grow,
  IconButton,
  ListItemText,
  MenuItem,
  MenuList,
  Paper,
  Popper,
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
import { FormEvent, useEffect, useMemo, useRef, useState } from "react";
import AddIcon from "@mui/icons-material/Add";
import ArrowDropDownIcon from "@mui/icons-material/ArrowDropDown";
import DeleteOutlineIcon from "@mui/icons-material/DeleteOutline";
import { getKeyValueTemplates, KeyValueTemplate } from "./keyValueTemplates.ts";

// Paths whose entries are flat key/value pairs stored directly under one
// path (rather than nested object instances with their own sub-schema).
// Add new paths here as we identify them — keep alphabetical for sanity.
export const KEY_VALUE_COLLECTION_PATHS: ReadonlyArray<string> = [
  "/settings/check helpers/alias",
  "/settings/WEB/server/roles",
];

interface Props {
  collectionPath: string;
  descriptions: SettingsDescription[];
}

export default function SettingsKeyValueCollection({ collectionPath, descriptions }: Props) {
  const { data: storedSettings } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();

  const sectionInfo = descriptions.find((s) => s.path === collectionPath && s.key === "");
  const title = sectionInfo?.title || collectionPath;
  const sectionHelp = sectionInfo?.description;

  const pairs = useMemo<Settings[]>(() => {
    if (!storedSettings) return [];
    // /v2/settings can surface the same key multiple times (once per source —
    // schema / ini / registry / cache). Keep the last occurrence so the most
    // recently written value wins.
    const byKey = new Map<string, Settings>();
    for (const s of storedSettings) {
      if (s.path !== collectionPath || s.key === "") continue;
      byKey.set(s.key, s);
    }
    return [...byKey.values()].sort((a, b) => a.key.localeCompare(b.key));
  }, [storedSettings, collectionPath]);
  const existingKeys = useMemo(() => pairs.map((p) => p.key), [pairs]);

  const templates = useMemo(() => getKeyValueTemplates(collectionPath), [collectionPath]);

  const [adding, setAdding] = useState(false);
  const [newKey, setNewKey] = useState("");
  const [newValue, setNewValue] = useState("");
  const [addError, setAddError] = useState<string | null>(null);
  const [addBusy, setAddBusy] = useState(false);
  const keyInputRef = useRef<HTMLInputElement | null>(null);
  const addMenuAnchor = useRef<HTMLDivElement | null>(null);
  const [addMenuOpen, setAddMenuOpen] = useState(false);

  const [confirmDelete, setConfirmDelete] = useState<string | null>(null);
  const [deleteBusy, setDeleteBusy] = useState(false);
  const [deleteError, setDeleteError] = useState<string | null>(null);

  const onOpenAdd = (template?: KeyValueTemplate) => {
    // Suggest the template's default key but dedupe with `_2`, `_3`, … if a
    // pair with that name already exists, so users don't immediately hit the
    // "Name already exists" guard.
    let suggestion = template?.defaultKey ?? "";
    if (suggestion && existingKeys.includes(suggestion)) {
      let i = 2;
      while (existingKeys.includes(`${suggestion}_${i}`)) i++;
      suggestion = `${suggestion}_${i}`;
    }
    setNewKey(suggestion);
    setNewValue(template?.defaultValue ?? "");
    setAddError(null);
    setAdding(true);
  };

  const onConfirmAdd = async () => {
    const key = newKey.trim();
    if (!key) return;
    if (key.includes("/")) {
      setAddError("Name cannot contain '/'");
      return;
    }
    if (existingKeys.includes(key)) {
      setAddError("Name already exists");
      return;
    }
    setAddBusy(true);
    setAddError(null);
    try {
      // Backend treats empty values as a delete; substitute a single space so
      // the path materializes even when the user didn't fill in a value.
      const value = newValue === "" ? " " : newValue;
      await updateSettings({ path: collectionPath, key, value }).unwrap();
      setAdding(false);
    } catch (err) {
      setAddError(err instanceof Error ? err.message : String(err));
    } finally {
      setAddBusy(false);
    }
  };

  const onConfirmDeleteEntry = async () => {
    if (!confirmDelete) return;
    setDeleteBusy(true);
    setDeleteError(null);
    try {
      await deleteSettings({ path: collectionPath, key: confirmDelete }).unwrap();
      setConfirmDelete(null);
    } catch (err) {
      setDeleteError(err instanceof Error ? err.message : String(err));
    } finally {
      setDeleteBusy(false);
    }
  };

  return (
    <Card>
      <CardHeader
        title={
          <Stack direction="row" alignItems="center" spacing={1}>
            <Typography variant="h6">{title}</Typography>
            <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
              {collectionPath}
            </Typography>
          </Stack>
        }
        subheader={sectionHelp}
        action={
          <Stack direction="row" spacing={1}>
            <ButtonGroup size="small" variant="outlined" ref={addMenuAnchor}>
              <Button startIcon={<AddIcon />} onClick={() => onOpenAdd()}>
                Add
              </Button>
              {templates.length > 0 && (
                <Button
                  size="small"
                  aria-label="add from template"
                  onClick={() => setAddMenuOpen((o) => !o)}
                >
                  <ArrowDropDownIcon />
                </Button>
              )}
            </ButtonGroup>
            <Popper
              open={addMenuOpen}
              anchorEl={addMenuAnchor.current}
              transition
              disablePortal
              placement="bottom-end"
              sx={{ zIndex: 1 }}
            >
              {({ TransitionProps }) => (
                <Grow {...TransitionProps}>
                  <Paper>
                    <ClickAwayListener onClickAway={() => setAddMenuOpen(false)}>
                      <MenuList autoFocusItem={addMenuOpen}>
                        <MenuItem
                          onClick={() => {
                            setAddMenuOpen(false);
                            onOpenAdd();
                          }}
                        >
                          <ListItemText primary="Empty entry" />
                        </MenuItem>
                        {templates.map((t) => (
                          <MenuItem
                            key={t.label}
                            onClick={() => {
                              setAddMenuOpen(false);
                              onOpenAdd(t);
                            }}
                          >
                            <ListItemText primary={t.label} secondary={t.description} />
                          </MenuItem>
                        ))}
                      </MenuList>
                    </ClickAwayListener>
                  </Paper>
                </Grow>
              )}
            </Popper>
          </Stack>
        }
      />
      <CardContent>
        {pairs.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ py: 2 }}>
            No entries.
          </Typography>
        ) : (
          <Table size="small">
            <TableHead>
              <TableRow>
                <TableCell sx={{ width: "30%" }}>Name</TableCell>
                <TableCell>Value</TableCell>
                <TableCell sx={{ width: 60 }} />
              </TableRow>
            </TableHead>
            <TableBody>
              {pairs.map((p) => (
                <KeyValueRow
                  key={p.key}
                  path={collectionPath}
                  pair={p}
                  onDelete={() => {
                    setDeleteError(null);
                    setConfirmDelete(p.key);
                  }}
                />
              ))}
            </TableBody>
          </Table>
        )}
      </CardContent>

      <Dialog
        open={adding}
        onClose={() => {
          if (!addBusy) setAdding(false);
        }}
        slotProps={{
          paper: {
            component: "form",
            onSubmit: (e: FormEvent) => {
              e.preventDefault();
              if (!newKey.trim() || addBusy) return;
              void onConfirmAdd();
            },
          },
          transition: {
            onEntered: () => {
              keyInputRef.current?.focus();
              keyInputRef.current?.select();
            },
          },
        }}
      >
        <DialogTitle>Add entry</DialogTitle>
        <DialogContent>
          <Stack spacing={2} sx={{ pt: 1 }}>
            <TextField
              inputRef={keyInputRef}
              onFocus={(e) => (e.target as HTMLInputElement).select()}
              label="Key"
              variant="standard"
              fullWidth
              value={newKey}
              onChange={(e) => {
                setNewKey(e.target.value);
                if (addError) setAddError(null);
              }}
              error={!!addError}
              helperText={addError ?? `Will be created under ${collectionPath} as <key>=<value>`}
              disabled={addBusy}
            />
            <TextField
              label="Value"
              variant="standard"
              fullWidth
              value={newValue}
              onChange={(e) => setNewValue(e.target.value)}
              disabled={addBusy}
            />
          </Stack>
        </DialogContent>
        <DialogActions>
          <Button type="button" onClick={() => setAdding(false)} disabled={addBusy}>
            Cancel
          </Button>
          <Button
            type="submit"
            variant="contained"
            disabled={!newKey.trim() || addBusy}
            loading={addBusy}
          >
            Add
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog
        open={confirmDelete !== null}
        onClose={() => {
          if (!deleteBusy) setConfirmDelete(null);
        }}
      >
        <DialogTitle>Delete entry</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            Delete{" "}
            <Typography component="span" sx={{ fontFamily: "monospace" }}>
              {confirmDelete}
            </Typography>{" "}
            from{" "}
            <Typography component="span" sx={{ fontFamily: "monospace" }}>
              {collectionPath}
            </Typography>
            ?
          </Typography>
          {deleteError && (
            <Typography variant="body2" color="error" sx={{ mt: 1 }}>
              {deleteError}
            </Typography>
          )}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setConfirmDelete(null)} disabled={deleteBusy}>
            Cancel
          </Button>
          <Button
            onClick={() => void onConfirmDeleteEntry()}
            disabled={deleteBusy}
            color="error"
            variant="contained"
            loading={deleteBusy}
          >
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}

interface RowProps {
  path: string;
  pair: Settings;
  onDelete: () => void;
}

function KeyValueRow({ path, pair, onDelete }: RowProps) {
  const [value, setValue] = useState(pair.value);
  const [dirty, setDirty] = useState(false);
  const [saving, setSaving] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();

  useEffect(() => {
    setValue(pair.value);
    setDirty(false);
  }, [pair.value]);

  const persist = async (next: string) => {
    if (next === pair.value) {
      setDirty(false);
      return;
    }
    setSaving(true);
    try {
      // Empty string would delete the key on the backend; the user must use
      // the delete button if they really want to remove the entry.
      const safe = next === "" ? " " : next;
      await updateSettings({ path, key: pair.key, value: safe }).unwrap();
      setDirty(false);
    } finally {
      setSaving(false);
    }
  };

  return (
    <TableRow hover>
      <TableCell sx={{ fontFamily: "monospace", verticalAlign: "middle" }}>{pair.key}</TableCell>
      <TableCell sx={{ verticalAlign: "middle" }}>
        <TextField
          size="small"
          variant="standard"
          fullWidth
          value={value}
          onChange={(e) => {
            setValue(e.target.value);
            setDirty(e.target.value !== pair.value);
          }}
          onBlur={() => {
            if (dirty) void persist(value);
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") {
              (e.target as HTMLInputElement).blur();
            } else if (e.key === "Escape") {
              setValue(pair.value);
              setDirty(false);
              (e.target as HTMLInputElement).blur();
            }
          }}
          disabled={saving}
          slotProps={{ input: { sx: { fontFamily: "monospace" } } }}
        />
      </TableCell>
      <TableCell sx={{ verticalAlign: "middle" }}>
        <Tooltip title="Delete">
          <IconButton size="small" onClick={onDelete} disabled={saving}>
            <DeleteOutlineIcon fontSize="small" />
          </IconButton>
        </Tooltip>
      </TableCell>
    </TableRow>
  );
}
