import Stack from "@mui/material/Stack";
import { nsclientApi, useGetMetricsQuery } from "../api/api.ts";
import {
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  TextField,
  ToggleButton,
  ToggleButtonGroup,
} from "@mui/material";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { useState } from "react";
import CloseIcon from "@mui/icons-material/Close";
import { parseMetrics } from "../metric_parser.ts";
import Divider from "@mui/material/Divider";

export default function Metrics() {
  const dispatch = useAppDispatch();
  const [filter, setFilter] = useState<string>("");
  const { data: metrics } = useGetMetricsQuery();

  const result = parseMetrics(metrics);
  const filteredMetrics = result.metrics.filter((m) => filter === "" || m.key.includes(filter));

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Metrics"]));
  };

  const sortedMetrics = filteredMetrics.sort((a, b) => a.key.localeCompare(b.key));

  return (
    <Stack direction="column">
      <Toolbar>
        <Divider orientation="vertical" flexItem />
        <TextField
          label="Filter"
          variant="standard"
          size="small"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
        />
        <ToggleButtonGroup
          size="small"
          value={filter}
          exclusive
          onChange={(_e, f) => setFilter(f)}
          aria-label="text alignment"
        >
          {result.modules.map((m) => (
            <ToggleButton key={m} value={m}>
              {m}
            </ToggleButton>
          ))}
          <ToggleButton value="">
            <CloseIcon />
          </ToggleButton>
        </ToggleButtonGroup>
        <Spacing />
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
