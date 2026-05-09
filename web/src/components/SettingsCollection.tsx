import {
  SettingsDescription,
  useDeleteSettingsMutation,
  useUpdateSettingsMutation,
} from "../api/api.ts";
import {
  Button,
  ButtonGroup,
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
import BookmarkBorderIcon from "@mui/icons-material/BookmarkBorder";
import EditIcon from "@mui/icons-material/Edit";
import SettingsInstanceDialog, { FieldGroup } from "./SettingsInstanceDialog.tsx";
import DeleteOutlineIcon from "@mui/icons-material/DeleteOutline";
import { useMemo, useState } from "react";

interface Props {
  // Parent collection path, e.g. "/settings/system/windows/counters".
  collectionPath: string;
  // Full settings descriptions list (used to find instances and template keys).
  settings: SettingsDescription[];
  // Optional name of a child path that holds the template/sample (default "default").
  templateName?: string;
}

interface Instance {
  name: string;
  path: string;
  fields: SettingsDescription[];
  isTemplate: boolean;
}

const isTemplateValue = (v: string | undefined) => (v ?? "").toLowerCase() === "true";

// RTK Query rejects with either a FetchBaseQueryError ({status, data})
// or a plain Error. Pull out the most meaningful text we can find.
function formatError(err: unknown): string {
  if (err instanceof Error) return err.message;
  if (typeof err === "object" && err !== null) {
    const e = err as { data?: unknown; status?: number | string; error?: string };
    if (e.data) {
      if (typeof e.data === "string") return e.data;
      if (typeof e.data === "object" && e.data !== null) {
        const d = e.data as { error?: string; message?: string };
        if (d.error) return d.error;
        if (d.message) return d.message;
      }
    }
    if (e.error) return e.error;
    if (e.status !== undefined) return `HTTP ${e.status}`;
  }
  try {
    return JSON.stringify(err);
  } catch {
    return String(err);
  }
}

// Per-field visibility rules. Returns false to hide a field based on the
// current values of sibling fields. POC: only counter-specific rules.
function isFieldVisible(
  key: string,
  fields: SettingsDescription[],
  isTemplate: boolean,
  isDefaultTemplate: boolean,
): boolean {
  // is_template flag is set at creation time via the Add / Add template
  // buttons — never editable inline.
  if (key === "is template") return false;
  // `target` is documented as an alias for `destination` — hide it so the
  // user doesn't see two fields with the title "DESTINATION".
  if (key === "target") return false;
  // `debug` is rarely toggled at the per-instance level — hide from the editor.
  if (key === "debug") return false;
  // Templates describe schema for child instances; they don't bind to a
  // concrete PDH counter themselves and don't carry per-instance aliases.
  if ((key === "counter" || key === "alias") && isTemplate) return false;
  // The default template is the root of the inheritance chain — it has no parent.
  if (key === "parent" && isDefaultTemplate) return false;
  if (key === "buffer size") {
    const strategy = fields.find((f) => f.key === "collection strategy")?.value ?? "";
    return strategy === "rrd";
  }
  return true;
}

// Wizard-style tab groups for the per-instance edit dialog. Picked per
// collection by path. Any schema field not listed in any group falls into
// a final auto-generated "Other" tab so nothing is silently hidden.
const FILTER_FIELD_GROUPS: FieldGroup[] = [
  { name: "Source", keys: ["process", "time", "type"] },
  { name: "Filter", keys: ["filter", "warning", "critical", "ok"] },
  {
    name: "Syntax",
    keys: [
      "top syntax",
      "ok syntax",
      "detail syntax",
      "severity",
      "perf config",
      "silent period",
      "empty message",
      "maximum age",
      "escape html",
    ],
  },
  { name: "Target", keys: ["destination", "target id", "source id", "command"] },
];

const COUNTER_FIELD_GROUPS: FieldGroup[] = [
  { name: "Source", keys: ["counter", "instances", "type", "flags"] },
  { name: "Storage", keys: ["collection strategy", "buffer size"] },
];

function getFieldGroups(collectionPath: string): FieldGroup[] {
  if (/\/system\/windows\/real-time\//.test(collectionPath)) return FILTER_FIELD_GROUPS;
  if (collectionPath.includes("/system/windows/counters")) return COUNTER_FIELD_GROUPS;
  return [];
}

// One extra column shown next to the instance name in the list view so the
// user can tell instances apart without opening the dialog.
function getPreviewColumn(collectionPath: string): { key: string; label: string } | null {
  if (collectionPath.includes("/system/windows/counters")) return { key: "counter", label: "Counter" };
  if (collectionPath.includes("/system/windows/real-time/cpu")) return { key: "time", label: "Time" };
  if (collectionPath.includes("/system/windows/real-time/memory")) return { key: "type", label: "Type" };
  if (collectionPath.includes("/system/windows/real-time/process")) return { key: "process", label: "Process" };
  return null;
}

// Move `key` so it renders directly after `afterKey` (no-op if either is missing).
function reorderAfter<T extends { key: string }>(items: T[], key: string, afterKey: string): T[] {
  const fromIdx = items.findIndex((i) => i.key === key);
  const anchorIdx = items.findIndex((i) => i.key === afterKey);
  if (fromIdx < 0 || anchorIdx < 0 || fromIdx === anchorIdx + 1) return items;
  const next = items.slice();
  const [moved] = next.splice(fromIdx, 1);
  const insertAt = next.findIndex((i) => i.key === afterKey) + 1;
  next.splice(insertAt, 0, moved);
  return next;
}

export default function SettingsCollection({
  collectionPath,
  settings,
  templateName = "default",
}: Props) {
  const [confirmDelete, setConfirmDelete] = useState<Instance | null>(null);
  const [adding, setAdding] = useState<null | "regular" | "template">(null);
  const [newName, setNewName] = useState("");
  const [editingPath, setEditingPath] = useState<string | null>(null);
  const [addError, setAddError] = useState<string | null>(null);
  const [addBusy, setAddBusy] = useState(false);
  const [deleteBusy, setDeleteBusy] = useState(false);
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();

  const collectionInfo = settings.find((s) => s.path === collectionPath && s.key === "");
  const title = collectionInfo?.title || collectionPath;
  const description = collectionInfo?.description;

  const templatePath = `${collectionPath}/${templateName}`;
  const prefix = collectionPath + "/";

  // Instance names come from two sources:
  //   1. Pointer entries at the root path (key !== "" describes a known instance name).
  //   2. Direct child paths under the root.
  // Templates are now first-class instances (distinguished by their `is template`
  // value), so we no longer hide the conventionally-named template child.
  const instanceNames = useMemo(() => {
    const names = new Set<string>();
    for (const s of settings) {
      if (s.path === collectionPath && s.key !== "") names.add(s.key);
      if (s.path.startsWith(prefix)) {
        const rest = s.path.slice(prefix.length);
        if (!rest.includes("/")) names.add(rest);
      }
    }
    return [...names].sort((a, b) => a.localeCompare(b));
  }, [settings, collectionPath, prefix]);

  // Template keys come from the template child path. If absent, fall back to the first instance.
  const templateKeys = useMemo(() => {
    const fromTemplate = settings.filter(
      (s) => s.path === templatePath && s.key !== "" && !s.is_template_key,
    );
    let base: SettingsDescription[] = [];
    if (fromTemplate.length > 0) {
      base = fromTemplate;
    } else {
      for (const name of instanceNames) {
        const path = `${prefix}${name}`;
        const fromInstance = settings.filter((s) => s.path === path && s.key !== "");
        if (fromInstance.length > 0) {
          base = fromInstance;
          break;
        }
      }
    }
    return reorderAfter(base, "buffer size", "collection strategy");
  }, [settings, templatePath, instanceNames, prefix]);

  const instances: Instance[] = useMemo(() => {
    const built = instanceNames.map((name) => {
      const path = `${prefix}${name}`;
      const existing = settings.filter((s) => s.path === path && s.key !== "");
      const fields = templateKeys.map((tk) => {
        const match = existing.find((e) => e.key === tk.key);
        return match ?? { ...tk, path, value: "" };
      });
      const isTemplate =
        isTemplateValue(fields.find((f) => f.key === "is template")?.value) ||
        name === templateName;
      return { name, path, fields, isTemplate };
    });
    // Templates first, then regular objects; alphabetical within each group.
    return built.sort((a, b) => {
      if (a.isTemplate !== b.isTemplate) return a.isTemplate ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
  }, [instanceNames, prefix, settings, templateKeys, templateName]);

  // Some collections (counters, scripts, schedules) expose an `is template`
  // key in their schema; others (memory/cpu filters) don't. The Add-template
  // button and the seed key written when materializing a new instance both
  // depend on this.
  const supportsTemplates = useMemo(
    () => templateKeys.some((k) => k.key === "is template"),
    [templateKeys],
  );

  const templateOptions = useMemo(
    () => instances.filter((i) => i.isTemplate).map((i) => i.name),
    [instances],
  );

  const fieldGroups = useMemo(() => getFieldGroups(collectionPath), [collectionPath]);
  const previewColumn = useMemo(() => getPreviewColumn(collectionPath), [collectionPath]);
  const editingInstance = useMemo(
    () => (editingPath ? (instances.find((i) => i.path === editingPath) ?? null) : null),
    [editingPath, instances],
  );

  const onAdd = (mode: "regular" | "template") => {
    setNewName("");
    setAddError(null);
    setAdding(mode);
  };

  const onConfirmAdd = async () => {
    const name = newName.trim();
    if (!name) return;
    if (instanceNames.includes(name)) {
      setAddError("Name already exists");
      return;
    }
    if (name.includes("/")) {
      setAddError("Name cannot contain '/'");
      return;
    }
    const newPath = `${collectionPath}/${name}`;
    const isTemplate = adding === "template";
    setAddBusy(true);
    setAddError(null);
    try {
      // We need to write at least one key under the new path so the path
      // materializes and the instance shows up after the descriptions refetch.
      // For schemas that include `is template`, that key doubles as the
      // template/non-template marker. For schemas that don't (e.g. realtime
      // memory filters), pick the first available schema key and write its
      // current default — empty in almost all cases — which is a no-op for
      // values but still creates the path.
      let seedKey: string;
      let seedValue: string;
      if (supportsTemplates) {
        seedKey = "is template";
        seedValue = isTemplate ? "true" : "false";
      } else {
        // The backend interprets an empty value as "delete this key", so we
        // need a schema key that has a non-empty default we can echo back to
        // materialize the path without actually changing user-visible config.
        const seedField = templateKeys.find((k) => (k.default_value ?? "") !== "");
        if (!seedField) {
          throw new Error(
            "Cannot create entry: this collection's schema has no field with a default value to seed with.",
          );
        }
        seedKey = seedField.key;
        seedValue = seedField.default_value;
      }
      await updateSettings({
        path: newPath,
        key: seedKey,
        value: seedValue,
      }).unwrap();
      setAdding(null);
    } catch (err) {
      setAddError(`Failed to create entry: ${formatError(err)}`);
    } finally {
      setAddBusy(false);
    }
  };

  const onConfirmDelete = async () => {
    if (!confirmDelete) return;
    setDeleteBusy(true);
    try {
      // Walk every path under the instance and drop them deepest-first so we
      // never leave dangling children behind.
      const oldPath = confirmDelete.path;
      const subPaths = settings
        .filter((s) => s.path === oldPath || s.path.startsWith(oldPath + "/"))
        .map((s) => s.path);
      const uniqueSubPaths = [...new Set(subPaths)].sort((a, b) => b.length - a.length);
      // Fall back to deleting at least the instance path itself if nothing
      // matched (defensive: an empty instance with no descriptions yet).
      const targets = uniqueSubPaths.length > 0 ? uniqueSubPaths : [oldPath];
      for (const p of targets) {
        await deleteSettings({ path: p }).unwrap();
      }
      // Also clear the pointer entry at the parent if one exists.
      const hasPointer = settings.some(
        (s) => s.path === collectionPath && s.key === confirmDelete.name,
      );
      if (hasPointer) {
        await deleteSettings({ path: collectionPath, key: confirmDelete.name }).unwrap();
      }
      setConfirmDelete(null);
    } finally {
      setDeleteBusy(false);
    }
  };

  // Rename = walk the entire sub-tree under the old instance path, copy every
  // non-empty key to the corresponding new path, then drop the old paths.
  // Sub-paths matter for collections whose instances themselves contain
  // grouped child keys (e.g. nested template config).
  const renameInstance = async (oldPath: string, oldName: string, newName: string) => {
    const newPath = `${collectionPath}/${newName}`;

    // Collect every path under the old instance (the instance path itself plus
    // anything nested below it). Sort deepest-first so children get processed
    // before parents.
    const oldPaths = settings
      .filter((s) => s.path === oldPath || s.path.startsWith(oldPath + "/"))
      .map((s) => s.path);
    const uniqueOldPaths = [...new Set(oldPaths)].sort((a, b) => b.length - a.length);

    let copiedAnything = false;
    for (const op of uniqueOldPaths) {
      const targetPath = newPath + op.substring(oldPath.length);
      const fields = settings.filter((s) => s.path === op && s.key !== "");
      for (const f of fields) {
        const value = f.value ?? "";
        if (value === "") continue;
        await updateSettings({ path: targetPath, key: f.key, value }).unwrap();
        copiedAnything = true;
      }
    }
    // Make sure the new instance path materializes even if every field was empty.
    if (!copiedAnything) {
      await updateSettings({ path: newPath, key: "is template", value: "false" }).unwrap();
    }

    // Delete every old path (deepest-first so we don't orphan children).
    for (const op of uniqueOldPaths) {
      await deleteSettings({ path: op }).unwrap();
    }

    // Drop any pointer at the collection root that referenced the old name.
    const hasPointer = settings.some((s) => s.path === collectionPath && s.key === oldName);
    if (hasPointer) {
      await deleteSettings({ path: collectionPath, key: oldName }).unwrap();
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
        subheader={description}
        action={
          <ButtonGroup size="small" variant="outlined">
            <Button startIcon={<AddIcon />} onClick={() => onAdd("regular")}>
              Add
            </Button>
            {supportsTemplates && (
              <Button
                startIcon={<BookmarkBorderIcon />}
                onClick={() => onAdd("template")}
              >
                Add template
              </Button>
            )}
          </ButtonGroup>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {instances.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
            No entries yet. Use “Add” to create one.
          </Typography>
        ) : (
          <Table size="small">
            <TableHead>
              <TableRow>
                <TableCell>Name</TableCell>
                {previewColumn && <TableCell>{previewColumn.label}</TableCell>}
                <TableCell align="right" sx={{ width: 96 }} />
              </TableRow>
            </TableHead>
            <TableBody>
              {instances.map((inst) => {
                const previewValue = previewColumn
                  ? (inst.fields.find((f) => f.key === previewColumn.key)?.value ?? "")
                  : "";
                return (
                  <TableRow
                    key={inst.path}
                    hover
                    sx={{ cursor: "pointer" }}
                    onClick={() => setEditingPath(inst.path)}
                  >
                    <TableCell>
                      <Stack direction="row" alignItems="center" spacing={1}>
                        <Typography variant="body2">{inst.name}</Typography>
                        {inst.isTemplate && (
                          <Chip
                            size="small"
                            icon={<BookmarkBorderIcon fontSize="small" />}
                            label="Template"
                            color="secondary"
                            variant="outlined"
                          />
                        )}
                      </Stack>
                    </TableCell>
                    {previewColumn && (
                      <TableCell>
                        <Typography
                          variant="body2"
                          color={previewValue ? "text.primary" : "text.secondary"}
                          sx={{
                            fontFamily: "monospace",
                            maxWidth: 400,
                            overflow: "hidden",
                            textOverflow: "ellipsis",
                            whiteSpace: "nowrap",
                          }}
                        >
                          {previewValue || "—"}
                        </Typography>
                      </TableCell>
                    )}
                    <TableCell align="right" onClick={(e) => e.stopPropagation()}>
                      <Tooltip title="Edit">
                        <IconButton size="small" onClick={() => setEditingPath(inst.path)}>
                          <EditIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                      <Tooltip title="Delete">
                        <IconButton size="small" onClick={() => setConfirmDelete(inst)}>
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

      {editingInstance && (
        <SettingsInstanceDialog
          open={true}
          onClose={() => setEditingPath(null)}
          instanceName={editingInstance.name}
          instancePath={editingInstance.path}
          fields={editingInstance.fields}
          isTemplate={editingInstance.isTemplate}
          isDefaultTemplate={editingInstance.name === templateName}
          parentOptions={["", ...templateOptions]}
          groups={fieldGroups}
          isFieldVisible={(key) =>
            isFieldVisible(
              key,
              editingInstance.fields,
              editingInstance.isTemplate,
              editingInstance.name === templateName,
            )
          }
          existingNames={instanceNames}
          onRename={renameInstance}
        />
      )}

      <Dialog open={adding !== null} onClose={() => !addBusy && setAdding(null)}>
        <DialogTitle>{adding === "template" ? "Add template" : "Add entry"}</DialogTitle>
        <DialogContent>
          <TextField
            autoFocus
            margin="dense"
            label="Name"
            fullWidth
            variant="standard"
            value={newName}
            onChange={(e) => {
              setNewName(e.target.value);
              if (addError) setAddError(null);
            }}
            error={!!addError}
            helperText={addError ?? `Will be created at ${collectionPath}/<name>`}
            disabled={addBusy}
          />
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setAdding(null)} disabled={addBusy}>
            Cancel
          </Button>
          <Button onClick={onConfirmAdd} disabled={!newName.trim() || addBusy} loading={addBusy}>
            {adding === "template" ? "Add template" : "Add"}
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog
        open={!!confirmDelete}
        onClose={() => !deleteBusy && setConfirmDelete(null)}
      >
        <DialogTitle>Delete “{confirmDelete?.name}”?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            This will remove all settings under {confirmDelete?.path}.
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setConfirmDelete(null)} disabled={deleteBusy}>
            Cancel
          </Button>
          <Button color="error" onClick={onConfirmDelete} disabled={deleteBusy} loading={deleteBusy}>
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}

