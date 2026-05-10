import Stack from "@mui/material/Stack";
import { nsclientApi, useGetMetricsQuery } from "../api/api.ts";
import {
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
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { useEffect, useMemo, useRef, useState } from "react";
import CloseIcon from "@mui/icons-material/Close";
import { parseMetrics } from "../metric_parser.ts";
import FilterField from "./atoms/FilterField.tsx";

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

export default function Metrics() {
  const dispatch = useAppDispatch();
  const [filter, setFilter] = useState<string>("");
  const [refreshRate, setRefreshRate] = useState<number>(0);
  const { data: metrics, fulfilledTimeStamp } = useGetMetricsQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });

  const result = useMemo(() => parseMetrics(metrics), [metrics]);

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

  // The filter string drives both the toggle buttons and the free-text field.
  // We treat "<module>" and "<module>.<type>" as structured filters from the
  // toggles; anything else is a free-text search.
  const [selectedModule, selectedType] = useMemo(() => {
    const trimmed = filter.trim();
    if (!trimmed) return ["", ""];
    if (result.modules.includes(trimmed)) return [trimmed, ""];
    const dot = trimmed.indexOf(".");
    if (dot > 0) {
      const head = trimmed.slice(0, dot);
      const tail = trimmed.slice(dot + 1);
      if (result.modules.includes(head)) return [head, tail];
    }
    return ["", ""];
  }, [filter, result.modules]);

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

  const moduleTypes = selectedModule ? (typesByModule[selectedModule] ?? []) : [];

  const onPickModule = (mod: string | null) => setFilter(mod ?? "");
  const onPickType = (t: string | null) => {
    if (!selectedModule) return;
    setFilter(t ? `${selectedModule}.${t}` : selectedModule);
  };

  const needle = filter.trim().toLowerCase();
  const filteredMetrics = useMemo(() => {
    if (!needle) return result.metrics;
    return result.metrics.filter((m) =>
      [m.key, m.module, m.type, m.instance, m.metric].some((f) => (f ?? "").toLowerCase().includes(needle)),
    );
  }, [result.metrics, needle]);

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Metrics"]));
  };

  const sortedMetrics = useMemo(
    () => [...filteredMetrics].sort((a, b) => a.key.localeCompare(b.key)),
    [filteredMetrics],
  );

  return (
    <Stack direction="column">
      <Toolbar>
        <ToggleButtonGroup
          size="small"
          value={selectedModule}
          exclusive
          onChange={(_e, f) => onPickModule(f)}
          aria-label="filter by module"
          sx={{ "& .MuiToggleButton-root": { px: 1, py: 0.5 } }}
        >
          {result.modules.map((m) => (
            <ToggleButton key={m} value={m}>
              {m}
            </ToggleButton>
          ))}
          <ToggleButton value="">
            <CloseIcon fontSize="small" />
          </ToggleButton>
        </ToggleButtonGroup>
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter metrics…" />
        {needle && (
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
      {selectedModule && moduleTypes.length > 0 && (
        <Toolbar>
          <Typography variant="caption" color="text.secondary" sx={{ mr: 1 }}>
            {selectedModule}.
          </Typography>
          <ToggleButtonGroup
            size="small"
            value={selectedType}
            exclusive
            onChange={(_e, t) => onPickType(t)}
            aria-label={`filter by ${selectedModule} type`}
            sx={{ "& .MuiToggleButton-root": { px: 1, py: 0.5 } }}
          >
            {moduleTypes.map((t) => (
              <ToggleButton key={t} value={t}>
                {t}
              </ToggleButton>
            ))}
            <ToggleButton value="">
              <CloseIcon fontSize="small" />
            </ToggleButton>
          </ToggleButtonGroup>
        </Toolbar>
      )}
      <TableContainer sx={{ width: "100%" }}>
        <Table>
          <TableHead>
            <TableRow>
              <TableCell>Module</TableCell>
              <TableCell>Type</TableCell>
              <TableCell>Instance</TableCell>
              <TableCell>Metric</TableCell>
              <TableCell align="right">Value</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {sortedMetrics.map((m) => {
              const changed = recentlyChanged.has(m.key);
              return (
                <TableRow
                  hover
                  key={m.key}
                  sx={{
                    "&:last-child td, &:last-child th": { border: 0 },
                    backgroundColor: changed ? "rgba(67,160,71,0.22)" : undefined,
                    transition: "background-color 1.5s ease-out",
                  }}
                >
                  <TableCell>{m.module}</TableCell>
                  <TableCell>{m.type}</TableCell>
                  <TableCell>{m.instance}</TableCell>
                  <TableCell>{m.metric}</TableCell>
                  <TableCell align="right">{m.value}</TableCell>
                </TableRow>
              );
            })}
          </TableBody>
        </Table>
      </TableContainer>
    </Stack>
  );
}
