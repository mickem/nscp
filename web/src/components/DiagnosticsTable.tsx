import { useState } from "react";
import {
  Alert,
  Box,
  Chip,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Tooltip,
  Typography,
} from "@mui/material";
import { DiagnosticsColumn, DiagnosticsSection } from "../api/api.ts";
import { formatAge } from "../common/formatAge.ts";
import { DiagnosticsCell, formatCell } from "../common/diagnostics.ts";
import { useNow } from "../common/hooks/useNow.ts";

/**
 * One diagnostics section - a small point-in-time table a module produced
 * (the top processes, the services in trouble, the last log errors) - as the
 * failed-check context and the live /api/v2/diagnostics routes serve it.
 *
 * The rows are objects keyed by column name; they are laid out in the order
 * the section lists its columns, and each cell is formatted by its column's
 * type and unit.
 */

/** A text cell longer than this is clamped to two lines until clicked. */
const CLAMP_LENGTH = 80;

type Cell = DiagnosticsCell;

function isNumeric(column: DiagnosticsColumn): boolean {
  return column.type === "int" || column.type === "float";
}

/** What a column header's tooltip says: its description and unit. */
function columnHint(column: DiagnosticsColumn): string {
  const unit = column.unit ? `(${column.unit})` : "";
  return [column.description ?? "", unit].filter((s) => s).join(" ");
}

/** Long event text and paths: two lines until clicked, the full text always in the title. */
function LongText({ text }: { text: string }) {
  const [open, setOpen] = useState(false);
  return (
    <Box
      component="span"
      title={text}
      onClick={() => setOpen((o) => !o)}
      sx={{
        cursor: "pointer",
        overflowWrap: "anywhere",
        ...(open
          ? { whiteSpace: "pre-wrap" }
          : { display: "-webkit-box", WebkitLineClamp: 2, WebkitBoxOrient: "vertical", overflow: "hidden" }),
      }}
    >
      {text}
    </Box>
  );
}

function CellContent({ column, value }: { column: DiagnosticsColumn; value: Cell }) {
  const text = formatCell(column, value);
  if (value === null || value === undefined) {
    return (
      <Typography component="span" variant="body2" color="text.secondary">
        {text}
      </Typography>
    );
  }
  if (typeof value === "string" && column.type === "string" && value.length > CLAMP_LENGTH) {
    return <LongText text={value} />;
  }
  if (column.type === "timestamp" && typeof value === "string") {
    return <span title={value}>{text}</span>;
  }
  return <>{text}</>;
}

interface DiagnosticsTableProps {
  name: string;
  section: DiagnosticsSection;
  /** "Now", in epoch milliseconds, for the relative age; injectable for tests. */
  now?: number;
  /** The rows are still on their way: say so instead of "no rows". */
  loading?: boolean;
}

export default function DiagnosticsTable({ name, section, now, loading = false }: DiagnosticsTableProps) {
  const clock = useNow();
  const captured = new Date(section.captured_at);
  const hasTime = !isNaN(captured.getTime());
  const age = hasTime ? Math.max(0, ((now ?? clock) - captured.getTime()) / 1000) : undefined;
  const rows = section.rows ?? [];
  const columns = section.columns ?? [];

  return (
    <Stack spacing={0.5} component="section" aria-label={name}>
      <Stack direction="row" spacing={1} useFlexGap sx={{ alignItems: "center", flexWrap: "wrap" }}>
        <Typography variant="subtitle2" component="h3" sx={{ fontFamily: "monospace" }}>
          {name}
        </Typography>
        {section.source && (
          <Chip size="small" variant="outlined" label={section.source} title="The module that produced it" />
        )}
        {section.truncated && rows.length > 0 && (
          <Chip
            size="small"
            color="warning"
            variant="outlined"
            label={`showing the first ${rows.length}`}
            title="A row, cell or size cap cut this table: there was more"
          />
        )}
        {hasTime && (
          <Typography variant="caption" color="text.secondary" title={section.captured_at}>
            captured {formatAge(age ?? 0)} · {captured.toLocaleString()}
          </Typography>
        )}
      </Stack>
      {section.description && (
        <Typography variant="body2" color="text.secondary">
          {section.description}
        </Typography>
      )}
      {/* An error does not rule out rows: a producer reading two logs reports
          the one that failed and still carries the other's rows. */}
      {section.error && (
        <Alert severity="warning" variant="outlined">
          {rows.length === 0 ? "Could not be collected" : "Not all of it could be collected"}: {section.error}
        </Alert>
      )}
      {section.error && rows.length === 0 ? null : rows.length === 0 ? (
        <Typography variant="body2" color="text.secondary">
          {loading ? "Loading…" : "No rows: nothing to report when this was captured."}
        </Typography>
      ) : (
        <TableContainer sx={{ maxWidth: "100%", overflowX: "auto" }}>
          <Table size="small" aria-label={`${name} rows`}>
            <TableHead>
              <TableRow>
                {columns.map((c) => {
                  const hint = columnHint(c);
                  const label = (
                    <span>
                      {c.name}
                      {c.unit && (
                        <Typography component="span" variant="caption" color="text.secondary" sx={{ ml: 0.5 }}>
                          {c.unit === "percent" ? "%" : c.unit}
                        </Typography>
                      )}
                    </span>
                  );
                  return (
                    <TableCell
                      key={c.name}
                      align={isNumeric(c) ? "right" : "left"}
                      sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}
                    >
                      {hint ? (
                        <Tooltip title={hint} placement="top">
                          {label}
                        </Tooltip>
                      ) : (
                        label
                      )}
                    </TableCell>
                  );
                })}
              </TableRow>
            </TableHead>
            <TableBody>
              {rows.map((row, i) => (
                <TableRow key={i}>
                  {columns.map((c) => (
                    <TableCell
                      key={c.name}
                      align={isNumeric(c) ? "right" : "left"}
                      sx={{
                        verticalAlign: "top",
                        whiteSpace: isNumeric(c) || c.type === "timestamp" ? "nowrap" : undefined,
                        minWidth: c.type === "string" ? 80 : undefined,
                        maxWidth: c.type === "string" ? 520 : undefined,
                      }}
                    >
                      <CellContent column={c} value={row[c.name]} />
                    </TableCell>
                  ))}
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </TableContainer>
      )}
    </Stack>
  );
}
