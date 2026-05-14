import { useEffect, useMemo, useRef } from "react";
import { Box, Card, CardContent, FormControl, InputLabel, MenuItem, Select, SelectChangeEvent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { pushNetwork, setSelectedNic } from "../common/dashboardSlice.ts";

function scaleUnit(bytesPerSec: number[]): { label: string; divisor: number } {
  const max = Math.max(...bytesPerSec);
  if (max >= 1024 * 1024) return { label: "MB/s", divisor: 1024 * 1024 };
  if (max >= 1024) return { label: "KB/s", divisor: 1024 };
  return { label: "B/s", divisor: 1 };
}

interface NetworkWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function NetworkWidget({ metrics, fulfilledTimeStamp, xAxis }: NetworkWidgetProps) {
  const dispatch = useAppDispatch();
  const userSelectedNic = useAppSelector((state) => state.dashboard.selectedNic);
  const history = useAppSelector((state) => state.dashboard.networkHistory);
  const prevTimestampRef = useRef(fulfilledTimeStamp);

  const networkCards = useMemo(() => {
    const cards = new Map<string, string>();
    metrics
      .filter((m) => m.type === "network" && m.instance && m.metric === "NetConnectionID")
      .forEach((m) => cards.set(m.instance!, m.value as string));
    return cards;
  }, [metrics]);

  const selectedNic =
    userSelectedNic && networkCards.has(userSelectedNic)
      ? userSelectedNic
      : networkCards.size > 0
        ? Array.from(networkCards.keys())[0]
        : "";

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      if (selectedNic) {
        const networkFiltered = metrics.filter((m) => m.type === "network");
        const receivedMetric = networkFiltered.find(
          (m) => m.instance === selectedNic && m.metric === "BytesReceivedPersec",
        );
        const sentMetric = networkFiltered.find(
          (m) => m.instance === selectedNic && m.metric === "BytesSentPersec",
        );
        if (receivedMetric || sentMetric) {
          dispatch(pushNetwork({
            received: receivedMetric ? (receivedMetric.value as number) : undefined,
            sent: sentMetric ? (sentMetric.value as number) : undefined,
          }));
        }
      }
    }
  }, [fulfilledTimeStamp, metrics, selectedNic, dispatch]);

  const handleNicChange = (event: SelectChangeEvent) => {
    dispatch(setSelectedNic(event.target.value));
    prevTimestampRef.current = undefined;
  };

  if (networkCards.size === 0) return null;

  const { label: unit, divisor } = scaleUnit([...history.bytesReceived, ...history.bytesSent]);
  const scaledReceived = history.bytesReceived.map((v) => v / divisor);
  const scaledSent = history.bytesSent.map((v) => v / divisor);

  return (
    <Card variant="outlined" sx={{ height: "100%" }}>
      <CardContent>
        <Box sx={{ display: "flex", alignItems: "center", gap: 2, mb: 1 }}>
          <Typography variant="h5" component="div" sx={{ flexShrink: 0 }}>
            Network
          </Typography>
          <FormControl size="small" sx={{ minWidth: 0, flexGrow: 1 }}>
            <InputLabel id="nic-select-label">Network adapter</InputLabel>
            <Select
              labelId="nic-select-label"
              value={selectedNic}
              label="Network adapter"
              onChange={handleNicChange}
              sx={{ "& .MuiSelect-select": { overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" } }}
            >
              {Array.from(networkCards.entries()).map(([instance, friendlyName]) => (
                <MenuItem key={instance} value={instance}>
                  {friendlyName} ({instance})
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>
        <Box sx={{ width: "100%" }}>
          <LineChart
            xAxis={xAxis}
            yAxis={[{ label: unit }]}
            series={[
              { id: "Received", label: `Received (${unit})`, data: scaledReceived, area: true },
              { id: "Sent", label: `Sent (${unit})`, data: scaledSent, area: true },
            ]}
            grid={{ vertical: true, horizontal: true }}
            height={300}
          />
        </Box>
      </CardContent>
    </Card>
  );
}
