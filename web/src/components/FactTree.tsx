import { useState } from "react";
import { Box, ButtonBase, Chip, Collapse, Stack, Typography } from "@mui/material";
import KeyboardArrowDownIcon from "@mui/icons-material/KeyboardArrowDown";
import KeyboardArrowRightIcon from "@mui/icons-material/KeyboardArrowRight";
import { FactValue } from "../api/api.ts";
import { ShowMore } from "./atoms/ShowMore.tsx";
import { useCapped } from "./atoms/useCapped.ts";

/**
 * Renders one node of the host-facts document.
 *
 * The document is not a flat map the way tags are — that is the whole reason
 * it exists — so this handles the three shapes the agent's rules allow:
 * objects, lists (of records with an `id`, or of plain strings) and scalars.
 *
 * A list of records is the shape that decides whether this page is readable.
 * Spelling out every field of every record costs seven lines per network
 * interface, so a dozen interfaces is a hundred lines and the card owns the
 * page. Each record is therefore one row — its `id`, which is the drive
 * letter or interface name the rest of its fields describe, plus a glance at
 * the first few of them — and opens on a click. The height of a set then
 * follows how many things the host has, not how much its producer has to say
 * about each one.
 */

/** How many record rows a list shows before it offers the rest. */
const PREVIEW_ROWS = 12;
/** How many of a record's fields the collapsed row glances at. */
const SUMMARY_FIELDS = 3;
/** A value longer than this is an identifier, not a glance — skip it. */
const SUMMARY_MAX_LENGTH = 32;

/** A record's `id` is its heading, not one of its fields — drop it from the body. */
function recordLabel(value: FactValue): string | undefined {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return undefined;
  const id = (value as { [key: string]: FactValue }).id;
  return typeof id === "string" || typeof id === "number" ? String(id) : undefined;
}

function isPlainObject(value: FactValue): value is { [key: string]: FactValue } {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

/**
 * A list every item of which is a record, so it can render as rows.
 *
 * Deliberately not a type predicate: narrowing a `FactValue[]` to `FactValue[]`
 * leaves the else-branch as `never`, and the plain-string list below it is a
 * real case, not an impossible one.
 */
function isRecordList(value: FactValue): boolean {
  return Array.isArray(value) && value.length > 0 && value.every((item) => recordLabel(item) !== undefined);
}

function scalarText(value: FactValue): string | undefined {
  if (typeof value === "boolean") return value ? "yes" : "no";
  if (typeof value === "string") return value.length > 0 ? value : undefined;
  if (typeof value === "number") return String(value);
  // A list of addresses reads as its first one; the rest are in the record.
  if (Array.isArray(value) && value.length > 0) return scalarText(value[0]);
  return undefined;
}

/**
 * The glance a collapsed row carries beside its name.
 *
 * Generic on purpose: a fact is whatever a producer publishes, so this takes
 * the first few short scalars in the order the opened record lists them
 * rather than claiming to know which field of an interface is the telling
 * one. Long values are skipped because they are identifiers - a volume's
 * device GUID says nothing at a glance and crowds out the fields that do.
 */
function summarize(record: { [key: string]: FactValue }): string {
  return Object.entries(record)
    .filter(([key]) => key !== "id")
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([, value]) => scalarText(value))
    .filter((text): text is string => text !== undefined && text.length <= SUMMARY_MAX_LENGTH)
    .slice(0, SUMMARY_FIELDS)
    .join(" · ");
}

function ScalarValue({ value }: { value: FactValue }) {
  if (typeof value === "boolean") {
    return <Chip label={value ? "yes" : "no"} size="small" variant="outlined" color={value ? "success" : "default"} />;
  }
  return (
    <Typography component="span" variant="body2" sx={{ fontFamily: "monospace", wordBreak: "break-word" }}>
      {String(value)}
    </Typography>
  );
}

