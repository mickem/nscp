import { nsclientApi, useGetMetricsQuery } from "../api/api.ts";
import { LineChart } from "@mui/x-charts/LineChart";
import { parseMetrics } from "../metric_parser.ts";
import { useEffect, useMemo, useState } from "react";
import { useAppDispatch } from "../store/store.ts";
import { Box, Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import Grid from "@mui/material/Grid2";

export default function Welcome() {
  const dispatch = useAppDispatch();
  const { data: metrics } = useGetMetricsQuery();
  const [kernelCpu, setKernelCpu] = useState<number[]>([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
  const [userCpu, setUserCpu] = useState<number[]>([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
  const [mem, setMem] = useState<number[]>([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  useEffect(() => {
    setInterval(() => {
      dispatch(nsclientApi.util.invalidateTags(["Metrics"]));
    }, 5000);
  }, [dispatch]);

  const result = useMemo(() => parseMetrics(metrics), [metrics]);
  const filteredMetrics = useMemo(
    () => result.metrics.filter((m) => m.key.startsWith("system.cpu.total") || m.key.startsWith("system.mem.physical")),
    [result.metrics],
  );

  useEffect(() => {
    const kernelCpuMetric = filteredMetrics.find((m) => m.key === "system.cpu.total.kernel");
    if (kernelCpuMetric) {
      setKernelCpu((old) => [...old, kernelCpuMetric.value as number].slice(-10));
    }
    const userCpuMetric = filteredMetrics.find((m) => m.key === "system.cpu.total.user");
    if (userCpuMetric) {
      setUserCpu((old) => [...old, userCpuMetric.value as number].slice(-10));
    }
    const memUsed = filteredMetrics.find((m) => m.key === "system.mem.physical.used");
    const memTotal = filteredMetrics.find((m) => m.key === "system.mem.physical.total");
    if (memUsed && memTotal) {
      const pct = Math.round((1000 * (memUsed.value as number)) / (memTotal.value as number)) / 10;
      setMem((old) => [...old, pct].slice(-10));
    }
  }, [filteredMetrics]);

  return (
    <Box>
      <Grid container spacing={2}>
        <Grid>
          <Card variant="outlined">
            <CardContent>
              <Typography gutterBottom variant="h5" component="div">
                CPU Load
              </Typography>
              <LineChart
                xAxis={[
                  {
                    data: [-45, -40, -35, -30, -25, -20, -15, -10, -5, 0],
                    valueFormatter: (value) => (value === 0 ? "now" : `${value} s`),
                    min: -45,
                    max: 0,
                  },
                ]}
                series={[
                  {
                    id: "Kernel",
                    label: "Kernel time (%)",
                    data: kernelCpu,
                    stack: "total",
                    area: true,
                  },
                  {
                    id: "User",
                    label: "User time (%)",
                    data: userCpu,
                    stack: "total",
                    area: true,
                  },
                ]}
                grid={{ vertical: true, horizontal: true }}
                width={500}
                height={300}
                yAxis={[{ min: 0, max: 100 }]}
              />
            </CardContent>
          </Card>
        </Grid>
        <Grid>
          <Card variant="outlined">
            <CardContent>
              <Typography gutterBottom variant="h5" component="div">
                Memory usage
              </Typography>
              <LineChart
                xAxis={[
                  {
                    data: [-45, -40, -35, -30, -25, -20, -15, -10, -5, 0],
                    valueFormatter: (value) => (value === 0 ? "now" : `${value} s`),
                    min: -45,
                    max: 0,
                  },
                ]}
                yAxis={[{ min: 0, max: 100 }]}
                series={[
                  {
                    id: "Memory",
                    label: "Memory (%)",
                    data: mem,
                    area: true,
                  },
                ]}
                grid={{ vertical: true, horizontal: true }}
                width={500}
                height={300}
              />
            </CardContent>
          </Card>
        </Grid>
      </Grid>
    </Box>
  );
}
