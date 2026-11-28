// The scheduler's schedules: each a check run on an interval or a cron line,
// its result submitted on a channel. `default` holds what every schedule that
// does not set its own takes, which is how one-line schedules get their
// interval.
import {
  Autocomplete,
  Button,
  Card,
  CardContent,
  CardHeader,
  Checkbox,
  Chip,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Divider,
  FormControlLabel,
  FormGroup,
  IconButton,
  Radio,
  RadioGroup,
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
import {
  Settings,
  useDeleteSettingsMutation,
  useGetChannelMetadataQuery,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../../api/api.ts";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import CommandLineField from "../kit/CommandLineField.tsx";
import { deleteEntry, Entry, entryWrites, Write } from "../kit/entries.ts";
import { AdvancedSection } from "../kit/PanelShell.tsx";
import ValueField from "../kit/ValueField.tsx";
import {
  COMMON_CHANNELS,
  DEFAULT_SCHEDULE,
  describeWhen,
  FUZZY,
  isFuzzy,
  isTemplate,
  readSchedules,
  reportSet,
  reportValue,
  REPORTS,
  SCHEDULE_KEYS,
  scheduleProblem,
  SCHEDULES,
  scheduleValue,
} from "./scheduler.ts";

interface Editing {
  original: Entry | null;
  name: string;
  values: Record<string, string>;
}

const defaultEntry = (stored: Settings[] | undefined): Entry => {
  const values: Record<string, string> = {};
  for (const s of stored ?? [])
    if (s.path === `${SCHEDULES}/${DEFAULT_SCHEDULE}` && s.key !== "") values[s.key] = s.value;
  return { name: DEFAULT_SCHEDULE, form: "section", values };
};

export default function SchedulesEditor() {
  const { data: stored } = useGetSettingsQuery();
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  const [editing, setEditing] = useState<Editing | null>(null);
  const [deleting, setDeleting] = useState<Entry | null>(null);
  const [busy, setBusy] = useState(false);
  const [renaming, setRenaming] = useState(false);
  const { data: liveChannels } = useGetChannelMetadataQuery();
  const channelOptions = [
    ...new Set([...COMMON_CHANNELS, ...(liveChannels ?? []).map((c) => c.value)]),
  ];
  const [error, setError] = useState<string | null>(null);
  const schedules = readSchedules(stored);
  const rows = [defaultEntry(stored), ...schedules];

  const run = async (writes: Write[], done: () => void) => {
    setBusy(true);
    setError(null);
    try {
      for (const w of writes) {
        if (w.kind === "put")
          await updateSettings({ path: w.path, key: w.key ?? "", value: w.value ?? "" }).unwrap();
        else if (w.kind === "delete") await deleteSettings({ path: w.path, key: w.key }).unwrap();
        else await deleteSettings({ path: w.path }).unwrap();
      }
      done();
    } catch (e) {
      setError(e instanceof Error ? e.message : JSON.stringify(e));
    } finally {
      setBusy(false);
    }
  };

  const isDefault =
    editing?.original?.name === DEFAULT_SCHEDULE || (editing ? isTemplate(editing.values) : false);
  const name =
    editing?.original?.name === DEFAULT_SCHEDULE ? DEFAULT_SCHEDULE : (editing?.name.trim() ?? "");
  const own = (k: string) => editing?.values[k] ?? "";
  const get = (k: string) => (editing ? scheduleValue(stored, name, k, editing.values) : "");
  const set = (k: string, v: string) =>
    editing && setEditing({ ...editing, values: { ...editing.values, [k]: v } });
  const when = get("schedule") ? "cron" : "interval";
  const problem = !editing
    ? null
    : !name
      ? "Give the schedule a name."
      : /[\s/=]/.test(name)
        ? "A schedule name cannot hold spaces, '/' or '='."
        : !isDefault && name !== editing.original?.name && schedules.some((s) => s.name === name)
          ? "A schedule with that name exists."
          : scheduleProblem(get, isDefault);
  const reports = reportSet(get("report"));
  // What the schedule would have without a randomness of its own.
  const inheritedRandomness = editing
    ? scheduleValue(stored, name, "randomness", { ...editing.values, randomness: "" })
    : "";

  const writes = (): Write[] => {
    if (!editing) return [];
    if (editing.original?.name === DEFAULT_SCHEDULE) {
      const path = `${SCHEDULES}/${DEFAULT_SCHEDULE}`;
      return SCHEDULE_KEYS.flatMap((k): Write[] => {
        const before = editing.original?.values[k];
        const v = editing.values[k] ?? "";
        if (v !== "" && v !== before) return [{ kind: "put", path, key: k, value: v }];
        if (v === "" && before !== undefined) return [{ kind: "delete", path, key: k }];
        return [];
      });
    }
    return entryWrites(SCHEDULES, SCHEDULE_KEYS, "command", editing.original, name, editing.values);
  };

  return (
    <Card>
      <CardHeader
        title={<Typography variant="h6">Schedules</Typography>}
        subheader="Checks run on a schedule; each result is submitted on a channel, to the client module listening there."
        action={
          <Button
            startIcon={<AddIcon />}
            onClick={() => {
              setError(null);
              setEditing({ original: null, name: "", values: {} });
              setRenaming(false);
            }}
          >
            Add schedule
          </Button>
        }
      />
      <CardContent sx={{ p: 0, "&:last-child": { pb: 0 } }}>
        <Table size="small" aria-label="Schedules">
          <TableHead>
            <TableRow>
              <TableCell>Schedule</TableCell>
              <TableCell>Runs</TableCell>
              <TableCell>When</TableCell>
              <TableCell>Submits to</TableCell>
              <TableCell />
            </TableRow>
          </TableHead>
          <TableBody>
            {rows.map((s) => {
              const v = (k: string) => scheduleValue(stored, s.name, k, s.values);
              const d = s.name === DEFAULT_SCHEDULE;
              const template = d || isTemplate(s.values);
              const refused = template ? null : scheduleProblem(v, false);
              return (
                <TableRow key={s.name} hover>
                  <TableCell>
                    <Typography variant="body2" sx={{ fontFamily: "monospace" }}>
                      {s.name}
                    </Typography>
                    {d ? (
                      <Typography variant="caption" color="text.secondary">
                        What every schedule takes unless it sets its own
                      </Typography>
                    ) : (
                      template && (
                        <Typography variant="caption" color="text.secondary">
                          Template: not run, other schedules take from it
                        </Typography>
                      )
                    )}
                  </TableCell>
                  <TableCell>
                    <Typography
                      variant="body2"
                      sx={{ fontFamily: "monospace", wordBreak: "break-all" }}
                    >
                      {v("command") || (d ? "" : "-")}
                    </Typography>
                  </TableCell>
                  <TableCell>
                    {refused ? (
                      <Tooltip title={refused} describeChild>
                        <Chip
                          size="small"
                          color="error"
                          variant="outlined"
                          label="refused by the agent"
                        />
                      </Tooltip>
                    ) : (
                      <Typography variant="body2">{describeWhen(v)}</Typography>
                    )}
                  </TableCell>
                  <TableCell>
                    <Typography variant="body2">
                      {v("channel")}
                      {v("target") ? ` (${v("target")})` : ""}
                    </Typography>
                  </TableCell>
                  <TableCell align="right" sx={{ whiteSpace: "nowrap" }}>
                    <Tooltip title="Edit">
                      <IconButton
                        size="small"
                        onClick={() => {
                          setError(null);
                          setEditing({ original: s, name: s.name, values: { ...s.values } });
                          setRenaming((s.values.alias ?? "") !== "");
                        }}
                      >
                        <EditIcon fontSize="small" />
                      </IconButton>
                    </Tooltip>
                    {!d && (
                      <Tooltip title="Delete">
                        <IconButton size="small" onClick={() => setDeleting(s)}>
                          <DeleteOutlineIcon fontSize="small" />
                        </IconButton>
                      </Tooltip>
                    )}
                  </TableCell>
                </TableRow>
              );
            })}
          </TableBody>
        </Table>
      </CardContent>

      <Dialog
        open={editing !== null}
        onClose={() => !busy && setEditing(null)}
        maxWidth="md"
        fullWidth
      >
        <DialogTitle>
          {editing?.original?.name === DEFAULT_SCHEDULE
            ? "Default schedule"
            : editing?.original
              ? `Schedule ${editing.original.name}`
              : "Add schedule"}
        </DialogTitle>
        <DialogContent>
          {editing && (
            <Stack spacing={3} divider={<Divider flexItem />} sx={{ pt: 1 }}>
              {editing.original?.name === DEFAULT_SCHEDULE ? (
                <Typography variant="body2" color="text.secondary">
                  Every schedule takes what it does not set from here: a one-line schedule (name =
                  command) runs only if an interval or a schedule is set here.
                </Typography>
              ) : (
                <TextField
                  variant="standard"
                  label="Name"
                  value={editing.name}
                  onChange={(e) => setEditing({ ...editing, name: e.target.value })}
                  helperText="The service the result is submitted as, unless it is renamed below."
                  autoFocus={!editing.original}
                />
              )}
              {!isDefault && (
                <Stack spacing={2}>
                  <Typography variant="subtitle1">Check</Typography>
                  <CommandLineField
                    value={own("command")}
                    onChange={(v) => set("command", v)}
                    disabled={busy}
                  />
                  <ValueField
                    label="Rename"
                    kind="bool"
                    value={renaming ? "true" : "false"}
                    onChange={(v) => {
                      setRenaming(v === "true");
                      if (v !== "true") set("alias", "");
                    }}
                    help="Submit the result under another service name than the schedule's."
                  />
                  {renaming && (
                    <ValueField
                      label="Service name"
                      kind="text"
                      value={own("alias")}
                      onChange={(v) => set("alias", v)}
                    />
                  )}
                </Stack>
              )}
              <Stack spacing={2}>
                <Typography variant="subtitle1">When</Typography>
                <RadioGroup
                  row
                  value={when}
                  onChange={(e) =>
                    setEditing({
                      ...editing,
                      values: {
                        ...editing.values,
                        // Only one of them may be in effect.
                        interval: e.target.value === "cron" ? "" : own("interval") || "5m",
                        schedule: e.target.value === "cron" ? own("schedule") || "0 * * * *" : "",
                      },
                    })
                  }
                >
                  <FormControlLabel
                    value="interval"
                    control={<Radio size="small" />}
                    label="Every"
                  />
                  <FormControlLabel
                    value="cron"
                    control={<Radio size="small" />}
                    label="At set times (cron)"
                  />
                </RadioGroup>
                {when === "interval" ? (
                  <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
                    <ValueField
                      label="Interval"
                      kind="text"
                      value={get("interval")}
                      onChange={(v) => set("interval", v)}
                      help="30s, 5m, 1h, 1d or 1w."
                    />
                    <ValueField
                      label="Fuzzy"
                      kind="bool"
                      value={isFuzzy(get("randomness")) ? "true" : "false"}
                      onChange={(v) =>
                        set(
                          "randomness",
                          v === "true" ? FUZZY : isFuzzy(inheritedRandomness) ? "0" : "",
                        )
                      }
                      help="Spread the runs a little (±20% of the interval), so checks do not all fire at once."
                    />
                  </Stack>
                ) : (
                  <ValueField
                    label="Schedule"
                    kind="text"
                    value={get("schedule")}
                    onChange={(v) => set("schedule", v)}
                    help="minute hour day-of-month month day-of-week, each * or numbers separated by commas: 0,30 * * * 1,2,3,4,5"
                  />
                )}
                {when === "interval" && (
                  <AdvancedSection count={own("randomness") !== "" ? 1 : 0}>
                    <ValueField
                      label="Randomness (%)"
                      kind="text"
                      value={get("randomness")}
                      onChange={(v) => set("randomness", v)}
                      help="Spreads each run by up to this share of the interval; Fuzzy sets 20%."
                    />
                  </AdvancedSection>
                )}
                <ValueField
                  label="Also run at startup"
                  kind="bool"
                  value={get("run on startup")}
                  onChange={(v) => set("run on startup", v)}
                />
              </Stack>
              <Stack spacing={2}>
                <Typography variant="subtitle1">Result</Typography>
                <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
                  <Autocomplete
                    freeSolo
                    fullWidth
                    options={channelOptions}
                    value={get("channel")}
                    onInputChange={(_, v, reason) => {
                      if (reason !== "reset" || v !== get("channel")) set("channel", v);
                    }}
                    renderInput={(params) => (
                      <TextField
                        {...params}
                        variant="standard"
                        label="Channel"
                        helperText="The client module listening on it submits the result; drop discards it."
                      />
                    )}
                  />
                  <ValueField
                    label="Target"
                    kind="text"
                    value={get("target")}
                    onChange={(v) => set("target", v)}
                    help="Which of that client's targets; its default when empty."
                  />
                </Stack>
                <FormGroup row aria-label="Submit results that are">
                  <Typography variant="body2" sx={{ mr: 2, alignSelf: "center" }}>
                    Submit results that are
                  </Typography>
                  {REPORTS.map((r) => (
                    <FormControlLabel
                      key={r}
                      control={
                        <Checkbox
                          size="small"
                          checked={reports.has(r)}
                          onChange={(e) => {
                            const next = new Set(reports);
                            if (e.target.checked) next.add(r);
                            else next.delete(r);
                            set("report", reportValue(next));
                          }}
                        />
                      }
                      label={r.toUpperCase()}
                    />
                  ))}
                </FormGroup>
              </Stack>
              {problem && (editing.name !== "" || isDefault) && (
                <NscpAlert severity="warning" text={problem} />
              )}
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
            disabled={busy || !!problem || writes().length === 0}
            loading={busy}
            onClick={() => void run(writes(), () => setEditing(null))}
          >
            Save
          </Button>
        </DialogActions>
      </Dialog>

      <Dialog open={deleting !== null} onClose={() => !busy && setDeleting(null)}>
        <DialogTitle>Delete schedule {deleting?.name}?</DialogTitle>
        <DialogContent>
          <Typography variant="body2">
            The monitoring server stops receiving its result, and the service goes stale there.
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
              deleting && void run([deleteEntry(SCHEDULES, deleting)], () => setDeleting(null))
            }
          >
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </Card>
  );
}
