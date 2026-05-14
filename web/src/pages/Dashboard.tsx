import { useGetMetricsQuery } from "../api/api.ts";
import { parseMetrics } from "../metric_parser.ts";
import { useMemo } from "react";
import { Box, CircularProgress, FormControl, InputLabel, MenuItem, Select, SelectChangeEvent, Tooltip } from "@mui/material";
import Typography from "@mui/material/Typography";
import Grid from "@mui/material/Grid";
import CpuWidget from "../components/CpuWidget.tsx";
import MemoryWidget from "../components/MemoryWidget.tsx";
import NetworkWidget from "../components/NetworkWidget.tsx";
import SystemInfoWidget from "../components/SystemInfoWidget.tsx";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { setRefreshRate } from "../common/dashboardSlice.ts";
import DiskIoWidget from "../components/DiskIoWidget.tsx";
import DiskFreeWidget from "../components/DiskFreeWidget.tsx";

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

export default function Dashboard() {
  const dispatch = useAppDispatch();
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data: metrics, fulfilledTimeStamp, isLoading } = useGetMetricsQuery(undefined, {
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

  // Minimum sensible polling interval (ms) â€“ no point polling faster than the server updates
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

  // --- Data availability guards ---
  const hasCpuData = result.metrics.some(
    (m) => m.key === "system.cpu.total.kernel" || m.key === "system.cpu.total.user",
  );
  const hasMemData = result.metrics.some(
    (m) => m.key === "system.mem.physical.used" && result.metrics.some((n) => n.key === "system.mem.physical.total"),
  );
  const hasNetworkData = result.metrics.some((m) => m.type === "network" && m.metric === "NetConnectionID");
  const hasDiskIoData = result.metrics.some((m) => m.module === "disk" && m.type === "io");
  const hasDiskFreeData = result.metrics.some((m) => m.module === "disk" && m.type === "free");
  const hasSystemInfoData = result.metrics.some(
    (m) => m.key.startsWith("system.uptime") || m.key.startsWith("system.metrics") || m.key.startsWith("workers."),
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
                {REFRESH_RATES.map((r) => {
                  const tooLow = r.value > 0 && r.value < maxRefreshMs;
                  return (
                    <MenuItem
                      key={r.value}
                      value={r.value}
                      disabled={tooLow}
                      sx={{ "&.Mui-disabled": { pointerEvents: "auto" } }}
                    >
                      <Tooltip
                        title={
                          tooLow
                            ? `Server updates every ${maxRefreshMs / 1000}s â€“ polling faster has no effect`
                            : ""
                        }
                        placement="right"
                      >
                        <Box sx={{ width: "100%" }}>{r.label}</Box>
                      </Tooltip>
                    </MenuItem>
                  );
                })}
              </Select>
            </FormControl>
            {isLoading && <CircularProgress size={20} />}
          </Box>
        </Grid>
        {hasCpuData && (
          <Grid size={{ xs: 12, md: 6 }}>
            <CpuWidget
              key={refreshRate}
              metrics={result.metrics}
              fulfilledTimeStamp={fulfilledTimeStamp}
              xAxis={xAxis}
              historySize={HISTORY_SIZE}
            />
          </Grid>
        )}
        {hasMemData && (
          <Grid size={{ xs: 12, md: 6 }}>
            <MemoryWidget
              key={refreshRate}
              metrics={result.metrics}
              fulfilledTimeStamp={fulfilledTimeStamp}
              xAxis={xAxis}
              historySize={HISTORY_SIZE}
            />
          </Grid>
        )}
        {hasNetworkData && (
          <Grid size={{ xs: 12, md: 6 }}>
            <NetworkWidget
              key={refreshRate}
              metrics={result.metrics}
              fulfilledTimeStamp={fulfilledTimeStamp}
              xAxis={xAxis}
              historySize={HISTORY_SIZE}
            />
          </Grid>
        )}
        {hasDiskIoData && (
          <Grid size={{ xs: 12, md: 6 }}>
            <DiskIoWidget
              key={refreshRate}
              metrics={result.metrics}
              fulfilledTimeStamp={fulfilledTimeStamp}
              xAxis={xAxis}
              historySize={HISTORY_SIZE}
            />
          </Grid>
        )}
        {hasDiskFreeData && (
          <Grid size={{ xs: 12, md: 6 }}>
            <DiskFreeWidget metrics={result.metrics} />
          </Grid>
        )}
        {hasSystemInfoData && (
          <Grid size={{ xs: 12, md: 6 }}>
            <SystemInfoWidget metrics={result.metrics} />
          </Grid>
        )}
      </Grid>
    </Box>
  );
}
