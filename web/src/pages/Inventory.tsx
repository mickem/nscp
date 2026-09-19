import Stack from "@mui/material/Stack";
import {
  Alert,
  AlertTitle,
  Card,
  CardContent,
  Chip,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from "@mui/material";
import { useMemo, useState } from "react";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import FilterField from "../components/atoms/FilterField.tsx";
import {
  FactValue,
  useGetFactsQuery,
  useRefreshFactsMutation,
} from "../api/api.ts";

// The INI an operator pastes to turn a fact set on. Shown verbatim on the
// empty state, because "nothing is collected until you enable a set" is the
// default every install is in and an empty page would read as a bug.
const ENABLE_EXAMPLE = `[/settings/facts]
os = true
storage.volumes = true`;

function isRecordList(value: FactValue): value is { [key: string]: FactValue }[] {
  return (
    Array.isArray(value) &&
    value.length > 0 &&
    value.every((entry) => entry !== null && typeof entry === "object" && !Array.isArray(entry))
  );
}

function renderScalar(value: FactValue): string {
  if (Array.isArray(value)) return value.map(renderScalar).join(", ");
  if (value !== null && typeof value === "object") return JSON.stringify(value);
  return String(value);
}

/** The column order of a record table: `id` first, then the fields as found. */
function columnsOf(records: { [key: string]: FactValue }[]): string[] {
  const seen: string[] = [];
  for (const record of records) {
    for (const key of Object.keys(record)) {
      if (!seen.includes(key)) seen.push(key);
    }
  }
  return ["id", ...seen.filter((key) => key !== "id")];
}

function RecordTable({ records }: { records: { [key: string]: FactValue }[] }) {
  const columns = columnsOf(records);
  return (
    <TableContainer sx={{ width: "100%" }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            {columns.map((column) => (
              <TableCell key={column}>{column}</TableCell>
            ))}
          </TableRow>
        </TableHead>
        <TableBody>
          {records.map((record, index) => (
            <TableRow hover key={renderScalar(record.id ?? index)}>
              {columns.map((column) => (
                <TableCell key={column}>
                  {record[column] === undefined ? "" : renderScalar(record[column])}
                </TableCell>
              ))}
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
  );
}

/**
 * An object's scalars as a definition list, with nested objects and lists
 * below it. A fact set is shallow by construction (the core caps it at six
 * levels), so this recursion is bounded.
 */
function FactObject({
  value,
  prefix,
}: {
  value: { [key: string]: FactValue };
  prefix: string;
}) {
  const entries = Object.entries(value);
  const scalars = entries.filter(([, v]) => v === null || typeof v !== "object" || (Array.isArray(v) && !isRecordList(v)));
  const nested = entries.filter(([, v]) => v !== null && typeof v === "object" && (!Array.isArray(v) || isRecordList(v)));

  return (
    <Stack direction="column" spacing={1}>
      {scalars.length > 0 && (
        <TableContainer sx={{ width: "100%" }}>
          <Table size="small">
            <TableBody>
              {scalars.map(([key, scalar]) => (
                <TableRow hover key={`${prefix}.${key}`}>
                  <TableCell sx={{ width: "30%", color: "text.secondary" }}>{key}</TableCell>
                  <TableCell>{renderScalar(scalar)}</TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </TableContainer>
      )}
      {nested.map(([key, child]) => (
        <Stack direction="column" key={`${prefix}.${key}`} spacing={0.5} sx={{ pt: 1 }}>
          <Typography variant="subtitle2" color="text.secondary">
            {key}
            {Array.isArray(child) ? ` (${child.length})` : ""}
          </Typography>
          {isRecordList(child) ? (
            <RecordTable records={child} />
          ) : Array.isArray(child) ? (
            <Typography variant="body2">{child.length === 0 ? "(none)" : renderScalar(child)}</Typography>
          ) : (
            <FactObject value={child as { [key: string]: FactValue }} prefix={`${prefix}.${key}`} />
          )}
        </Stack>
      ))}
    </Stack>
  );
}

function FactSetCard({ name, value }: { name: string; value: FactValue }) {
  return (
    <Card variant="outlined">
      <CardContent>
        <Typography variant="h6" gutterBottom>
          {name}
        </Typography>
        {isRecordList(value) ? (
          <RecordTable records={value} />
        ) : value !== null && typeof value === "object" && !Array.isArray(value) ? (
          <FactObject value={value as { [key: string]: FactValue }} prefix={name} />
        ) : (
          <Typography variant="body2">{renderScalar(value)}</Typography>
        )}
      </CardContent>
    </Card>
  );
}

export default function Inventory() {
  const [filter, setFilter] = useState<string>("");
  const { data: facts, isFetching } = useGetFactsQuery();
  const [refreshFacts, { isLoading: isRefreshing }] = useRefreshFactsMutation();

  const needle = filter.trim().toLowerCase();
  const sets = useMemo(() => {
    const all = Object.entries(facts?.facts ?? {});
    if (!needle) return all;
    // Match the set name or anything in it, so "ext4" finds the volume list
    // without the user knowing which set holds it.
    return all.filter(
      ([name, value]) =>
        name.toLowerCase().includes(needle) || JSON.stringify(value).toLowerCase().includes(needle),
    );
  }, [facts, needle]);

  const errors = Object.entries(facts?.errors ?? {});
  const nothingEnabled = (facts?.enabled?.length ?? 0) === 0;

  return (
    <Stack direction="column" spacing={2}>
      <Toolbar>
        {(facts?.enabled ?? []).map((name) => (
          <Chip key={name} label={name} size="small" />
        ))}
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter inventory…" />
        {facts && (
          <Typography variant="body2" color="text.secondary">
            revision {facts.revision}
          </Typography>
        )}
        <RefreshButton onRefresh={() => void refreshFacts()} isFetching={isFetching || isRefreshing} />
      </Toolbar>

      {errors.length > 0 && (
        <Alert severity="warning">
          <AlertTitle>Some fact sets could not be collected</AlertTitle>
          {errors.map(([name, message]) => (
            <Typography variant="body2" key={name}>
              <strong>{name}</strong>: {message}
            </Typography>
          ))}
        </Alert>
      )}

      {nothingEnabled && (
        <Alert severity="info">
          <AlertTitle>No inventory is being collected</AlertTitle>
          <Typography variant="body2" gutterBottom>
            Facts are opt-in: this agent collects nothing about the host until a fact set is
            enabled. Add the sets you want to the configuration, or enable them for a whole group
            from the fleet server.
          </Typography>
          <Typography component="pre" variant="body2" sx={{ mt: 1, fontFamily: "monospace" }}>
            {ENABLE_EXAMPLE}
          </Typography>
          <Typography variant="body2" sx={{ mt: 1 }}>
            The sets this agent can collect, and what each one costs, are listed by{" "}
            <code>nscp test</code> → <code>facts list</code>.
          </Typography>
        </Alert>
      )}

      {!nothingEnabled && sets.length === 0 && (
        <Typography variant="body2" color="text.secondary">
          {needle ? "Nothing in the inventory matches that filter." : "No facts have been collected yet."}
        </Typography>
      )}

      {sets.map(([name, value]) => (
        <FactSetCard key={name} name={name} value={value} />
      ))}

      {facts && !nothingEnabled && (
        <Typography variant="caption" color="text.secondary">
          Document hash {facts.hash.slice(0, 12)} — the fleet server compares this to tell whether
          the inventory changed.
        </Typography>
      )}
    </Stack>
  );
}
