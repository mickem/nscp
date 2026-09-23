import { Box, Chip, Stack, Typography } from "@mui/material";
import { FactValue } from "../api/api.ts";

/**
 * Renders one node of the host-facts document.
 *
 * The document is not a flat map the way tags are — that is the whole reason
 * it exists — so this handles the three shapes the agent's rules allow:
 * objects, lists (of records with an `id`, or of plain strings) and scalars.
 * A record leads with its `id`, which is the drive letter, interface name or
 * service name the rest of its fields describe, so the reader can scan down
 * the identifiers rather than read every field to find out what a row is.
 */

/** A record's `id` is its heading, not one of its fields — drop it from the body. */
function recordLabel(value: FactValue): string | undefined {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return undefined;
  const id = (value as { [key: string]: FactValue }).id;
  return typeof id === "string" || typeof id === "number" ? String(id) : undefined;
}

function isPlainObject(value: FactValue): value is { [key: string]: FactValue } {
  return value !== null && typeof value === "object" && !Array.isArray(value);
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

export default function FactTree({ node, depth = 0 }: { node: FactValue; depth?: number }) {
  if (Array.isArray(node)) {
    if (node.length === 0) {
      return (
        <Typography variant="body2" color="text.secondary">
          (empty)
        </Typography>
      );
    }
    return (
      <Stack spacing={1}>
        {node.map((item, index) => {
          const label = recordLabel(item);
          if (label === undefined) {
            // A list of plain strings: no heading to give it.
            return <ScalarValue key={index} value={item} />;
          }
          const rest = Object.fromEntries(
            Object.entries(item as { [key: string]: FactValue }).filter(([key]) => key !== "id"),
          );
          return (
            <Box key={label} sx={{ borderLeft: 2, borderColor: "divider", pl: 1.5 }}>
              <Typography variant="subtitle2" sx={{ fontFamily: "monospace" }}>
                {label}
              </Typography>
              <FactTree node={rest} depth={depth + 1} />
            </Box>
          );
        })}
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
                {nested ? "" : ":"}
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
