import { useEffect, useReducer, useRef } from "react";
import { Box, Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";

type MemAction = { type: "push"; value: number; historySize: number };

function memReducer(state: number[], action: MemAction): number[] {
  return [...state, action.value].slice(-action.historySize);
}

interface MemoryWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function MemoryWidget({ metrics, fulfilledTimeStamp, xAxis, historySize }: MemoryWidgetProps) {
  const prevTimestampRef = useRef(fulfilledTimeStamp);
  const [mem, dispatch] = useReducer(memReducer, historySize, (size) => Array(size).fill(0));

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      const memUsed = metrics.find((m) => m.key === "system.mem.physical.used");
      const memTotal = metrics.find((m) => m.key === "system.mem.physical.total");
      if (memUsed && memTotal) {
        const pct = Math.round((1000 * (memUsed.value as number)) / (memTotal.value as number)) / 10;
        dispatch({ type: "push", value: pct, historySize });
      }
    }
  }, [fulfilledTimeStamp, metrics, historySize]);

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
