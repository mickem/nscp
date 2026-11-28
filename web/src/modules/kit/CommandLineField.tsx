// A command line: the check, picked from those the agent knows, and its
// arguments, with the check's options a click away and a way to run it now.
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Alert,
  Autocomplete,
  Button,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import { useMemo, useRef, useState } from "react";
import { makeVocabulary } from "../../common/queryHelp.ts";
import { contextAt, CursorContext } from "../../common/syntax.ts";
import SyntaxArgumentsField, {
  SyntaxArgumentsFieldHandle,
} from "../../components/atoms/SyntaxArgumentsField.tsx";
import QueryHelpPanel from "../../components/QueryHelpPanel.tsx";
import {
  useExecuteQueryMutation,
  useGetAliasesQuery,
  useGetQueriesQuery,
  useGetQueryHelpQuery,
} from "../../api/api.ts";
import {
  commandLineProblem,
  joinCommand,
  placeholders,
  splitCommand,
  tokenize,
} from "./commandLine.ts";

interface Props {
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
  // Said under the arguments, e.g. how $ARG1$ is filled.
  argumentsHelp?: string;
  // Offer to run the line as it stands (it has no side effect for a check).
  runnable?: boolean;
}

// A check's description can run to a paragraph; the field says its first
// sentence. The help panel below has the rest.
function firstSentence(text: string): string {
  const m = /^.*?[.!?](?=\s|$)/.exec(text.trim());
  return m ? m[0] : text;
}

interface Option {
  name: string;
  description: string;
  kind: "check" | "alias";
}

export default function CommandLineField({
  value,
  onChange,
  disabled,
  argumentsHelp,
  runnable = true,
}: Props) {
  const { command, args } = splitCommand(value);
  const { data: queries } = useGetQueriesQuery();
  const { data: aliases } = useGetAliasesQuery();
  const { data: help, isFetching: helpLoading } = useGetQueryHelpQuery(command, {
    skip: command === "",
  });
  const vocabulary = useMemo(() => makeVocabulary(help), [help]);
  const [context, setContext] = useState<CursorContext>(() => contextAt("", 0));
  const argumentsRef = useRef<SyntaxArgumentsFieldHandle>(null);
  const [executeQuery, { isLoading }] = useExecuteQueryMutation();
  const [result, setResult] = useState<{ ok: boolean; text: string } | null>(null);

  const options = useMemo<Option[]>(
    () => [
      ...(queries ?? []).map((q) => ({
        name: q.name,
        description: q.description,
        kind: "check" as const,
      })),
      ...(aliases ?? []).map((a) => ({
        name: a.name,
        description: a.description,
        kind: "alias" as const,
      })),
    ],
    [queries, aliases],
  );
  const known = options.find((o) => o.name === command);
  const holes = placeholders(value);
  const lineProblem = commandLineProblem(value);

  const run = async () => {
    setResult(null);
    try {
      const r = await executeQuery({ query: command, args: tokenize(args) }).unwrap();
      const text = r.lines.map((l) => l.message).join(" ") || "(no output)";
      const status = ["OK", "WARNING", "CRITICAL", "UNKNOWN"][r.result] ?? String(r.result);
      setResult({ ok: r.result === 0, text: `${status}: ${text}` });
    } catch (e) {
      setResult({ ok: false, text: e instanceof Error ? e.message : JSON.stringify(e) });
    }
  };

  return (
    <Stack spacing={1.5}>
      <Autocomplete
        freeSolo
        options={options}
        value={command}
        disabled={disabled}
        groupBy={(o) => (typeof o === "string" ? "" : o.kind === "check" ? "Checks" : "Aliases")}
        getOptionLabel={(o) => (typeof o === "string" ? o : o.name)}
        onInputChange={(_, v, reason) => {
          if (reason !== "reset" || v !== command) onChange(joinCommand(v, args));
        }}
        renderOption={(props, o) => (
          <li {...props} key={o.name}>
            <Stack>
              <Typography variant="body2" sx={{ fontFamily: "monospace" }}>
                {o.name}
              </Typography>
              {o.description && (
                <Typography variant="caption" color="text.secondary">
                  {firstSentence(o.description)}
                </Typography>
              )}
            </Stack>
          </li>
        )}
        renderInput={(params) => (
          <TextField
            {...params}
            variant="standard"
            label="Command"
            helperText={
              (known && firstSentence(known.description)) ??
              (command && options.length > 0 ? "Not a command this agent has loaded." : undefined)
            }
          />
        )}
      />
      <SyntaxArgumentsField
        ref={argumentsRef}
        value={args}
        onChange={(a) => onChange(joinCommand(command, a))}
        vocabulary={vocabulary}
        onContextChange={setContext}
        disabled={disabled}
        placeholder={argumentsHelp ?? '"warn=load > 80%" "crit=load > 90%"'}
      />
      {command && (
        <Accordion
          disableGutters
          elevation={0}
          sx={{ bgcolor: "transparent", "&:before": { display: "none" } }}
        >
          <AccordionSummary expandIcon={<ExpandMoreIcon />} sx={{ px: 0 }}>
            <Typography variant="body2">Options and filter keywords of {command}</Typography>
          </AccordionSummary>
          <AccordionDetails sx={{ px: 0 }}>
            <QueryHelpPanel
              queryName={command}
              vocabulary={vocabulary}
              context={context}
              loading={helpLoading}
              onInsert={(entry) => argumentsRef.current?.insert(entry)}
            />
          </AccordionDetails>
        </Accordion>
      )}
      {runnable && (
        <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
          <Button
            size="small"
            onClick={() => void run()}
            disabled={disabled || !command || holes.length > 0 || lineProblem !== null}
            loading={isLoading}
          >
            Run now
          </Button>
          {holes.length > 0 && (
            <Typography variant="caption" color="text.secondary">
              Takes {holes.join(", ")} from its caller, so it cannot run on its own.
            </Typography>
          )}
        </Stack>
      )}
      {result && (
        <Alert severity={result.ok ? "success" : "warning"} onClose={() => setResult(null)}>
          {result.text}
        </Alert>
      )}
    </Stack>
  );
}
