import { useState } from "react";
import { Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";

interface MemoryWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function MemoryWidget({ metrics, fulfilledTimeStamp, xAxis, historySize }: MemoryWidgetProps) {
  const [prevTimestamp, setPrevTimestamp] = useState(fulfilledTimeStamp);
  const [mem, setMem] = useState<number[]>(() => Array(historySize).fill(0));

  if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestamp) {
    setPrevTimestamp(fulfilledTimeStamp);

    const memUsed = metrics.find((m) => m.key === "system.mem.physical.used");
    const memTotal = metrics.find((m) => m.key === "system.mem.physical.total");
    if (memUsed && memTotal) {
      const pct = Math.round((1000 * (memUsed.value as number)) / (memTotal.value as number)) / 10;
      setMem((old) => [...old, pct].slice(-historySize));
    }
  }

  return (
    <Card variant="outlined">
      <CardContent>
        <Typography gutterBottom variant="h5" component="div">
          Memory usage
        </Typography>
        <LineChart
          xAxis={xAxis}
          yAxis={[{ min: 0, max: 100, label: "%" }]}
          series={[{ id: "Memory", label: "Memory (%)", data: mem, area: true }]}
          grid={{ vertical: true, horizontal: true }}
          width={500}
          height={300}
        />
      </CardContent>
    </Card>
  );
}
