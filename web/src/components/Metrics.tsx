import Stack from "@mui/material/Stack";
import { nsclientApi, useGetMetricsQuery } from "../api/api.ts";
import {
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  ToggleButton,
  ToggleButtonGroup,
  Typography,
} from "@mui/material";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { useMemo, useState } from "react";
import CloseIcon from "@mui/icons-material/Close";
import { parseMetrics } from "../metric_parser.ts";
import FilterField from "./atoms/FilterField.tsx";

export default function Metrics() {
  const dispatch = useAppDispatch();
  const [filter, setFilter] = useState<string>("");
  const { data: metrics } = useGetMetricsQuery();

  const result = useMemo(() => parseMetrics(metrics), [metrics]);

  const needle = filter.trim().toLowerCase();
  const filteredMetrics = useMemo(() => {
    if (!needle) return result.metrics;
    return result.metrics.filter((m) =>
      [m.key, m.module, m.type, m.instance, m.metric].some((f) => (f ?? "").toLowerCase().includes(needle)),
    );
  }, [result.metrics, needle]);

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Metrics"]));
  };

  const sortedMetrics = useMemo(
    () => [...filteredMetrics].sort((a, b) => a.key.localeCompare(b.key)),
    [filteredMetrics],
  );

  return (
    <Stack direction="column">
      <Toolbar>
        <ToggleButtonGroup
          size="small"
          value={filter}
          exclusive
          onChange={(_e, f) => setFilter(f ?? "")}
          aria-label="filter by module"
          sx={{ "& .MuiToggleButton-root": { px: 1, py: 0.5 } }}
        >
          {result.modules.map((m) => (
            <ToggleButton key={m} value={m}>
              {m}
            </ToggleButton>
          ))}
          <ToggleButton value="">
            <CloseIcon fontSize="small" />
          </ToggleButton>
        </ToggleButtonGroup>
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter metrics…" />
        {needle && (
          <Typography variant="body2" color="text.secondary">
            {filteredMetrics.length}/{result.metrics.length}
          </Typography>
        )}
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <TableContainer sx={{ width: "100%" }}>
        <Table>
          <TableHead>
            <TableRow>
              <TableCell>Module</TableCell>
              <TableCell>Type</TableCell>
              <TableCell>Instance</TableCell>
              <TableCell>Metric</TableCell>
              <TableCell align="right">Value</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {sortedMetrics.map((m) => (
              <TableRow hover key={m.key} sx={{ "&:last-child td, &:last-child th": { border: 0 } }}>
                <TableCell>{m.module}</TableCell>
                <TableCell>{m.type}</TableCell>
                <TableCell>{m.instance}</TableCell>
                <TableCell>{m.metric}</TableCell>
                <TableCell align="right">{m.value}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </TableContainer>
    </Stack>
  );
}
