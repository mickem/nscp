import { useEffect, useMemo, useReducer, useRef } from "react";
import { Box, Card, CardContent, FormControl, InputLabel, MenuItem, Select, SelectChangeEvent } from "@mui/material";
import Typography from "@mui/material/Typography";
import { LineChart } from "@mui/x-charts/LineChart";
import { Metric } from "../metric_parser.ts";
import type { ComponentProps } from "react";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { setSelectedDisk } from "../common/dashboardSlice.ts";

function scaleUnit(bytesPerSec: number[]): { label: string; divisor: number } {
  const max = Math.max(...bytesPerSec);
  if (max >= 1024 * 1024) return { label: "MB/s", divisor: 1024 * 1024 };
  if (max >= 1024) return { label: "KB/s", divisor: 1024 };
  return { label: "B/s", divisor: 1 };
}

interface DiskIoHistory {
  readBytes: number[];
  writeBytes: number[];
}

type DiskIoAction =
  | { type: "push"; read?: number; write?: number; historySize: number }
  | { type: "reset"; historySize: number };

function diskIoReducer(state: DiskIoHistory, action: DiskIoAction): DiskIoHistory {
  switch (action.type) {
    case "push":
      return {
        readBytes:
          action.read !== undefined
            ? [...state.readBytes, action.read].slice(-action.historySize)
            : state.readBytes,
        writeBytes:
          action.write !== undefined
            ? [...state.writeBytes, action.write].slice(-action.historySize)
            : state.writeBytes,
      };
    case "reset":
      return {
        readBytes: Array(action.historySize).fill(0),
        writeBytes: Array(action.historySize).fill(0),
      };
  }
}

interface DiskIoWidgetProps {
  metrics: Metric[];
  fulfilledTimeStamp: number | undefined;
  xAxis: NonNullable<ComponentProps<typeof LineChart>["xAxis"]>;
  historySize: number;
}

export default function DiskIoWidget({ metrics, fulfilledTimeStamp, xAxis, historySize }: DiskIoWidgetProps) {
  const dispatch = useAppDispatch();
  const userSelectedDisk = useAppSelector((state) => state.dashboard.selectedDisk);
  const prevTimestampRef = useRef(fulfilledTimeStamp);
  const [history, dispatchHistory] = useReducer(diskIoReducer, historySize, (size) => ({
    readBytes: Array(size).fill(0),
    writeBytes: Array(size).fill(0),
  }));

  const diskDrives = useMemo(() => {
    return [
      ...new Set(
        metrics
          .filter((m) => m.module === "disk" && m.type === "io" && m.instance && m.instance !== "_Total")
          .map((m) => m.instance!),
      ),
    ].sort();
  }, [metrics]);

  const selectedDisk =
    userSelectedDisk && diskDrives.includes(userSelectedDisk)
      ? userSelectedDisk
      : diskDrives.length > 0
        ? diskDrives[0]
        : "";

  useEffect(() => {
    if (metrics.length > 0 && fulfilledTimeStamp !== prevTimestampRef.current) {
      prevTimestampRef.current = fulfilledTimeStamp;

      if (selectedDisk) {
        const ioFiltered = metrics.filter(
          (m) => m.module === "disk" && m.type === "io" && m.instance === selectedDisk,
        );
        const readMetric = ioFiltered.find((m) => m.metric === "read_bytes_per_sec");
        const writeMetric = ioFiltered.find((m) => m.metric === "write_bytes_per_sec");
        if (readMetric || writeMetric) {
          dispatchHistory({
            type: "push",
            read: readMetric ? (readMetric.value as number) : undefined,
            write: writeMetric ? (writeMetric.value as number) : undefined,
            historySize,
          });
        }
      }
    }
  }, [fulfilledTimeStamp, metrics, historySize, selectedDisk]);

  const handleDiskChange = (event: SelectChangeEvent) => {
    dispatch(setSelectedDisk(event.target.value));
    dispatchHistory({ type: "reset", historySize });
    prevTimestampRef.current = undefined;
  };

  if (diskDrives.length === 0) return null;

  const { label: unit, divisor } = scaleUnit([...history.readBytes, ...history.writeBytes]);
  const scaledRead = history.readBytes.map((v) => v / divisor);
  const scaledWrite = history.writeBytes.map((v) => v / divisor);

  return (
    <Card variant="outlined" sx={{ height: "100%" }}>
      <CardContent>
        <Box sx={{ display: "flex", alignItems: "center", gap: 2, mb: 1 }}>
          <Typography variant="h5" component="div" sx={{ flexShrink: 0 }}>
            Disk I/O
          </Typography>
          <FormControl size="small" sx={{ minWidth: 0, flexGrow: 1 }}>
            <InputLabel id="disk-select-label">Disk drive</InputLabel>
            <Select
              labelId="disk-select-label"
              value={selectedDisk}
              label="Disk drive"
              onChange={handleDiskChange}
              sx={{ "& .MuiSelect-select": { overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" } }}
            >
              {diskDrives.map((drive) => (
                <MenuItem key={drive} value={drive}>
                  {drive}
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
              { id: "Read", label: `Read (${unit})`, data: scaledRead, area: true },
              { id: "Write", label: `Write (${unit})`, data: scaledWrite, area: true },
            ]}
            grid={{ vertical: true, horizontal: true }}
            height={300}
          />
        </Box>
      </CardContent>
    </Card>
  );
}

