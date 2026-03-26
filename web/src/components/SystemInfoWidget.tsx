import { useMemo } from "react";
import { Card, CardContent, Table, TableBody, TableCell, TableRow } from "@mui/material";
import Typography from "@mui/material/Typography";
import { Metric } from "../metric_parser.ts";

interface SystemInfoWidgetProps {
  metrics: Metric[];
}

export default function SystemInfoWidget({ metrics }: SystemInfoWidgetProps) {
  const systemInfo = useMemo(() => {
    const get = (key: string) => metrics.find((m) => m.key === key)?.value;
    const fmt = (v: string | number | undefined) => {
      if (v === undefined) return "—";
      if (typeof v === "string") return v;
      return Number.isInteger(v) ? v.toLocaleString() : Math.round(v).toLocaleString();
    };
    return [
      { label: "Uptime", value: fmt(get("system.uptime.uptime")) },
      { label: "Boot time", value: fmt(get("system.uptime.boot")) },
      { label: "Processes", value: fmt(get("system.metrics.procs.procs")) },
      { label: "Threads", value: fmt(get("system.metrics.procs.threads")) },
      { label: "Handles", value: fmt(get("system.metrics.procs.handles")) },
      { label: "Worker jobs", value: fmt(get("workers.jobs")) },
      { label: "Worker submitted", value: fmt(get("workers.submitted")) },
      { label: "Worker errors", value: fmt(get("workers.errors")) },
      { label: "Worker threads", value: fmt(get("workers.threads")) },
      { label: "Data collected every (s)", value: fmt(get("workers.refresh_interval")) },
      { label: "CPU/Memory refreshed every (s)", value: fmt(get("system.refresh_interval")) },
      { label: "Network refreshed every (s)", value: fmt(get("system.network_refresh_interval")) },
    ];
  }, [metrics]);

  return (
    <Card variant="outlined">
      <CardContent>
        <Typography gutterBottom variant="h5" component="div">
          System Info
        </Typography>
        <Table size="small" sx={{ width: 500 }}>
          <TableBody>
            {systemInfo.map((row) => (
              <TableRow key={row.label}>
                <TableCell component="th" scope="row">
                  {row.label}
                </TableCell>
                <TableCell align="right">{row.value}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  );
}

