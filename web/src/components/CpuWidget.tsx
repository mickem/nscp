import { useEffect, useRef } from "react";
import { Box, Card, CardContent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { pushCpu } from "../common/dashboardSlice.ts";

interface CpuWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function CpuWidget({ metrics, fulfilledTimeStamp, xAxis }: CpuWidgetProps) {
  const dispatch = useAppDispatch();
  const history = useAppSelector((state) => state.dashboard.cpuHistory);
  const prevTimestampRef = useRef(fulfilledTimeStamp);

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      const kernelMetric = metrics.find((m) => m.key === "system.cpu.total.kernel");
      const userMetric = metrics.find((m) => m.key === "system.cpu.total.user");
      if (kernelMetric || userMetric) {
        dispatch(pushCpu({
          kernel: kernelMetric ? (kernelMetric.value as number) : undefined,
          user: userMetric ? (userMetric.value as number) : undefined,
        }));
      }
    }
  }, [fulfilledTimeStamp, metrics, dispatch]);

  return (
    <Card variant="outlined" sx={{ height: "100%" }}>
      <CardContent>
        <Typography gutterBottom variant="h5" component="div">
          CPU Load
        </Typography>
        <Box sx={{ width: "100%" }}>
          <LineChart
            xAxis={xAxis}
            series={[
              { id: "Kernel", label: "Kernel time (%)", data: history.kernel, stack: "total", area: true },
              { id: "User", label: "User time (%)", data: history.user, stack: "total", area: true },
            ]}
            grid={{ vertical: true, horizontal: true }}
            height={300}
            yAxis={[{ min: 0, max: 100, label: "%" }]}
          />
        </Box>
      </CardContent>
    </Card>
  );
}
