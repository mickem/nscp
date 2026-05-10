import Stack from "@mui/material/Stack";
import {
  Alert,
  Button,
  Collapse,
  FormControl,
  IconButton,
  InputLabel,
  MenuItem,
  Select,
  SelectChangeEvent,
  Snackbar,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from "@mui/material";
import DeleteSweepIcon from "@mui/icons-material/DeleteSweep";
import KeyboardArrowDownIcon from "@mui/icons-material/KeyboardArrowDown";
import KeyboardArrowRightIcon from "@mui/icons-material/KeyboardArrowRight";
import { useMemo, useState } from "react";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import {
  EventEntry,
  nsclientApi,
  useClearEventsMutation,
  useGetEventsQuery,
} from "../api/api.ts";
import FilterField from "./atoms/FilterField.tsx";
import Trail from "./atoms/Trail.tsx";

const REFRESH_RATES = [
  { label: "Off", value: 0 },
  { label: "1 second", value: 1000 },
  { label: "5 seconds", value: 5000 },
  { label: "10 seconds", value: 10000 },
  { label: "30 seconds", value: 30000 },
  { label: "1 minute", value: 60000 },
];

// Per-type subject extractor. The "subject" is a short, human-readable
// summary shown next to the key in the row so the user can tell events
// apart without expanding them. Add a new case here for each new event
// type that has well-known fields worth surfacing.
function getSubject(type: string, data: Record<string, string>): string {
  if (type === "system.process") {
    return [data.exe, data.state].filter(Boolean).join(" — ");
  }
  return "";
}

interface ParsedEvent extends EventEntry {
  type: string;
  key: string;
  subject: string;
}

function parseEvent(e: EventEntry): ParsedEvent {
  const colon = e.event.indexOf(":");
  const type = colon >= 0 ? e.event.slice(0, colon) : e.event;
  const key = colon >= 0 ? e.event.slice(colon + 1) : "";
  return { ...e, type, key, subject: getSubject(type, e.data ?? {}) };
}

export default function Events() {
  const dispatch = useAppDispatch();
  const [refreshRate, setRefreshRate] = useState<number>(0);
  const { data: events } = useGetEventsQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const [clearEvents, { isLoading: isClearing }] = useClearEventsMutation();
  const [filter, setFilter] = useState<string>("");
  const [error, setError] = useState<string | undefined>();
  const [expanded, setExpanded] = useState<Set<number>>(new Set());

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Events"]));
  };

  const onClear = async () => {
    try {
      await clearEvents().unwrap();
      setExpanded(new Set());
    } catch (err) {
      setError(`Failed to clear events: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  const toggleExpanded = (idx: number) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(idx)) next.delete(idx);
      else next.add(idx);
      return next;
    });
  };

  const parsed = useMemo(() => (events ?? []).map(parseEvent), [events]);

  const needle = filter.trim().toLowerCase();
  const filtered = useMemo(() => {
    if (!needle) return parsed;
    return parsed.filter((e) => {
      if (e.type.toLowerCase().includes(needle)) return true;
      if (e.key.toLowerCase().includes(needle)) return true;
      if (e.subject.toLowerCase().includes(needle)) return true;
      if (e.date.toLowerCase().includes(needle)) return true;
      for (const [k, v] of Object.entries(e.data ?? {})) {
        if (k.toLowerCase().includes(needle)) return true;
        if (v.toLowerCase().includes(needle)) return true;
      }
      return false;
    });
  }, [parsed, needle]);

  const sorted = useMemo(
    () => [...filtered].sort((a, b) => b.index - a.index),
    [filtered],
  );

  return (
    <Stack direction="column">
      <Toolbar>
        <Trail title="Events" />
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter events…" />
        {needle && (
          <Typography variant="body2" color="text.secondary">
            {filtered.length}/{events?.length ?? 0}
          </Typography>
        )}
        <Button
          variant="outlined"
          color="error"
          startIcon={<DeleteSweepIcon />}
          onClick={onClear}
          loading={isClearing}
          disabled={!events || events.length === 0}
          sx={{ height: 40 }}
        >
          Clear all
        </Button>
        <FormControl size="small" sx={{ minWidth: 140 }}>
          <InputLabel id="events-refresh-rate">Refresh</InputLabel>
          <Select
            labelId="events-refresh-rate"
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
      <TableContainer sx={{ width: "100%" }}>
        <Table size="small">
          <TableHead>
            <TableRow>
              <TableCell sx={{ width: 36 }} />
              <TableCell sx={{ width: 64 }}>#</TableCell>
              <TableCell sx={{ width: 200 }}>Date</TableCell>
              <TableCell sx={{ width: 220 }}>Type</TableCell>
              <TableCell sx={{ width: 220 }}>Key</TableCell>
              <TableCell>Subject</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {sorted.length === 0 && (
              <TableRow>
                <TableCell colSpan={6}>
                  <Typography variant="body2" color="text.secondary" sx={{ p: 1 }}>
                    {needle
                      ? `No events match “${filter}”.`
                      : "No events recorded."}
                  </Typography>
                </TableCell>
              </TableRow>
            )}
            {sorted.map((e) => {
              const isOpen = expanded.has(e.index);
              const dataPairs = Object.entries(e.data ?? {});
              return (
                <EventRow
                  key={e.index}
                  event={e}
                  isOpen={isOpen}
                  onToggle={() => toggleExpanded(e.index)}
                  dataPairs={dataPairs}
                />
              );
            })}
          </TableBody>
        </Table>
      </TableContainer>
      <Snackbar open={!!error} autoHideDuration={6000} onClose={() => setError(undefined)}>
        <Alert onClose={() => setError(undefined)} severity="error">
          {error}
        </Alert>
      </Snackbar>
    </Stack>
  );
}

interface EventRowProps {
  event: ParsedEvent;
  isOpen: boolean;
  onToggle: () => void;
  dataPairs: [string, string][];
}

function EventRow({ event, isOpen, onToggle, dataPairs }: EventRowProps) {
  return (
    <>
      <TableRow
        hover
        sx={{ cursor: "pointer", "& > *": { borderBottom: isOpen ? 0 : undefined } }}
        onClick={onToggle}
      >
        <TableCell>
          <IconButton size="small" aria-label={isOpen ? "collapse" : "expand"}>
            {isOpen ? (
              <KeyboardArrowDownIcon fontSize="small" />
            ) : (
              <KeyboardArrowRightIcon fontSize="small" />
            )}
          </IconButton>
        </TableCell>
        <TableCell>{event.index}</TableCell>
        <TableCell sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>
          {event.date}
        </TableCell>
        <TableCell sx={{ fontFamily: "monospace" }}>{event.type}</TableCell>
        <TableCell sx={{ fontFamily: "monospace" }}>{event.key}</TableCell>
        <TableCell>
          <Typography
            variant="body2"
            color={event.subject ? "text.primary" : "text.secondary"}
            sx={{
              maxWidth: 600,
              overflow: "hidden",
              textOverflow: "ellipsis",
              whiteSpace: "nowrap",
            }}
          >
            {event.subject || (dataPairs.length === 0 ? "—" : `${dataPairs.length} fields`)}
          </Typography>
        </TableCell>
      </TableRow>
      <TableRow>
        <TableCell colSpan={6} sx={{ pb: 0, pt: 0, border: 0 }}>
          <Collapse in={isOpen} timeout="auto" unmountOnExit>
            {dataPairs.length === 0 ? (
              <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
                No data attached to this event.
              </Typography>
            ) : (
              <Table size="small" sx={{ mb: 2, mt: 1 }}>
                <TableBody>
                  {dataPairs.map(([k, v]) => (
                    <TableRow key={k}>
                      <TableCell
                        sx={{ width: 220, fontFamily: "monospace", color: "text.secondary" }}
                      >
                        {k}
                      </TableCell>
                      <TableCell sx={{ fontFamily: "monospace", wordBreak: "break-all" }}>
                        {v || (
                          <Typography component="span" variant="body2" color="text.secondary">
                            (empty)
                          </Typography>
                        )}
                      </TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            )}
          </Collapse>
        </TableCell>
      </TableRow>
    </>
  );
}
