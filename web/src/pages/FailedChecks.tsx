import Stack from "@mui/material/Stack";
import {
  Alert,
  Button,
  Chip,
  Collapse,
  FormControl,
  IconButton,
  InputLabel,
  MenuItem,
  Select,
  SelectChangeEvent,
  Snackbar,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from "@mui/material";
import DeleteSweepIcon from "@mui/icons-material/DeleteSweep";
import KeyboardArrowDownIcon from "@mui/icons-material/KeyboardArrowDown";
import KeyboardArrowRightIcon from "@mui/icons-material/KeyboardArrowRight";
import { useMemo, useState } from "react";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import { QueryResultChip } from "../components/atoms/QueryResultChip.tsx";
import { useAppDispatch } from "../store/store.ts";
import {
  FailedCheck,
  FailedCheckContextState,
  QueryExecutionResultLinePerf,
  nsclientApi,
  useClearFailuresMutation,
  useGetFailureContextQuery,
  useGetFailuresQuery,
} from "../api/api.ts";
import FilterField from "../components/atoms/FilterField.tsx";
import Trail from "../components/atoms/Trail.tsx";
import { formatAge } from "../common/formatAge.ts";

const REFRESH_RATES = [
  { label: "Off", value: 0 },
  { label: "1 second", value: 1000 },
  { label: "5 seconds", value: 5000 },
  { label: "10 seconds", value: 10000 },
  { label: "30 seconds", value: 30000 },
  { label: "1 minute", value: 60000 },
];

/** The status filter: "" is every status the log records. */
const STATUS_FILTERS = [
  { label: "All", value: "" },
  { label: "Warning", value: "1" },
  { label: "Critical", value: "2" },
  { label: "Unknown", value: "3" },
];

/**
 * The core's failed-check log: every query the agent dispatched, from any
 * transport, that came back WARNING, CRITICAL or UNKNOWN - newest first, with
 * the result as the check produced it and, when the agent records them, the
 * arguments it was given.
 */
export default function FailedChecks() {
  const dispatch = useAppDispatch();
  const [refreshRate, setRefreshRate] = useState<number>(0);
  const { data } = useGetFailuresQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const [clearFailures, { isLoading: isClearing }] = useClearFailuresMutation();
  const [filter, setFilter] = useState<string>("");
  const [status, setStatus] = useState<string>("");
  const [error, setError] = useState<string | undefined>();
  const [expanded, setExpanded] = useState<Set<number>>(new Set());

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Failures"]));
  };

  const onClear = async () => {
    try {
      await clearFailures().unwrap();
      setExpanded(new Set());
    } catch (err) {
      setError(`Failed to clear the failed checks: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  const toggleExpanded = (idx: number) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(idx)) next.delete(idx);
      else next.add(idx);
      return next;
    });
  };

  const entries = useMemo(() => data?.entries ?? [], [data]);
  const needle = filter.trim().toLowerCase();
  const filtered = useMemo(() => {
    return entries.filter((e) => {
      if (status && String(e.status) !== status) return false;
      if (!needle) return true;
      if (e.command.toLowerCase().includes(needle)) return true;
      if (e.alias.toLowerCase().includes(needle)) return true;
      if (e.subject.toLowerCase().includes(needle)) return true;
      if (e.message.toLowerCase().includes(needle)) return true;
      if (e.date.toLowerCase().includes(needle)) return true;
      return (e.arguments ?? []).some((a) => a.toLowerCase().includes(needle));
    });
  }, [entries, needle, status]);

  const narrowed = !!needle || !!status;

  return (
    <Stack direction="column">
      <Toolbar>
        <Trail title="Failed checks" />
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter failed checks" />
        <FormControl size="small" sx={{ minWidth: 120 }}>
          <InputLabel id="failures-status">Status</InputLabel>
          <Select
            labelId="failures-status"
            value={status}
            label="Status"
            onChange={(e: SelectChangeEvent<string>) => setStatus(e.target.value)}
          >
            {STATUS_FILTERS.map((s) => (
              <MenuItem key={s.value} value={s.value}>
                {s.label}
              </MenuItem>
            ))}
          </Select>
        </FormControl>
        {narrowed && (
          <Typography variant="body2" color="text.secondary">
            {filtered.length}/{entries.length}
          </Typography>
        )}
        <Button
          variant="outlined"
          color="error"
          startIcon={<DeleteSweepIcon />}
          onClick={onClear}
          loading={isClearing}
          disabled={entries.length === 0}
          sx={{ height: 40 }}
        >
          Clear all
        </Button>
        <FormControl size="small" sx={{ minWidth: 140 }}>
          <InputLabel id="failures-refresh-rate">Refresh</InputLabel>
          <Select
            labelId="failures-refresh-rate"
            value={refreshRate}
            label="Refresh"
            onChange={(e: SelectChangeEvent<number>) => setRefreshRate(e.target.value as number)}
          >
            {REFRESH_RATES.map((r) => (
              <MenuItem key={r.value} value={r.value}>
                {r.label}
              </MenuItem>
            ))}
          </Select>
        </FormControl>
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      {data && !data.enabled && (
        <Alert severity="info" sx={{ mb: 2 }}>
          The failed-check log is switched off. Set <code>enabled = true</code> under{" "}
          <code>[/settings/core/failed checks]</code> to record checks that come back WARNING, CRITICAL or UNKNOWN.
        </Alert>
      )}
      {data && data.enabled && (
        <Typography variant="caption" color="text.secondary" sx={{ mb: 1 }}>
          Keeping the last {data.max_entries} checks that came back {data.statuses.split(",").join(", ") || "non-OK"}
          {data.dropped > 0 ? ` · ${data.dropped} older ${data.dropped === 1 ? "entry" : "entries"} evicted` : ""}
          {data.arguments_recorded
            ? " · arguments recorded"
            : " · arguments not recorded (record arguments = false under [/settings/core/failed checks])"}
          {data.capture_metrics
            ? ` · metrics captured (${formatBytes(data.context_bytes_used)} of ${formatBytes(data.context_budget)})`
            : " · metrics not captured (capture metrics = false under [/settings/core/failed checks])"}
        </Typography>
      )}
      <TableContainer sx={{ width: "100%" }}>
        <Table size="small">
          <TableHead>
            <TableRow>
              <TableCell sx={{ width: 36 }} />
              <TableCell sx={{ width: 64 }}>#</TableCell>
              <TableCell sx={{ width: 200 }}>Date</TableCell>
              <TableCell sx={{ width: 110 }}>Status</TableCell>
              <TableCell sx={{ width: 220 }}>Command</TableCell>
              <TableCell>Message</TableCell>
              <TableCell sx={{ width: 180 }}>Source</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {filtered.length === 0 && (
              <TableRow>
                <TableCell colSpan={7}>
                  <Typography variant="body2" color="text.secondary" sx={{ p: 1 }}>
                    {narrowed ? "No failed checks match the filter." : "No failed checks recorded."}
                  </Typography>
                </TableCell>
              </TableRow>
            )}
            {filtered.map((e) => (
              <FailedCheckRow
                key={e.index}
                entry={e}
                isOpen={expanded.has(e.index)}
                onToggle={() => toggleExpanded(e.index)}
                argumentsRecorded={data?.arguments_recorded ?? false}
              />
            ))}
          </TableBody>
        </Table>
      </TableContainer>
      <Snackbar open={!!error} autoHideDuration={6000} onClose={() => setError(undefined)}>
        <Alert onClose={() => setError(undefined)} severity="error">
          {error}
        </Alert>
      </Snackbar>
    </Stack>
  );
}

interface FailedCheckRowProps {
  entry: FailedCheck;
  isOpen: boolean;
  onToggle: () => void;
  argumentsRecorded: boolean;
}

function FailedCheckRow({ entry, isOpen, onToggle, argumentsRecorded }: FailedCheckRowProps) {
  const perf: [string, QueryExecutionResultLinePerf][] = entry.lines.flatMap((l) => Object.entries(l.perf ?? {}));
  return (
    <>
      <TableRow hover sx={{ cursor: "pointer", "& > *": { borderBottom: isOpen ? 0 : undefined } }} onClick={onToggle}>
        <TableCell>
          <IconButton size="small" aria-label={isOpen ? "collapse" : "expand"}>
            {isOpen ? <KeyboardArrowDownIcon fontSize="small" /> : <KeyboardArrowRightIcon fontSize="small" />}
          </IconButton>
        </TableCell>
        <TableCell>{entry.index}</TableCell>
        <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }} title={formatAge(entry.age)}>
          {entry.date}
        </TableCell>
        <TableCell>
          <QueryResultChip result={entry.status} />
        </TableCell>
        <TableCell sx={{ fontFamily: "monospace" }}>
          {entry.command}
          {entry.depth > 0 && (
            <Chip size="small" variant="outlined" label="nested" sx={{ ml: 1 }} title="Run by another check" />
          )}
        </TableCell>
        <TableCell>
          <Typography
            variant="body2"
            color={entry.message ? "text.primary" : "text.secondary"}
            sx={{ maxWidth: 600, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}
          >
            {entry.message || "—"}
          </Typography>
        </TableCell>
        <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>{entry.subject}</TableCell>
      </TableRow>
      <TableRow>
        <TableCell colSpan={7} sx={{ pb: 0, pt: 0, border: 0 }}>
          <Collapse in={isOpen} timeout="auto" unmountOnExit>
            <Stack spacing={1} sx={{ my: 2, ml: 6 }}>
              {entry.alias && (
                <Typography variant="body2" color="text.secondary">
                  alias <code>{entry.alias}</code>
                </Typography>
              )}
              {entry.lines.map((line, i) => (
                <Typography key={i} variant="body2" sx={{ fontFamily: "monospace", whiteSpace: "pre-wrap" }}>
                  {line.message}
                </Typography>
              ))}
              {perf.length > 0 && (
                <Table size="small" sx={{ maxWidth: 900 }} aria-label="performance data">
                  <TableHead>
                    <TableRow>
                      <TableCell>Perf</TableCell>
                      <TableCell align="right">Value</TableCell>
                      <TableCell>Unit</TableCell>
                      <TableCell align="right">Warning</TableCell>
                      <TableCell align="right">Critical</TableCell>
                      <TableCell align="right">Min</TableCell>
                      <TableCell align="right">Max</TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {perf.map(([key, p]) => (
                      <TableRow key={key}>
                        <TableCell sx={{ fontFamily: "monospace" }}>{key}</TableCell>
                        <TableCell align="right">{formatValue(p.value)}</TableCell>
                        <TableCell>{p.unit ?? ""}</TableCell>
                        <TableCell align="right">{formatValue(p.warning)}</TableCell>
                        <TableCell align="right">{formatValue(p.critical)}</TableCell>
                        <TableCell align="right">{formatValue(p.minimum)}</TableCell>
                        <TableCell align="right">{formatValue(p.maximum)}</TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              )}
              <Stack direction="row" spacing={1} useFlexGap sx={{ flexWrap: "wrap", alignItems: "center" }}>
                <Typography variant="body2" color="text.secondary">
                  Arguments:
                </Typography>
                {!argumentsRecorded || entry.arguments === undefined ? (
                  <Typography variant="body2" color="text.secondary">
                    not recorded
                  </Typography>
                ) : entry.arguments.length === 0 ? (
                  <Typography variant="body2" color="text.secondary">
                    none
                  </Typography>
                ) : (
                  entry.arguments.map((a, i) => (
                    <Chip key={i} size="small" variant="outlined" label={a} sx={{ fontFamily: "monospace" }} />
                  ))
                )}
              </Stack>
              <ContextPanel entry={entry} />
            </Stack>
          </Collapse>
        </TableCell>
      </TableRow>
    </>
  );
}

/** Why an entry has no snapshot to show, keyed by its context state. */
const CONTEXT_STATE_TEXT: Record<Exclude<FailedCheckContextState, "captured">, string> = {
  disabled: "not captured (capture metrics = false under [/settings/core/failed checks] when this was recorded)",
  evicted: "released to stay within the metrics context budget",
  too_large: "larger than max context size, not kept",
  none: "no snapshot had been collected yet",
};

/**
 * "System at the time": the agent's metrics snapshot the failure was recorded
 * with, fetched only once the row is opened.
 */
function ContextPanel({ entry }: { entry: FailedCheck }) {
  const state = entry.context?.state ?? "disabled";
  return (
    <Stack spacing={1} sx={{ mt: 1 }}>
      <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
        <Typography variant="subtitle2">System at the time</Typography>
        {state === "captured" ? (
          <Typography variant="body2" color="text.secondary">
            snapshot #{entry.context.snapshot_id}, {entry.context.age_at_failure ?? 0}s before the failure
          </Typography>
        ) : (
          <Chip size="small" variant="outlined" label={state.replace("_", " ")} title={CONTEXT_STATE_TEXT[state]} />
        )}
      </Stack>
      {state === "captured" ? (
        <ContextTable index={entry.index} />
      ) : (
        <Typography variant="body2" color="text.secondary">
          Metrics {CONTEXT_STATE_TEXT[state]}.
        </Typography>
      )}
    </Stack>
  );
}

function ContextTable({ index }: { index: number }) {
  const { data, isLoading, isError } = useGetFailureContextQuery(index);
  const [filter, setFilter] = useState<string>("");
  const rows = useMemo(() => {
    const needle = filter.trim().toLowerCase();
    return Object.entries(data?.metrics ?? {})
      .filter(([key]) => !needle || key.toLowerCase().includes(needle))
      .sort(([a], [b]) => a.localeCompare(b));
  }, [data, filter]);

  if (isLoading) {
    return (
      <Typography variant="body2" color="text.secondary">
        Loading…
      </Typography>
    );
  }
  if (isError || !data) {
    return (
      <Typography variant="body2" color="text.secondary">
        The snapshot is no longer available.
      </Typography>
    );
  }
  if (!data.metrics) {
    return (
      <Typography variant="body2" color="text.secondary">
        Metrics {data.state === "captured" ? "unavailable" : CONTEXT_STATE_TEXT[data.state]}.
      </Typography>
    );
  }
  return (
    <Stack spacing={1} sx={{ maxWidth: 900 }}>
      <FilterField value={filter} onChange={setFilter} placeholder="Filter metrics" />
      <Table size="small" aria-label="metrics at the time">
        <TableHead>
          <TableRow>
            <TableCell>Metric</TableCell>
            <TableCell align="right">Value</TableCell>
            <TableCell>Unit</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {rows.length === 0 && (
            <TableRow>
              <TableCell colSpan={3}>
                <Typography variant="body2" color="text.secondary">
                  No metrics match the filter.
                </Typography>
              </TableCell>
            </TableRow>
          )}
          {rows.map(([key, value]) => (
            <TableRow key={key}>
              <TableCell sx={{ fontFamily: "monospace" }} title={data.metadata?.[key]?.help}>
                {key}
              </TableCell>
              <TableCell align="right">{String(value)}</TableCell>
              <TableCell>{data.metadata?.[key]?.unit ?? ""}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </Stack>
  );
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}

/** Perf fields are optional and thresholds may be Nagios range strings. */
function formatValue(value: number | string | undefined): string {
  if (value === undefined || value === null) return "";
  return String(value);
}
