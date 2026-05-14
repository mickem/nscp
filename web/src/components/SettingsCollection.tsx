import {
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
  Chip,
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
import AddIcon from "@mui/icons-material/Add";
import ArrowDropDownIcon from "@mui/icons-material/ArrowDropDown";
import BookmarkBorderIcon from "@mui/icons-material/BookmarkBorder";
import EditIcon from "@mui/icons-material/Edit";
import SettingsInstanceDialog, { FieldGroup } from "./SettingsInstanceDialog.tsx";
import DeleteOutlineIcon from "@mui/icons-material/DeleteOutline";
import { FormEvent, useMemo, useRef, useState } from "react";
import { getFactoryTemplates, InstanceTemplate } from "./instanceTemplates.ts";

interface Props {
  // Parent collection path, e.g. "/settings/system/windows/counters".
  collectionPath: string;
  // Full settings descriptions list (used to find instances and template keys).
  settings: SettingsDescription[];
  // Optional name of a child path that holds the template/sample (default "default").
  templateName?: string;
  // When true, schema entries flagged is_advanced_key (debug, perf config, etc.)
  // are rendered alongside the regular fields.
  showAdvanced?: boolean;
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
  showAdvanced: boolean,
  collectionPath: string,
): boolean {
  // is_template flag is set at creation time via the Add / Add template
  // buttons — never editable inline.
  if (key === "is template") return false;
  // Schema-flagged advanced keys mirror the per-module flat list: hidden by
  // default to keep the dialog focused on common config, revealed when the
  // user opts in via the Show advanced toggle.
  const field = fields.find((f) => f.key === key);
  if (field?.is_advanced_key && !showAdvanced) return false;
  // `target` is documented as an alias for `destination` — hide it so the
  // user doesn't see two fields with the title "DESTINATION".
  if (key === "target") return false;
  // Templates describe schema for child instances; they don't bind to a
  // concrete PDH counter themselves and don't carry per-instance aliases.
  if ((key === "counter" || key === "alias") && isTemplate) return false;
  // On WEB users `alias` is an advanced authentication mechanism — keep it
  // out of the way unless the user has opted into advanced fields.
  if (
    key === "alias" &&
    collectionPath === "/settings/WEB/server/users" &&
    !showAdvanced
  ) {
    return false;
  }
  // The default template is the root of the inheritance chain — it has no parent.
  if (key === "parent" && isDefaultTemplate) return false;
  if (key === "buffer size") {
    const strategy = fields.find((f) => f.key === "collection strategy")?.value ?? "";
    return strategy === "rrd";
  }
  // The "Create objects" fields only matter when auto-creation is enabled —
  // hide them when ensure objects is off so the tab isn't full of inert config.
  if (
    collectionPath.includes("/icinga/client/targets") &&
    (key === "check command" || key === "host template" || key === "service template")
  ) {
    const ensureField = fields.find((f) => f.key === "ensure objects");
    const effective = (ensureField?.value || ensureField?.default_value || "").toLowerCase();
    return effective === "true" || effective === "1";
  }
  // `debug` is rarely toggled at the per-instance level — hide it unless the
  // user has explicitly opted into advanced fields.
  if (key === "debug" && !showAdvanced) return false;
  return true;
}

// Wizard-style tab groups for the per-instance edit dialog. Picked per
// collection by path. Any schema field not listed in any group falls into
// a final auto-generated "Other" tab so nothing is silently hidden.
const FILTER_FIELD_GROUPS: FieldGroup[] = [
  { name: "Source", keys: ["process", "time", "type", "log", "file", "files", "column-split"] },
  { name: "Filter", keys: ["filter", "warning", "critical", "ok"] },
  {
    name: "Display",
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
      "debug",
    ],
  },
  { name: "Target", keys: ["destination", "target id", "source id", "command"] },
];

const COUNTER_FIELD_GROUPS: FieldGroup[] = [
  { name: "Source", keys: ["counter", "instances", "type", "flags"] },
  { name: "Storage", keys: ["collection strategy", "buffer size"] },
];

