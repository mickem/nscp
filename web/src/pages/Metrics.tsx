import Stack from "@mui/material/Stack";
import { nsclientApi, useGetDescribedMetricsQuery } from "../api/api.ts";
import {
  Chip,
  FormControl,
  InputLabel,
  MenuItem,
  Select,
  SelectChangeEvent,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  ToggleButton,
  ToggleButtonGroup,
  Typography,
} from "@mui/material";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { Fragment, useEffect, useMemo, useRef, useState } from "react";
import CloseIcon from "@mui/icons-material/Close";
import { Metric, parseMetrics } from "../metric_parser.ts";
import { formatMetricValue } from "../common/format.ts";
import FilterField from "../components/atoms/FilterField.tsx";

const REFRESH_RATES = [
  { label: "Off", value: 0 },
  { label: "1 second", value: 1000 },
  { label: "5 seconds", value: 5000 },
  { label: "10 seconds", value: 10000 },
  { label: "30 seconds", value: 30000 },
  { label: "1 minute", value: 60000 },
];

// How long a row stays highlighted after its value changes.
const HIGHLIGHT_MS = 1500;

// The toggle that clears a selection. Not "": a script module publishes its
// metrics in a bundle with no name, so the empty string is a real module.
const ALL = "\u0000all";
// How that nameless module is spelled on screen.
const UNNAMED = "(unnamed)";
const moduleLabel = (module: string) => module || UNNAMED;

interface MetricGroup {
  id: string;
  /** Module, type and instance: what every metric in the group is measured on. */
  path: string[];
  unnamed: boolean;
  metrics: Metric[];
  /** Labels every metric in the group carries with the same value. */
  labels: [string, string][];
}

type Metadata = { [key: string]: { labels?: { [key: string]: string } } } | undefined;

/** Consecutive metrics of one module, type and instance, so those are said once and not per row. */
function groupMetrics(metrics: Metric[], metadata: Metadata): MetricGroup[] {
  const groups = new Map<string, MetricGroup>();
  for (const m of metrics) {
    const id = [m.module, m.type ?? "", m.instance ?? ""].join("\u0000");
    let group = groups.get(id);
    if (!group) {
      const path = [moduleLabel(m.module), m.type, m.instance].filter((p): p is string => !!p);
      group = { id, path, unnamed: !m.module, metrics: [], labels: [] };
      groups.set(id, group);
    }
    group.metrics.push(m);
  }
  const sorted = [...groups.values()].sort((a, b) => a.id.localeCompare(b.id));
  for (const group of sorted) {
    group.metrics.sort((a, b) => a.metric.localeCompare(b.metric));
    const first = Object.entries(metadata?.[group.metrics[0].key]?.labels ?? {});
    group.labels = first.filter(([k, v]) =>
      group.metrics.every((m) => metadata?.[m.key]?.labels?.[k] === v),
    );
  }
  return sorted;
}

