import { useEffect, useReducer, useRef } from "react";
import { Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";

interface CpuHistory {
  kernel: number[];
  user: number[];
}

type CpuAction = { type: "push"; kernel?: number; user?: number; historySize: number };

function cpuReducer(state: CpuHistory, action: CpuAction): CpuHistory {
  return {
    kernel:
      action.kernel !== undefined
        ? [...state.kernel, action.kernel].slice(-action.historySize)
        : state.kernel,
    user:
      action.user !== undefined
        ? [...state.user, action.user].slice(-action.historySize)
        : state.user,
  };
}

interface CpuWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function CpuWidget({ metrics, fulfilledTimeStamp, xAxis, historySize }: CpuWidgetProps) {
  const prevTimestampRef = useRef(fulfilledTimeStamp);
  const [history, dispatch] = useReducer(cpuReducer, historySize, (size) => ({
    kernel: Array(size).fill(0),
    user: Array(size).fill(0),
  }));

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      const kernelMetric = metrics.find((m) => m.key === "system.cpu.total.kernel");
      const userMetric = metrics.find((m) => m.key === "system.cpu.total.user");
      if (kernelMetric || userMetric) {
        dispatch({
          type: "push",
          kernel: kernelMetric ? (kernelMetric.value as number) : undefined,
          user: userMetric ? (userMetric.value as number) : undefined,
          historySize,
        });
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
            { id: "Kernel", label: "Kernel time (%)", data: history.kernel, stack: "total", area: true },
            { id: "User", label: "User time (%)", data: history.user, stack: "total", area: true },
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
