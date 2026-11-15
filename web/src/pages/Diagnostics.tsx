import {
  Alert,
  Box,
  Card,
  CardContent,
  Chip,
  FormControl,
  InputLabel,
  MenuItem,
  Select,
  SelectChangeEvent,
  Stack,
} from "@mui/material";
import { useState } from "react";
import {
  DiagnosticsSectionSummary,
  nsclientApi,
  useGetDiagnosticsQuery,
  useGetDiagnosticsSectionQuery,
} from "../api/api.ts";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import Trail from "../components/atoms/Trail.tsx";
import DiagnosticsTable from "../components/DiagnosticsTable.tsx";
import { formatTimestamp, orderSectionNames } from "../common/diagnostics.ts";
import { useAppDispatch } from "../store/store.ts";

const REFRESH_RATES = [
  { label: "Off", value: 0 },
  { label: "1 second", value: 1000 },
  { label: "5 seconds", value: 5000 },
  { label: "10 seconds", value: 10000 },
  { label: "30 seconds", value: 30000 },
  { label: "1 minute", value: 60000 },
];

/** How to turn diagnostics on, for the page to show when they are off. */
const ENABLE_SAMPLE = `[/settings/core/failed checks]
capture diagnostics = true

; Then switch on the tables you want in the modules that produce them:
[/settings/system/windows/diagnostics]   ; CheckSystem (Windows)
processes = true
services = true

[/settings/eventlog/diagnostics]         ; CheckEventLog (Windows)
services = true
errors = true

[/settings/system/unix/diagnostics]      ; CheckSystemUnix (Linux)
processes = true
services = true
errors = true

[/settings/disk/diagnostics]             ; CheckDisk
growth spots = true`;

function errorStatus(error: unknown): number | undefined {
  if (error && typeof error === "object" && "status" in error) {
    const status = (error as { status: unknown }).status;
    return typeof status === "number" ? status : undefined;
  }
  return undefined;
}

/**
 * Diagnostics, live: the tables of the latest diagnostics snapshot - the top
 * processes, the services in trouble, the last log errors, the growth spots
 * and the last failed checks - as the agent collected them on its last round.
 * Nothing is collected because the page asked; it shows what is held.
 *
 * The list route carries each section's summary but not its rows, so each
 * section is fetched on its own. The fetch is keyed on the snapshot id, so a
 * poll that finds the same snapshot costs one request, not one per table.
 */
export default function Diagnostics() {
  const dispatch = useAppDispatch();
  const [refreshRate, setRefreshRate] = useState<number>(0);
  const { data, error } = useGetDiagnosticsQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Diagnostics"]));
  };

  const status = errorStatus(error);
  const sections = data?.sections ?? [];
  const byName = new Map(sections.map((s) => [s.name, s]));
  const names = orderSectionNames(sections.map((s) => s.name));

  return (
    <Stack direction="column" sx={{ width: "100%" }}>
      <Toolbar>
        <Trail title="Diagnostics" />
        {data?.enabled && data.snapshot_id > 0 && (
          <>
            <Chip size="small" variant="outlined" label={`snapshot #${data.snapshot_id}`} />
            <Chip
              size="small"
              variant="outlined"
              label={`collected ${formatTimestamp(data.captured_at)}`}
              title="When the agent last asked its modules for diagnostics; each table says when it was captured"
            />
          </>
        )}
        <Spacing />
        <FormControl size="small" sx={{ minWidth: 140 }}>
          <InputLabel id="diagnostics-refresh-rate">Refresh</InputLabel>
          <Select
            labelId="diagnostics-refresh-rate"
            value={refreshRate}
            label="Refresh"
            onChange={(e: SelectChangeEvent<number>) => setRefreshRate(e.target.value as number)}
          >
            {REFRESH_RATES.map((r) => (
              <MenuItem key={r.value} value={r.value}>
                {r.label}
              </MenuItem>
            ))}
          </Select>
        </FormControl>
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>

      {status === 403 && (
        <Alert severity="warning" sx={{ mx: 2, mb: 2 }}>
          Reading diagnostics needs the <code>diagnostics.list</code> privilege, which your role does not have. No
          built-in role holds it other than <code>full</code>; add it to a role under{" "}
          <code>[/settings/WEB/server/roles]</code>.
        </Alert>
      )}
      {error !== undefined && status !== 403 && (
        <Alert severity="error" sx={{ mx: 2, mb: 2 }}>
          Could not read the diagnostics{status ? ` (HTTP ${status})` : ""}.
        </Alert>
      )}

      {data && !data.enabled && (
        <Alert severity="info" sx={{ mx: 2, mb: 2 }}>
          Diagnostics are switched off. Set <code>capture diagnostics = true</code> under{" "}
          <code>[/settings/core/failed checks]</code> to collect them with every metrics round (or every{" "}
          <code>diagnostics interval</code> under <code>[/settings/core]</code>), keep them with each failed check and
          show them here. That alone gives the last failed checks; each other table is switched on under the{" "}
          <code>diagnostics</code> section of the module that produces it:
          <Box component="pre" sx={{ mt: 1, mb: 0, fontSize: "0.8rem", overflowX: "auto" }}>
            {ENABLE_SAMPLE}
          </Box>
        </Alert>
      )}

      {data?.enabled && data.snapshot_id === 0 && (
        <Alert severity="info" sx={{ mx: 2, mb: 2 }}>
          No diagnostics collected yet: the first round runs with the next metrics round.
        </Alert>
      )}

      {data?.enabled && (data.excluded.length > 0 || data.dropped > 0) && (
        <Alert severity="info" sx={{ mx: 2, mb: 2 }}>
          {data.excluded.length > 0 && (
            <div>
              Left out by <code>prune pattern</code>:{" "}
              {data.excluded.map((name, i) => (
                <span key={name}>
                  {i > 0 && ", "}
                  <code>{name}</code>
                </span>
              ))}
              .
            </div>
          )}
          {data.dropped > 0 && (
            <div>
              {data.dropped} {data.dropped === 1 ? "table was" : "tables were"} dropped: over the 16-table cap, or a
              second table of the same name.
            </div>
          )}
        </Alert>
      )}

      {data?.enabled && (
        <Stack spacing={2} sx={{ px: 2, pb: 2 }}>
          {names.map((name) => (
            <Card key={name} variant="outlined">
              <CardContent>
                <LiveSection summary={byName.get(name)!} snapshotId={data.snapshot_id} />
              </CardContent>
            </Card>
          ))}
        </Stack>
      )}
    </Stack>
  );
}

interface LiveSectionProps {
  summary: DiagnosticsSectionSummary;
  snapshotId: number;
}

function LiveSection({ summary, snapshotId }: LiveSectionProps) {
  const { data, error } = useGetDiagnosticsSectionQuery({ name: summary.name, snapshotId });
  if (data) {
    return <DiagnosticsTable name={summary.name} section={data} />;
  }
  // Until the rows arrive (or if the section left the snapshot in between),
  // the summary from the list is still worth showing.
  const status = errorStatus(error);
  return (
    <DiagnosticsTable
      name={summary.name}
      section={{
        source: summary.source,
        description: summary.description,
        captured_at: summary.captured_at,
        truncated: false,
        error:
          summary.error ??
          (error === undefined
            ? undefined
            : status === 404
              ? "no longer in the latest snapshot"
              : `could not be read${status ? ` (HTTP ${status})` : ""}`),
        columns: [],
        rows: [],
      }}
      loading={error === undefined && !summary.error}
    />
  );
}
