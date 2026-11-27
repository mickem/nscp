// The targets of a client module, as one card: each target with where it
// points, how it is protected and what weakens it, a probe where the protocol
// has a harmless one, and a dialog that edits a target by section.
import {
  Alert,
  Box,
  Button,
  Card,
  CardContent,
  CardHeader,
  Chip,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Divider,
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
import {
  Settings,
  useDeleteSettingsMutation,
  useExecuteQueryMutation,
  useGetSettingsQuery,
} from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import {
  applyToStored,
  describeChange,
  enableModuleChange,
  pendingChanges,
  readStored,
  useApplyChanges,
} from "./changes.ts";
import ConfigDialog from "./ConfigDialog.tsx";
import ModeSelector from "./ModeSelector.tsx";
import { CUSTOM } from "./modes.ts";
import { AdvancedSection } from "./PanelShell.tsx";
import RiskBanner, { Risk } from "./RiskBanner.tsx";
import SecretField from "./SecretField.tsx";
import {
  advancedKeys,
  DEFAULT_TARGET,
  endpoint,
  inherited,
  TargetMode,
  targetChanges,
  targetIni,
  targetNames,
  targetPath,
  TargetSpec,
  targetValues,
} from "./targets.ts";
import ValueField from "./ValueField.tsx";

interface Props {
  spec: TargetSpec;
  onExpertMode?: () => void;
}

function worst(risks: Risk[]): Risk | undefined {
  return risks.find((r) => r.severity === "error") ?? risks[0];
}

export default function TargetsEditor({ spec, onExpertMode }: Props) {
  const { data: stored } = useGetSettingsQuery();
  const [deleteSettings] = useDeleteSettingsMutation();
  const [editing, setEditing] = useState<{ name: string; isNew: boolean } | null>(null);
  const [deleting, setDeleting] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const names = useMemo(() => targetNames(stored, spec), [stored, spec]);

  const remove = async () => {
    if (!deleting) return;
    setError(null);
    try {
      await deleteSettings({ path: targetPath(spec, deleting) }).unwrap();
      if (readStored(stored, spec.path, deleting) !== undefined)
        await deleteSettings({ path: spec.path, key: deleting }).unwrap();
      setDeleting(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">Targets</Typography>}
        subheader={`The ${spec.noun}s this module talks to. A query or submission picks one with target=; without one it uses default.`}
        action={
          <Button startIcon={<AddIcon />} onClick={() => setEditing({ name: "", isNew: true })}>
            Add target
          </Button>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        <Table size="small" aria-label="Targets">
          <TableHead>
            <TableRow>
              <TableCell>Target</TableCell>
              <TableCell>Address</TableCell>
              <TableCell>Security</TableCell>
              <TableCell />
            </TableRow>
          </TableHead>
          <TableBody>
            {names.map((name) => (
              <TargetRow
                key={name}
                spec={spec}
                stored={stored}
                name={name}
                onEdit={() => setEditing({ name, isNew: false })}
                onDelete={() => setDeleting(name)}
              />
            ))}
          </TableBody>
        </Table>
      </CardContent>

      {editing && (
        <TargetDialog
          spec={spec}
          stored={stored}
          existing={names}
          name={editing.name}
          isNew={editing.isNew}
          onClose={() => setEditing(null)}
          onExpertMode={onExpertMode}
        />
      )}

      <Dialog open={deleting !== null} onClose={() => setDeleting(null)}>
        <DialogTitle>Delete target {deleting}?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            Checks and submissions naming it with target= will fail until it is added again.
          </Typography>
          {error && <NscpAlert severity="error" text={error} />}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setDeleting(null)}>Cancel</Button>
          <Button color="error" variant="contained" onClick={() => void remove()}>
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}

function TargetRow({
  spec,
  stored,
  name,
  onEdit,
  onDelete,
}: {
  spec: TargetSpec;
  stored: Settings[] | undefined;
  name: string;
  onEdit: () => void;
  onDelete: () => void;
}) {
  const [executeQuery, { isLoading }] = useExecuteQueryMutation();
  const [probe, setProbe] = useState<{ ok: boolean; text: string } | null>(null);
  const v = targetValues(stored, spec, name);
  const risk = worst(spec.risks(v));
  const isDefault = name === DEFAULT_TARGET;
  const address = endpoint(v);

  const test = async () => {
    if (!spec.test) return;
    setProbe(null);
    try {
      const r = await executeQuery({
        query: spec.test.query,
        args: spec.test.args(name),
      }).unwrap();
      const text = r.lines.map((l) => l.message).join(" ") || "(no output)";
      setProbe({ ok: r.result === 0, text });
    } catch (e) {
      setProbe({ ok: false, text: e instanceof Error ? e.message : String(e) });
    }
  };

  return (
    <>
      <TableRow hover>
        <TableCell>
          <Typography variant="body2" sx={{ fontFamily: "monospace" }}>
            {name}
          </Typography>
          {isDefault && (
            <Typography variant="caption" color="text.secondary">
              Used when a request names none
            </Typography>
          )}
        </TableCell>
        <TableCell>
          <Typography variant="body2" color={address ? undefined : "text.secondary"}>
            {address || "not set"}
          </Typography>
        </TableCell>
        <TableCell>
          <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
            <Typography variant="body2">{spec.security(v)}</Typography>
            {risk && (
              <Chip size="small" variant="outlined" color={risk.severity} label={risk.title} />
            )}
          </Stack>
        </TableCell>
        <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
          {spec.test && address && (
            <Tooltip title={spec.test.what} describeChild>
              <Button size="small" onClick={() => void test()} loading={isLoading}>
                Test
              </Button>
            </Tooltip>
          )}
          <Tooltip title="Edit">
            <IconButton size="small" onClick={onEdit}>
              <EditIcon fontSize="small" />
            </IconButton>
          </Tooltip>
          {!isDefault && (
            <Tooltip title="Delete">
              <IconButton size="small" onClick={onDelete}>
                <DeleteOutlineIcon fontSize="small" />
              </IconButton>
            </Tooltip>
          )}
        </TableCell>
      </TableRow>
      {probe && (
        <TableRow>
          <TableCell colSpan={4}>
            <Alert severity={probe.ok ? "success" : "error"} onClose={() => setProbe(null)}>
              {probe.text}
            </Alert>
          </TableCell>
        </TableRow>
      )}
    </>
  );
}

function modeKeys(mode: TargetMode, v: (k: string) => string): string[] {
  return [...new Set(mode.options.flatMap((o) => Object.keys(mode.apply(o.value, v))))];
}

function TargetDialog({
  spec,
  stored,
  existing,
  name: initialName,
  isNew,
  onClose,
  onExpertMode,
}: {
  spec: TargetSpec;
  stored: Settings[] | undefined;
  existing: string[];
  name: string;
  isNew: boolean;
  onClose: () => void;
  onExpertMode?: () => void;
}) {
  const applyChanges = useApplyChanges();
  const [name, setName] = useState(initialName);
  const [draft, setDraft] = useState<Record<string, string>>({});
  // Modes the reader picked; a mode the values match by themselves needs none.
  const [modes, setModes] = useState<Record<string, string>>({});
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [showConfig, setShowConfig] = useState(false);

  const target = name.trim();
  // A new target inherits from default until it is saved under its name.
  const readAs = isNew ? `\u0000new` : initialName;
  const storedValues = targetValues(stored, spec, readAs);
  const v = targetValues(stored, spec, readAs, draft);
  const set = (key: string, value: string) => setDraft((d) => ({ ...d, [key]: value }));

  const changes = useMemo(() => {
    if (!target) return [];
    const own = targetChanges(stored, spec, target, draft);
    // A new target is written even when it only takes the defaults.
    if (isNew && own.length > 0) own[0] = { ...own[0], pin: true };
    if (isNew && own.length === 0)
      own.push({ path: spec.path, key: target, value: target, pin: true });
    const out = pendingChanges(stored, own);
    return out.length > 0
      ? pendingChanges(stored, [...out, enableModuleChange(spec.moduleId)])
      : out;
  }, [stored, spec, target, draft, isNew]);

  const nameProblem = !isNew
    ? null
    : !target
      ? "Give the target a name."
      : target.includes("/") || target.includes("=")
        ? "A target name cannot hold '/' or '='."
        : existing.includes(target)
          ? "A target with that name exists."
          : null;
  const problem = nameProblem ?? spec.problem?.(v) ?? null;
  const risks = spec.risks(v);

  const save = async () => {
    setBusy(true);
    setError(null);
    try {
      await applyChanges(changes);
      onClose();
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const field = (key: string) => {
    const f = spec.fields[key];
    if (!f || (f.visible && !f.visible(v))) return null;
    const own = readStored(stored, targetPath(spec, readAs), key);
    const from = initialName === DEFAULT_TARGET ? undefined : inherited(stored, spec, readAs, key);
    const help = (
      <>
        {f.help}
        {own === undefined && draft[key] === undefined && from !== undefined && from !== f.builtIn
          ? " Taken from the default target."
          : ""}
        {initialName === DEFAULT_TARGET
          ? f.inherits
            ? " Inherited by other targets."
            : " Not inherited: other targets use the built-in default unless they set it."
          : ""}
      </>
    );
    if (f.kind === "secret")
      return (
        <SecretField
          key={key}
          label={f.label}
          help={help}
          current={storedValues(key)}
          value={v(key)}
          onChange={(x) => set(key, x)}
          disabled={busy}
        />
      );
    return (
      <ValueField
        key={key}
        label={f.label}
        kind={typeof f.kind === "function" ? f.kind(v) : f.kind}
        help={help}
        value={v(key)}
        onChange={(x) => set(key, x)}
        disabled={busy}
      />
    );
  };

  const modeSelector = (title: string, mode: TargetMode) => {
    const storedMode = mode.detect(storedValues);
    const current = modes[title] ?? mode.detect(v) ?? CUSTOM;
    return (
      <ModeSelector
        label={mode.label}
        options={mode.options}
        value={current}
        storedIsCustom={storedMode === null}
        onExpertMode={onExpertMode}
        disabled={busy}
        onChange={(m) => {
          setModes((s) => ({ ...s, [title]: m }));
          if (m === CUSTOM) {
            // Back to the stored keys.
            const keys = modeKeys(mode, storedValues);
            setDraft((d) =>
              Object.fromEntries(Object.entries(d).filter(([k]) => !keys.includes(k))),
            );
          } else {
            setDraft((d) => ({ ...d, ...mode.apply(m, (k) => d[k] ?? storedValues(k)) }));
          }
        }}
      />
    );
  };

  const advanced = advancedKeys(spec).filter((k) => {
    const f = spec.fields[k];
    return !f.visible || f.visible(v);
  });
  const advancedSet = advanced.filter(
    (k) => readStored(stored, targetPath(spec, readAs), k) !== undefined,
  ).length;
  const title = isNew
    ? `Add ${spec.noun}`
    : initialName === DEFAULT_TARGET
      ? "Default target"
      : `Target ${initialName}`;

  return (
    <Dialog open onClose={() => !busy && onClose()} maxWidth="md" fullWidth>
      <DialogTitle>{title}</DialogTitle>
      <DialogContent>
        <Stack spacing={3} divider={<Divider flexItem />} sx={{ pt: 1 }}>
          {(isNew || initialName === DEFAULT_TARGET) && (
            <Stack spacing={2}>
              {isNew ? (
                <TextField
                  label="Name"
                  value={name}
                  onChange={(e) => setName(e.target.value)}
                  variant="standard"
                  autoFocus
                  helperText="What queries and submissions select it by, with target=."
                />
              ) : (
                <Typography variant="body2" color="text.secondary">
                  Used when a request names no target, or one that does not exist. Other targets
                  take only the settings marked as inherited from here; the rest they set themselves
                  or leave at the built-in default.
                </Typography>
              )}
            </Stack>
          )}
          {spec.sections.map((s) => (
            <Stack key={s.title} spacing={2}>
              <Typography variant="subtitle1">{s.title}</Typography>
              {s.note}
              {s.mode && modeSelector(s.title, s.mode)}
              {s.keys.map(field)}
            </Stack>
          ))}
          {advanced.length > 0 && (
            <AdvancedSection count={advancedSet}>{advanced.map(field)}</AdvancedSection>
          )}
          <Stack spacing={1.5}>
            {risks.map((r) => (
              <RiskBanner key={r.title} risk={r} />
            ))}
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
                No changes.
              </Typography>
            )}
            {error && <NscpAlert severity="error" text={error} />}
          </Stack>
        </Stack>
      </DialogContent>
      <DialogActions>
        <Button onClick={() => setShowConfig(true)} disabled={!target}>
          Show config
        </Button>
        <Box sx={{ flexGrow: 1 }} />
        <Button onClick={onClose} disabled={busy}>
          Cancel
        </Button>
        <Button
          variant="contained"
          onClick={() => void save()}
          disabled={busy || changes.length === 0 || !!problem}
          loading={busy}
        >
          Save
        </Button>
      </DialogActions>
      <ConfigDialog
        open={showConfig}
        onClose={() => setShowConfig(false)}
        title={`Target ${target} configuration`}
        note={
          changes.length > 0
            ? "Includes the changes not saved yet. Paste it into nsclient.ini on another agent, or into a fleet bundle."
            : "Paste it into nsclient.ini on another agent, or into a fleet bundle."
        }
        render={(includeDefaults) =>
          targetIni(applyToStored(stored, changes), spec, target, includeDefaults)
        }
      />
    </Dialog>
  );
}

