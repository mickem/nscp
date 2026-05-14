import { useEffect, useRef } from "react";
import { Box, Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { pushMem } from "../common/dashboardSlice.ts";

interface MemoryWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function MemoryWidget({ metrics, fulfilledTimeStamp, xAxis }: MemoryWidgetProps) {
  const dispatch = useAppDispatch();
  const mem = useAppSelector((state) => state.dashboard.memHistory);
  const prevTimestampRef = useRef(fulfilledTimeStamp);

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      const memUsed = metrics.find((m) => m.key === "system.mem.physical.used");
      const memTotal = metrics.find((m) => m.key === "system.mem.physical.total");
      if (memUsed && memTotal) {
        const pct = Math.round((1000 * (memUsed.value as number)) / (memTotal.value as number)) / 10;
        dispatch(pushMem(pct));
      }
    }
  }, [fulfilledTimeStamp, metrics, dispatch]);

  return (
    <Card variant="outlined" sx={{ height: "100%" }}>
      <CardContent>
        <Typography gutterBottom variant="h5" component="div">
          Memory Usage
        </Typography>
        <Box sx={{ width: "100%" }}>
          <LineChart
            xAxis={xAxis}
            yAxis={[{ min: 0, max: 100, label: "%" }]}
            series={[{ id: "Memory", label: "Memory (%)", data: mem, area: true }]}
            grid={{ vertical: true, horizontal: true }}
            height={300}
          />
        </Box>
      </CardContent>
    </Card>
  );
}
