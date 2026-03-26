import { useEffect, useRef, useState } from "react";
import { Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";

interface CpuWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function CpuWidget({ metrics, fulfilledTimeStamp, xAxis, historySize }: CpuWidgetProps) {
  const prevTimestampRef = useRef(fulfilledTimeStamp);
  const [kernelCpu, setKernelCpu] = useState<number[]>(() => Array(historySize).fill(0));
  const [userCpu, setUserCpu] = useState<number[]>(() => Array(historySize).fill(0));

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      const kernelCpuMetric = metrics.find((m) => m.key === "system.cpu.total.kernel");
      if (kernelCpuMetric) {
        setKernelCpu((old) => [...old, kernelCpuMetric.value as number].slice(-historySize));
      }
      const userCpuMetric = metrics.find((m) => m.key === "system.cpu.total.user");
      if (userCpuMetric) {
        setUserCpu((old) => [...old, userCpuMetric.value as number].slice(-historySize));
      }
    }
  }, [fulfilledTimeStamp, metrics, historySize]);

  return (
    <Card variant="outlined">
      <CardContent>
        <Typography gutterBottom variant="h5" component="div">
          CPU Load
        </Typography>
        <LineChart
          xAxis={xAxis}
          series={[
            { id: "Kernel", label: "Kernel time (%)", data: kernelCpu, stack: "total", area: true },
            { id: "User", label: "User time (%)", data: userCpu, stack: "total", area: true },
          ]}
          grid={{ vertical: true, horizontal: true }}
          width={500}
          height={300}
          yAxis={[{ min: 0, max: 100, label: "%" }]}
        />
      </CardContent>
    </Card>
  );
}