/** One record of a list: a name and a glance, with its fields a click away. */
function RecordRow({ record, label, depth }: { record: { [key: string]: FactValue }; label: string; depth: number }) {
  const [open, setOpen] = useState(false);
  const summary = summarize(record);
  const fields = Object.fromEntries(Object.entries(record).filter(([key]) => key !== "id"));

  return (
    <Box>
      <ButtonBase
        onClick={() => setOpen((was) => !was)}
        aria-expanded={open}
        sx={{
          width: "100%",
          justifyContent: "flex-start",
          gap: 0.5,
          px: 0.5,
          py: 0.25,
          borderRadius: 1,
          textAlign: "left",
          "&:hover": { backgroundColor: "action.hover" },
        }}
      >
        {open ? (
          <KeyboardArrowDownIcon fontSize="small" sx={{ color: "text.secondary" }} />
        ) : (
          <KeyboardArrowRightIcon fontSize="small" sx={{ color: "text.secondary" }} />
        )}
        <Typography variant="subtitle2" sx={{ fontFamily: "monospace", whiteSpace: "nowrap" }}>
          {label}
        </Typography>
        {/* Dropped once the record is open: the fields below say it better,
            and repeating them above only makes the row harder to leave. */}
        {!open && summary !== "" && (
          <Typography
            variant="caption"
            color="text.secondary"
            sx={{
              // Held off the name: run together, the two read as one long
              // string and the eye cannot find where the record's name ends.
              ml: 1.5,
              fontFamily: "monospace",
              minWidth: 0,
              overflow: "hidden",
              textOverflow: "ellipsis",
              whiteSpace: "nowrap",
            }}
          >
            {summary}
          </Typography>
        )}
      </ButtonBase>
      <Collapse in={open} unmountOnExit>
        <Box sx={{ borderLeft: 2, borderColor: "divider", ml: 1.25, pl: 1.5, py: 0.5 }}>
          <FactTree node={fields} depth={depth + 1} />
        </Box>
      </Collapse>
    </Box>
  );
}

/**
 * A list of records, capped by rows so nothing is ever cut mid-field.
 */
function RecordList({ records, depth }: { records: FactValue[]; depth: number }) {
  const { shown, hidden, showAll, toggle } = useCapped(records, PREVIEW_ROWS);

  return (
    <Stack>
      {shown.map((item, index) => (
        <RecordRow
          key={`${recordLabel(item)}-${index}`}
          record={item as { [key: string]: FactValue }}
          label={recordLabel(item) as string}
          depth={depth}
        />
      ))}
      <ShowMore hidden={hidden} showAll={showAll} onToggle={toggle} />
    </Stack>
  );
}

export default function FactTree({ node, depth = 0 }: { node: FactValue; depth?: number }) {
  if (Array.isArray(node)) {
    if (node.length === 0) {
      return (
        <Typography variant="body2" color="text.secondary">
          (empty)
        </Typography>
      );
    }
    if (isRecordList(node)) return <RecordList records={node} depth={depth} />;
    // A list of plain strings: no heading to give it, one line each.
    return (
      <Stack spacing={0.5}>
        {node.map((item, index) => (
          <ScalarValue key={index} value={item} />
        ))}
      </Stack>
    );
  }

  if (isPlainObject(node)) {
    const entries = Object.entries(node).sort(([a], [b]) => a.localeCompare(b));
    if (entries.length === 0) {
      return (
        <Typography variant="body2" color="text.secondary">
          (empty)
        </Typography>
      );
    }
    return (
      <Stack spacing={0.5} sx={{ pl: depth > 0 ? 1.5 : 0 }}>
        {entries.map(([key, value]) => {
          const nested = Array.isArray(value) || isPlainObject(value);
          const records = Array.isArray(value) && isRecordList(value) ? value.length : undefined;
          return (
            <Box
              key={key}
              sx={
                nested
                  ? undefined
                  : // A scalar reads best as one line, with the keys aligned so a
                    // set can be scanned down its left edge.
                    { display: "flex", gap: 1, alignItems: "baseline" }
              }
            >
              <Typography
                variant="body2"
                color="text.secondary"
                sx={{ fontFamily: "monospace", minWidth: nested ? undefined : 160 }}
              >
                {key}
                {/* The count belongs on the heading of a list of things: with
                    the rows folded it is what the reader came to find out. */}
                {records !== undefined ? ` (${records})` : nested ? "" : ":"}
              </Typography>
              {nested ? <FactTree node={value} depth={depth + 1} /> : <ScalarValue value={value} />}
            </Box>
          );
        })}
      </Stack>
    );
  }

  return <ScalarValue value={node} />;
}
