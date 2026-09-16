// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

import { useState } from "react";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Box,
  Chip,
  LinearProgress,
  Stack,
  Typography,
} from "@mui/material";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import FilterField from "./atoms/FilterField.tsx";
import { firstLine, HelpEntry, QueryVocabulary } from "../common/queryHelp.ts";
import { CursorContext } from "../common/syntax.ts";

interface EntryListProps {
  entries: HelpEntry[];
  onInsert: (entry: HelpEntry) => void;
  emptyText: string;
}

function EntryList({ entries, onInsert, emptyText }: EntryListProps) {
  if (entries.length === 0) {
    return (
      <Typography variant="body2" color="text.secondary">
        {emptyText}
      </Typography>
    );
  }
  return (
    <Stack spacing={1}>
      {entries.map((entry) => (
        <Box
          key={`${entry.kind}:${entry.name}`}
          onClick={() => onInsert(entry)}
          sx={{
            cursor: "pointer",
            borderRadius: 1,
            px: 1,
            py: 0.5,
            "&:hover": { backgroundColor: "action.hover" },
          }}
        >
          <Stack direction="row" spacing={1} sx={{ alignItems: "baseline", flexWrap: "wrap" }}>
            <Typography component="span" sx={{ fontFamily: "monospace", fontWeight: 600 }}>
              {entry.kind === "function" ? `${entry.name}()` : entry.name}
            </Typography>
            {entry.required && <Chip label="required" size="small" color="warning" />}
            {entry.defaultValue ? (
              <Typography component="span" variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
                = {entry.defaultValue}
              </Typography>
            ) : null}
          </Stack>
          <Typography variant="body2" color="text.secondary" sx={{ whiteSpace: "pre-wrap" }}>
            {entry.description}
          </Typography>
        </Box>
      ))}
    </Stack>
  );
}

interface SectionProps {
  title: string;
  entries: HelpEntry[];
  onInsert: (entry: HelpEntry) => void;
  emptyText: string;
  defaultExpanded?: boolean;
}

/**
 * One half of the panel. The entries a check defines itself are what somebody
 * writing a filter for it needs, so they are shown first and expanded; the
 * ones every check shares - the thirty standard options and the generic
 * summary keywords - are behind their own fold, exactly as the reference docs
 * fold them out of each command's page.
 */
function Section({ title, entries, onInsert, emptyText, defaultExpanded }: SectionProps) {
  const [filter, setFilter] = useState("");
  const needle = filter.toLowerCase();
  const matching = entries.filter(
    (e) => needle === "" || e.name.toLowerCase().includes(needle) || e.description.toLowerCase().includes(needle),
  );
  const own = matching.filter((e) => !e.common);
  const common = matching.filter((e) => e.common);

  return (
    <Accordion defaultExpanded={defaultExpanded}>
      <AccordionSummary expandIcon={<ExpandMoreIcon />}>
        <Typography>
          {title} <Typography component="span" color="text.secondary">({entries.length})</Typography>
        </Typography>
      </AccordionSummary>
      <AccordionDetails>
        <Stack spacing={2}>
          <FilterField value={filter} onChange={setFilter} placeholder="Filter…" minWidth={200} />
          <EntryList entries={own} onInsert={onInsert} emptyText={emptyText} />
          {common.length > 0 && (
            <Accordion>
              <AccordionSummary expandIcon={<ExpandMoreIcon />}>
                <Typography variant="body2" color="text.secondary">
                  Shared by all checks ({common.length})
                </Typography>
              </AccordionSummary>
              <AccordionDetails>
                <EntryList entries={common} onInsert={onInsert} emptyText={emptyText} />
              </AccordionDetails>
            </Accordion>
          )}
        </Stack>
      </AccordionDetails>
    </Accordion>
  );
}

interface Props {
  queryName: string;
  vocabulary: QueryVocabulary;
  /** Where the caret is in the argument line, for the "what am I typing" line. */
  context: CursorContext;
  onInsert: (entry: HelpEntry) => void;
  loading?: boolean;
}

/**
 * What the check accepts, fetched from the agent: every option with its
 * default and its description, and every filter keyword it offers. The same
 * two lists `desc` and `keywords` print at the interactive prompt, next to the
 * field they describe, so a filter can be written against what the check
 * actually offers rather than against memory.
 *
 * Clicking an entry puts it into the argument line.
 */
export default function QueryHelpPanel({ queryName, vocabulary, context, onInsert, loading }: Props) {
  // What the caret is on, if it is on anything we can name. The prompt shows
  // the same thing as a greyed-out hint after the cursor; here there is room
  // for the whole description.
  const current =
    context.word === ""
      ? undefined
      : context.target === "option"
        ? vocabulary.find(context.word, "option")
        : context.target === "keyword"
          ? vocabulary.find(context.word, "keyword")
          : undefined;

  return (
    <Stack spacing={1}>
      {loading && <LinearProgress />}
      <Box sx={{ minHeight: 40 }}>
        {current ? (
          <>
            <Typography component="span" sx={{ fontFamily: "monospace", fontWeight: 600, mr: 1 }}>
              {current.kind === "function" ? `${current.name}()` : current.name}
            </Typography>
            <Typography component="span" variant="body2" color="text.secondary">
              {firstLine(current.description)}
            </Typography>
          </>
        ) : (
          <Typography variant="body2" color="text.secondary">
            Type an option name, or press Ctrl+Space, to see what {queryName} accepts.
          </Typography>
        )}
      </Box>
      <Section
        title="Options"
        entries={vocabulary.options}
        onInsert={onInsert}
        emptyText="This check declares no options of its own."
        defaultExpanded
      />
      <Section
        title={
          vocabulary.keywordSource && vocabulary.keywordSource !== queryName
            ? `Filter keywords (via ${vocabulary.keywordSource})`
            : "Filter keywords"
        }
        entries={vocabulary.keywords}
        onInsert={onInsert}
        emptyText="This check offers no filter keywords - it is not a filter based check."
      />
    </Stack>
  );
}
