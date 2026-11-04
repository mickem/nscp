import { Card, CardContent, Link, Stack, Table, TableBody, TableCell, TableRow, Typography } from "@mui/material";
import Grid from "@mui/material/Grid";
import { Link as RouterLink } from "react-router";
import { useGetFailuresQuery } from "../api/api.ts";
import { useAppSelector } from "../store/store.ts";
import { QueryResultChip } from "./atoms/QueryResultChip.tsx";
import { formatAge } from "../common/formatAge.ts";

/** How many of the latest failures the dashboard shows; the page has the rest. */
const PREVIEW_FAILURES = 5;

/**
 * The latest entries of the core's failed-check log: every query, from any
 * transport, that came back WARNING, CRITICAL or UNKNOWN. Renders its own
 * grid cell so the dashboard shows no gap while nothing has failed, and
 * polls at the dashboard's refresh rate like the other widgets.
 */
export default function FailedChecksWidget() {
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data } = useGetFailuresQuery({ limit: PREVIEW_FAILURES }, { pollingInterval: refreshRate || undefined });

  const entries = data?.entries ?? [];
  if (entries.length === 0) return null;

  return (
    <Grid size={{ xs: 12, md: 6 }}>
      <Card variant="outlined" sx={{ height: "100%" }}>
        <CardContent>
          <Stack direction="row" sx={{ alignItems: "baseline", justifyContent: "space-between" }}>
            <Typography gutterBottom variant="h5" component="div">
              Failed checks
            </Typography>
            <Link component={RouterLink} to="/failures" variant="body2">
              Show all
            </Link>
          </Stack>
          <Table size="small" aria-label="latest failed checks">
            <TableBody>
              {entries.map((entry) => (
                <TableRow key={entry.index}>
                  <TableCell sx={{ width: 100, pl: 0 }}>
                    <QueryResultChip result={entry.status} />
                  </TableCell>
                  <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>{entry.command}</TableCell>
                  <TableCell>
                    <Typography
                      variant="body2"
                      sx={{ maxWidth: 360, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}
                    >
                      {entry.message || "—"}
                    </Typography>
                  </TableCell>
                  <TableCell sx={{ whiteSpace: "nowrap", color: "text.secondary", pr: 0 }} align="right">
                    {formatAge(entry.age)}
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </CardContent>
      </Card>
    </Grid>
  );
}
