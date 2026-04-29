import { useMemo } from "react";
import { Box, Card, CardContent, LinearProgress, Stack } from "@mui/material";
import Typography from "@mui/material/Typography";
import { Metric } from "../metric_parser.ts";

function formatBytes(bytes: number): string {
  if (bytes >= 1024 ** 4) return `${(bytes / 1024 ** 4).toFixed(1)} TB`;
  if (bytes >= 1024 ** 3) return `${(bytes / 1024 ** 3).toFixed(1)} GB`;
  if (bytes >= 1024 ** 2) return `${(bytes / 1024 ** 2).toFixed(1)} MB`;
  return `${(bytes / 1024).toFixed(1)} KB`;
}

interface DiskFreeWidgetProps {
  metrics: Metric[];
}

export default function DiskFreeWidget({ metrics }: DiskFreeWidgetProps) {
  const disks = useMemo(() => {
    const diskMetrics = metrics.filter((m) => m.module === "disk" && m.type === "free");
    const instances = [...new Set(diskMetrics.map((m) => m.instance).filter(Boolean))];
    return instances
      .map((instance) => {
        const get = (metric: string) =>
          diskMetrics.find((m) => m.instance === instance && m.metric === metric)?.value as number | undefined;
        return {
          name: instance!,
          total: get("total") ?? 0,
          free: get("free") ?? 0,
          used: get("used") ?? 0,
          usedPct: get("used_pct") ?? 0,
        };
      })
      .filter((d) => d.total > 0)
      .sort((a, b) => a.name.localeCompare(b.name));
  }, [metrics]);

  if (disks.length === 0) return null;

  return (
    <Card variant="outlined" sx={{ height: "100%" }}>
      <CardContent>
        <Typography gutterBottom variant="h5" component="div">
          Disk Space
        </Typography>
        <Stack spacing={2}>
          {disks.map((disk) => (
            <Box key={disk.name}>
              <Box sx={{ display: "flex", justifyContent: "space-between", mb: 0.5 }}>
                <Typography variant="body2" fontWeight="bold">
                  {disk.name}
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  {formatBytes(disk.free)} free of {formatBytes(disk.total)}
                </Typography>
              </Box>
              <LinearProgress
                variant="determinate"
                value={disk.usedPct}
                sx={{
                  height: 20,
                  borderRadius: 1,
                  backgroundColor: "success.light",
                  "& .MuiLinearProgress-bar": {
                    backgroundColor:
                      disk.usedPct > 90 ? "error.main" : disk.usedPct > 75 ? "warning.main" : "primary.main",
                  },
                }}
              />
            </Box>
          ))}
        </Stack>
      </CardContent>
    </Card>
  );
}