const ICINGA_TARGET_FIELD_GROUPS: FieldGroup[] = [
  {
    name: "Icinga Server",
    keys: [
      "parent",
      "address",
      "host",
      "port",
      "username",
      "password",
      "retries",
      "timeout",
    ],
  },
  { name: "TLS", keys: ["tls version", "verify mode", "ca"] },
  {
    name: "Create objects",
    keys: ["ensure objects", "check command", "host template", "service template"],
  },
  { name: "Check", keys: ["check source"] },
];

function getFieldGroups(collectionPath: string): FieldGroup[] {
  if (collectionPath.includes("/system/windows/real-time")) return FILTER_FIELD_GROUPS;
  if (collectionPath.includes("/settings/eventlog/real-time/filters")) return FILTER_FIELD_GROUPS;
  if (collectionPath.includes("/system/windows/counters")) return COUNTER_FIELD_GROUPS;
  if (collectionPath.includes("/icinga/client/targets")) return ICINGA_TARGET_FIELD_GROUPS;
  if (collectionPath.includes("/settings/logfile/real-time/checks")) return FILTER_FIELD_GROUPS;
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

// Backend can register the same key from multiple schema sources (parent
// class + subclass, alias + canonical, etc.), producing duplicate
// SettingsDescription entries at the same (path, key). When that happens,
// pick the one with the richest metadata: prefer a concrete type, then a
// title, then a description, then a non-empty default. Without this dedupe
// the first duplicate wins and the dialog can pick the version where
// `is_advanced_key=true`, `type=""`, etc., hiding bool toggles behind the
// advanced switch and rendering them as plain text.
function dedupeByKey(items: SettingsDescription[]): SettingsDescription[] {
  const score = (s: SettingsDescription) =>
    (s.type ? 8 : 0) +
    (s.title ? 4 : 0) +
    (s.description ? 2 : 0) +
    (s.default_value ? 1 : 0);
  const byKey = new Map<string, SettingsDescription>();
  for (const s of items) {
    const prev = byKey.get(s.key);
    if (!prev || score(s) > score(prev)) byKey.set(s.key, s);
  }
  // Preserve original order by re-walking the input.
  const seen = new Set<string>();
  const out: SettingsDescription[] = [];
  for (const s of items) {
    if (seen.has(s.key)) continue;
    seen.add(s.key);
    const best = byKey.get(s.key);
    if (best) out.push(best);
  }
  return out;
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
  showAdvanced = false,
}: Props) {
  const [confirmDelete, setConfirmDelete] = useState<Instance | null>(null);
  const [adding, setAdding] = useState<null | "regular" | "template">(null);
  const [newName, setNewName] = useState("");
  const [editingPath, setEditingPath] = useState<string | null>(null);
  // Currently-selected factory template (if any). When set, onConfirmAdd
  // writes its `fields` map under the new path instead of the bare seed.
  const [pendingTemplate, setPendingTemplate] = useState<InstanceTemplate | null>(null);
  const factoryTemplates = useMemo(() => getFactoryTemplates(collectionPath), [collectionPath]);
  const addMenuAnchor = useRef<HTMLDivElement | null>(null);
  const nameInputRef = useRef<HTMLInputElement | null>(null);
  const [addMenuOpen, setAddMenuOpen] = useState(false);
  const [addError, setAddError] = useState<string | null>(null);
  const [addBusy, setAddBusy] = useState(false);
  const [deleteBusy, setDeleteBusy] = useState(false);
  const [deleteError, setDeleteError] = useState<string | null>(null);
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  // Stored settings (raw key/value pairs the user has saved). We detect the
  // existence of an instance from these rather than from the descriptions
  // list, because the C++ schema registry keeps a path's schema entries
  // around after a delete — only the stored values actually disappear.
  const { data: storedSettings } = useGetSettingsQuery();

  const collectionInfo = settings.find((s) => s.path === collectionPath && s.key === "");
  const title = collectionInfo?.title || collectionPath;
  const description = collectionInfo?.description;

  const templatePath = `${collectionPath}/${templateName}`;
  const prefix = collectionPath + "/";

  // Instance names come from two sources, both keyed on *stored* settings
  // (`/v2/settings`) rather than the descriptions list. After a delete the
  // C++ schema registry may keep description entries around for the path,
  // but the stored values genuinely disappear — so this is the reliable
  // source of "what instances actually exist right now":
  //   1. Pointer entries at the root path (key !== "" names a known child).
  //   2. Direct child paths under the root that have at least one stored key.
  // We always also surface the conventional `templateName` ("default") even
  // if it has no stored values, since it represents the schema-default
  // template that everything else inherits from.
  const instanceNames = useMemo(() => {
    const names = new Set<string>();
    const pathsWithKeys = new Set<string>();
    const stored = storedSettings ?? [];
    for (const s of stored) {
      if (s.path === collectionPath && s.key !== "") names.add(s.key);
      if (s.path.startsWith(prefix) && s.key !== "") pathsWithKeys.add(s.path);
    }
    for (const p of pathsWithKeys) {
      const rest = p.slice(prefix.length);
      if (!rest.includes("/")) names.add(rest);
    }
    // The schema-default template is always editable even when no values
    // have been stored under it.
    if (settings.some((s) => s.path === templatePath)) names.add(templateName);
    return [...names].sort((a, b) => a.localeCompare(b));
  }, [storedSettings, settings, templatePath, templateName, collectionPath, prefix]);

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
    return reorderAfter(dedupeByKey(base), "buffer size", "collection strategy");
  }, [settings, templatePath, instanceNames, prefix]);

  const instances: Instance[] = useMemo(() => {
    const built = instanceNames.map((name) => {
      const path = `${prefix}${name}`;
      const existing = dedupeByKey(
        settings.filter((s) => s.path === path && s.key !== ""),
      );
      // Always source field metadata (type, title, description, flags,
      // default_value) from the template — instance description entries only
      // get fleshed out by the backend after a save/reload, so a freshly
      // added instance can otherwise lose its toggle/enum widgets and have
      // is_advanced_key flip to true. Overlay just the stored value.
      const fields = templateKeys.map((tk) => {
        const match = existing.find((e) => e.key === tk.key);
        return { ...tk, path, value: match?.value ?? "" };
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

  const onAdd = (mode: "regular" | "template", template?: InstanceTemplate) => {
    // For template-driven adds, suggest the template's default name and avoid
    // collisions with existing instances by appending an incrementing suffix.
    let suggestion = template?.defaultName ?? "";
    if (suggestion && instanceNames.includes(suggestion)) {
      let i = 2;
      while (instanceNames.includes(`${suggestion}_${i}`)) i++;
      suggestion = `${suggestion}_${i}`;
    }
    setNewName(suggestion);
    setPendingTemplate(template ?? null);
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
      if (pendingTemplate) {
        // Factory template: write every non-empty field under the new path.
        let wroteSomething = false;
        for (const [key, value] of Object.entries(pendingTemplate.fields)) {
          if (value === "") continue;
          await updateSettings({ path: newPath, key, value }).unwrap();
          wroteSomething = true;
        }
        // Defensive — shouldn't happen unless every field in the template was
        // empty, but keep the create path materialized either way.
        if (!wroteSomething && supportsTemplates) {
          await updateSettings({ path: newPath, key: "is template", value: "false" }).unwrap();
        }
      } else {
        // Plain Add / Add template (no factory recipe). Write at least one key
        // under the new path so the path materializes and the instance shows
        // up after the descriptions refetch. For schemas that include
        // `is template`, that key doubles as the template/non-template
        // marker. For schemas that don't (e.g. realtime memory filters), pick
        // the first available schema key and write its current default —
        // empty in almost all cases — which is a no-op for values but still
        // creates the path.
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
      }
      setAdding(null);
      setPendingTemplate(null);
      // Jump straight into the edit dialog so the user can configure the
      // new instance — a seeded entry on its own is rarely useful. The
      // dialog actually mounts once the settings query refetches and the
      // new path appears in `instances`.
      setEditingPath(newPath);
    } catch (err) {
      setAddError(`Failed to create entry: ${formatError(err)}`);
    } finally {
      setAddBusy(false);
    }
  };

  const onConfirmDelete = async () => {
    if (!confirmDelete) return;
    setDeleteBusy(true);
    setDeleteError(null);
    try {
      const oldPath = confirmDelete.path;
      // Gather every path under the instance (the path itself + any nested
      // children) so we never leave dangling sub-config behind. Deepest-first
      // so children get processed before their parents.
      const subPaths = settings
        .filter((s) => s.path === oldPath || s.path.startsWith(oldPath + "/"))
        .map((s) => s.path);
      const uniqueSubPaths = [...new Set(subPaths)].sort((a, b) => b.length - a.length);
      const targets = uniqueSubPaths.length > 0 ? uniqueSubPaths : [oldPath];

      for (const p of targets) {
        // The backend handles cascading deletes when given a bare path.
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
    } catch (err) {
      setDeleteError(`Failed to delete: ${formatError(err)}`);
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
          <Stack direction="row" spacing={1}>
            <ButtonGroup size="small" variant="outlined" ref={addMenuAnchor}>
              <Button startIcon={<AddIcon />} onClick={() => onAdd("regular")}>
                Add
              </Button>
              {factoryTemplates.length > 0 && (
                <Button
                  size="small"
                  aria-label="add from template"
                  onClick={() => setAddMenuOpen((o) => !o)}
                >
                  <ArrowDropDownIcon />
                </Button>
              )}
            </ButtonGroup>
            {supportsTemplates && (
              <Button
                size="small"
                variant="outlined"
                startIcon={<BookmarkBorderIcon />}
                onClick={() => onAdd("template")}
              >
                Add template
              </Button>
            )}
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
                            onAdd("regular");
                          }}
                        >
                          <ListItemText primary="Empty entry" />
                        </MenuItem>
                        {factoryTemplates.map((t) => (
                          <MenuItem
                            key={t.name}
                            onClick={() => {
                              setAddMenuOpen(false);
                              onAdd("regular", t);
                            }}
                          >
                            <ListItemText primary={t.name} secondary={t.description} />
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
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        {instances.length === 0 ? (
          <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
            No entries yet. Use “Add” to create one.
          </Typography>
        ) : (
          <Table size="small" sx={{ tableLayout: "fixed" }}>
            <TableHead>
              <TableRow>
                <TableCell sx={{ width: 280 }}>Name</TableCell>
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
              showAdvanced,
              collectionPath,
            )
          }
          existingNames={instanceNames}
          onRename={renameInstance}
        />
      )}

      <Dialog
        open={adding !== null}
        onClose={() => {
          if (addBusy) return;
          setAdding(null);
          setPendingTemplate(null);
        }}
        slotProps={{
          paper: {
            component: "form",
            onSubmit: (e: FormEvent) => {
              e.preventDefault();
              if (!newName.trim() || addBusy) return;
              void onConfirmAdd();
            },
          },
          // autoFocus on TextField can lose the race with the Add-menu's
          // focus restore when the dialog opens immediately on menu click —
          // grab focus explicitly once the dialog transition is done.
          transition: {
            onEntered: () => {
              nameInputRef.current?.focus();
              nameInputRef.current?.select();
            },
          },
        }}
      >
        <DialogTitle>
          {pendingTemplate
            ? `Add: ${pendingTemplate.name}`
            : adding === "template"
              ? "Add template"
              : "Add entry"}
        </DialogTitle>
        <DialogContent>
          <TextField
            inputRef={nameInputRef}
            // Select any suggested name so the user can start typing to
            // replace it rather than having to clear it first.
            onFocus={(e) => (e.target as HTMLInputElement).select()}
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
          <Button
            type="button"
            onClick={() => {
              setAdding(null);
              setPendingTemplate(null);
            }}
            disabled={addBusy}
          >
            Cancel
          </Button>
          <Button
            type="submit"
            variant="contained"
            disabled={!newName.trim() || addBusy}
            loading={addBusy}
          >
            {adding === "template" ? "Add template" : "Add"}
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog
        open={!!confirmDelete}
        onClose={() => {
          if (deleteBusy) return;
          setConfirmDelete(null);
          setDeleteError(null);
        }}
      >
        <DialogTitle>Delete “{confirmDelete?.name}”?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            This will remove all settings under {confirmDelete?.path}.
          </Typography>
          {deleteError && (
            <Typography variant="body2" color="error" sx={{ mt: 1 }}>
              {deleteError}
            </Typography>
          )}
        </DialogContent>
        <DialogActions>
          <Button
            onClick={() => {
              setConfirmDelete(null);
              setDeleteError(null);
            }}
            disabled={deleteBusy}
          >
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