export default function Metrics() {
  const dispatch = useAppDispatch();
  const [filter, setFilter] = useState<string>("");
  const [refreshRate, setRefreshRate] = useState<number>(0);
  // null is "no selection"; "" is the module whose bundle has no name.
  const [pickedModule, setPickedModule] = useState<string | null>(null);
  const [pickedType, setPickedType] = useState<string | null>(null);
  const { data, fulfilledTimeStamp } = useGetDescribedMetricsQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });

  const result = useMemo(() => parseMetrics(data?.metrics), [data]);
  const metadata = data?.metadata;

  // Track previous values per key so we can highlight rows that just changed.
  const previousValuesRef = useRef<Map<string, string | number>>(new Map());
  const [recentlyChanged, setRecentlyChanged] = useState<Set<string>>(new Set());

  useEffect(() => {
    const prev = previousValuesRef.current;
    const justChanged: string[] = [];
    for (const m of result.metrics) {
      const old = prev.get(m.key);
      if (old !== undefined && old !== m.value) {
        justChanged.push(m.key);
      }
      prev.set(m.key, m.value);
    }
    if (justChanged.length === 0) return;
    setRecentlyChanged((curr) => {
      const next = new Set(curr);
      for (const k of justChanged) next.add(k);
      return next;
    });
    const timer = setTimeout(() => {
      setRecentlyChanged((curr) => {
        const next = new Set(curr);
        for (const k of justChanged) next.delete(k);
        return next;
      });
    }, HIGHLIGHT_MS);
    return () => clearTimeout(timer);
    // We intentionally key the effect on the response timestamp rather than
    // result.metrics — running once per server response is what we want.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fulfilledTimeStamp]);

  // A module that is no longer published (a script was unloaded) drops its
  // selection instead of leaving an empty table behind a toggle that is gone.
  const selectedModule =
    pickedModule !== null && result.modules.includes(pickedModule) ? pickedModule : null;

  const typesByModule = useMemo(() => {
    const map: Record<string, Set<string>> = {};
    for (const m of result.metrics) {
      if (!m.type) continue;
      if (!map[m.module]) map[m.module] = new Set();
      map[m.module].add(m.type);
    }
    return Object.fromEntries(
      Object.entries(map).map(([k, v]) => [k, [...v].sort((a, b) => a.localeCompare(b))]),
    );
  }, [result.metrics]);

  const moduleTypes = selectedModule !== null ? (typesByModule[selectedModule] ?? []) : [];
  const selectedType = pickedType !== null && moduleTypes.includes(pickedType) ? pickedType : null;

  const onPickModule = (mod: string | null) => {
    setPickedModule(mod === null || mod === ALL ? null : mod);
    setPickedType(null);
  };
  const onPickType = (t: string | null) => setPickedType(t === null || t === ALL ? null : t);

  // The toggles select exactly; the text field searches everything a row
  // shows, including what the metric means.
  const needle = filter.trim().toLowerCase();
  const filteredMetrics = useMemo(() => {
    return result.metrics.filter((m) => {
      if (selectedModule !== null && m.module !== selectedModule) return false;
      if (selectedType !== null && m.type !== selectedType) return false;
      if (!needle) return true;
      const meta = metadata?.[m.key];
      const labels = Object.entries(meta?.labels ?? {}).map(([k, v]) => `${k}=${v}`);
      return [m.key, moduleLabel(m.module), meta?.help, meta?.unit, meta?.type, ...labels].some(
        (f) => (f ?? "").toLowerCase().includes(needle),
      );
    });
  }, [result.metrics, metadata, selectedModule, selectedType, needle]);
  const isFiltered = filteredMetrics.length !== result.metrics.length;

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Metrics"]));
  };

  const groups = useMemo(
    () => groupMetrics(filteredMetrics, metadata),
    [filteredMetrics, metadata],
  );

  return (
    <Stack direction="column">
      <Toolbar>
        <ToggleButtonGroup
          size="small"
          value={selectedModule ?? ALL}
          exclusive
          onChange={(_e, f) => onPickModule(f)}
          aria-label="filter by module"
          sx={{ "& .MuiToggleButton-root": { px: 1, py: 0.5 } }}
        >
          {result.modules.map((m) => (
            <ToggleButton
              key={m}
              value={m}
              sx={m ? undefined : { fontStyle: "italic", textTransform: "none" }}
            >
              {moduleLabel(m)}
            </ToggleButton>
          ))}
          <ToggleButton value={ALL} aria-label="all modules">
            <CloseIcon fontSize="small" />
          </ToggleButton>
        </ToggleButtonGroup>
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter metrics…" />
        {isFiltered && (
          <Typography variant="body2" color="text.secondary">
            {filteredMetrics.length}/{result.metrics.length}
          </Typography>
        )}
        <FormControl size="small" sx={{ minWidth: 140 }}>
          <InputLabel id="metrics-refresh-rate">Refresh</InputLabel>
          <Select
            labelId="metrics-refresh-rate"
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
      {selectedModule !== null && moduleTypes.length > 0 && (
        <Toolbar>
          <Typography variant="caption" color="text.secondary" sx={{ mr: 1 }}>
            {moduleLabel(selectedModule)}.
          </Typography>
          <ToggleButtonGroup
            size="small"
            value={selectedType ?? ALL}
            exclusive
            onChange={(_e, t) => onPickType(t)}
            aria-label={`filter by ${moduleLabel(selectedModule)} type`}
            sx={{ "& .MuiToggleButton-root": { px: 1, py: 0.5 } }}
          >
            {moduleTypes.map((t) => (
              <ToggleButton key={t} value={t}>
                {t}
              </ToggleButton>
            ))}
            <ToggleButton value={ALL} aria-label="all types">
              <CloseIcon fontSize="small" />
            </ToggleButton>
          </ToggleButtonGroup>
        </Toolbar>
      )}
      <TableContainer sx={{ width: "100%" }}>
        <Table size="small">
          <TableHead>
            <TableRow>
              <TableCell>Metric</TableCell>
              <TableCell>Description</TableCell>
              <TableCell align="right">Value</TableCell>
              <TableCell>Kind</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {groups.map((group) => (
              <Fragment key={group.id}>
                <TableRow
                  data-group={group.path.join(".")}
                  sx={{ backgroundColor: "action.hover" }}
                >
                  <TableCell colSpan={4} sx={{ py: 0.75 }}>
                    <Stack
                      direction="row"
                      spacing={1}
                      useFlexGap
                      sx={{ alignItems: "center", flexWrap: "wrap" }}
                    >
                      <Typography
                        variant="subtitle2"
                        component="span"
                        sx={
                          group.unnamed
                            ? { fontStyle: "italic", color: "text.secondary" }
                            : undefined
                        }
                      >
                        {group.path.join(" › ")}
                      </Typography>
                      {group.labels.map(([k, v]) => (
                        <Chip key={k} size="small" variant="outlined" label={`${k}=${v}`} />
                      ))}
                    </Stack>
                  </TableCell>
                </TableRow>
                {group.metrics.map((m) => {
                  const changed = recentlyChanged.has(m.key);
                  const meta = metadata?.[m.key];
                  const common = new Set(group.labels.map(([k]) => k));
                  const labels = Object.entries(meta?.labels ?? {}).filter(([k]) => !common.has(k));
                  const shown = formatMetricValue(m.value, meta?.unit);
                  const raw = `${m.value}${meta?.unit ? ` ${meta.unit}` : ""}`;
                  return (
                    <TableRow
                      hover
                      key={m.key}
                      data-metric={m.key}
                      sx={{
                        backgroundColor: changed ? "rgba(67,160,71,0.22)" : undefined,
                        transition: "background-color 1.5s ease-out",
                      }}
                    >
                      <TableCell title={m.key} sx={{ pl: 4, whiteSpace: "nowrap" }}>
                        {m.metric}
                        {labels.map(([k, v]) => (
                          <Chip
                            key={k}
                            size="small"
                            variant="outlined"
                            label={`${k}=${v}`}
                            sx={{ ml: 1 }}
                          />
                        ))}
                      </TableCell>
                      <TableCell sx={{ color: "text.secondary", width: "100%" }}>
                        {meta?.help ?? ""}
                      </TableCell>
                      <TableCell
                        align="right"
                        title={shown === raw ? undefined : raw}
                        sx={{ whiteSpace: "nowrap", fontVariantNumeric: "tabular-nums" }}
                      >
                        {shown}
                      </TableCell>
                      <TableCell sx={{ color: "text.secondary" }}>{meta?.type ?? ""}</TableCell>
                    </TableRow>
                  );
                })}
              </Fragment>
            ))}
          </TableBody>
        </Table>
      </TableContainer>
    </Stack>
  );
}
