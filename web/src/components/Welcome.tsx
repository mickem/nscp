import { useGetMetricsQuery } from "../api/api.ts";
import { parseMetrics } from "../metric_parser.ts";
import { useMemo } from "react";
import { Box, FormControl, InputLabel, MenuItem, Select, SelectChangeEvent } from "@mui/material";
import Typography from "@mui/material/Typography";
import Grid from "@mui/material/Grid";
import CpuWidget from "./CpuWidget.tsx";
import MemoryWidget from "./MemoryWidget.tsx";
import NetworkWidget from "./NetworkWidget.tsx";
import SystemInfoWidget from "./SystemInfoWidget.tsx";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { setRefreshRate } from "../common/dashboardSlice.ts";

const HISTORY_SIZE = 30;

const REFRESH_RATES = [
  { label: "Off", value: 0 },
  { label: "1 second", value: 1000 },
  { label: "5 seconds", value: 5000 },
  { label: "10 seconds", value: 10000 },
  { label: "30 seconds", value: 30000 },
  { label: "1 minute", value: 60000 },
];

function buildXAxis(intervalMs: number, size: number = HISTORY_SIZE) {
  const stepSec = intervalMs / 1000;
  return Array.from({ length: size }, (_, i) => (i - size + 1) * stepSec);
}

function formatXValue(value: number) {
  if (value === 0) return "now";
  const seconds = Math.round(value);
  if (Math.abs(seconds) < 120) return `${seconds} s`;
  return `${Math.round(seconds / 60)} min`;
}

export default function Welcome() {
  const dispatch = useAppDispatch();
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data: metrics, fulfilledTimeStamp } = useGetMetricsQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });

  const result = useMemo(() => parseMetrics(metrics), [metrics]);

  // --- Server-reported refresh intervals (seconds) ---
  const serverIntervals = useMemo(() => {
    const get = (key: string) => {
      const m = result.metrics.find((metric) => metric.key === key);
      return m !== undefined ? Number(m.value) : undefined;
    };
    return {
      workersRefresh: get("workers.refresh_interval") ?? 10,
      systemRefresh: get("system.refresh_interval") ?? 10,
      networkRefresh: get("system.network_refresh_interval") ?? 100,
    };
  }, [result.metrics]);

  // Minimum sensible polling interval (ms) – no point polling faster than the server updates
  const maxRefreshMs = useMemo(
    () =>
      Math.max(serverIntervals.workersRefresh, serverIntervals.systemRefresh) * 1000,
    [serverIntervals],
  );

  // --- X-axis for system widgets (CPU / Memory) ---
  const xAxisData = useMemo(() => buildXAxis(refreshRate || 5000), [refreshRate]);
  const xAxisMin = xAxisData[0];
  const xAxis = useMemo(
    () => [{ data: xAxisData, valueFormatter: formatXValue, min: xAxisMin, max: 0 }],
    [xAxisData, xAxisMin],
  );

  const handleRefreshRateChange = (event: SelectChangeEvent<number>) => {
    dispatch(setRefreshRate(event.target.value as number));
  };

  return (
    <Box>
      <Grid container spacing={2}>
        <Grid size={12}>
          <Box sx={{ display: "flex", alignItems: "center", gap: 2 }}>
            <Typography variant="h4" component="div">
              Dashboard
            </Typography>
            <FormControl size="small" sx={{ minWidth: 150 }}>
              <InputLabel id="refresh-rate-label">Refresh rate</InputLabel>
              <Select
                labelId="refresh-rate-label"
                value={refreshRate}
                label="Refresh rate"
                onChange={handleRefreshRateChange}
              >
                {REFRESH_RATES.map((r) => (
                  <MenuItem key={r.value} value={r.value} disabled={r.value > 0 && r.value < maxRefreshMs}>
                    {r.label}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Box>
        </Grid>
        <Grid>
          <CpuWidget
            key={refreshRate}
            metrics={result.metrics}
            fulfilledTimeStamp={fulfilledTimeStamp}
            xAxis={xAxis}
            historySize={HISTORY_SIZE}
          />
        </Grid>
        <Grid>
          <MemoryWidget
            key={refreshRate}
            metrics={result.metrics}
            fulfilledTimeStamp={fulfilledTimeStamp}
            xAxis={xAxis}
            historySize={HISTORY_SIZE}
          />
        </Grid>
        <Grid>
          <NetworkWidget
            key={refreshRate}
            metrics={result.metrics}
            fulfilledTimeStamp={fulfilledTimeStamp}
            xAxis={xAxis}
            historySize={HISTORY_SIZE}
          />
        </Grid>
        <Grid>
          <SystemInfoWidget metrics={result.metrics} />
        </Grid>
      </Grid>
    </Box>
  );
}
