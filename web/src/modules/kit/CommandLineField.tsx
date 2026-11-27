// A command line: the check, picked from those the agent knows, and its
// arguments, with the check's options a click away and a way to run it now.
import {
  Alert,
  Autocomplete,
  Box,
  Button,
  Chip,
  Stack,
  TextField,
  Tooltip,
  Typography,
} from "@mui/material";
import { useMemo, useState } from "react";
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
  const { data: help } = useGetQueryHelpQuery(command, { skip: command === "" });
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

  const addOption = (name: string) => {
    const token = `${name}=`;
    onChange(joinCommand(command, `${args} ${token}`));
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
                  {o.description}
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
              known?.description ??
              (command && options.length > 0 ? "Not a command this agent has loaded." : undefined)
            }
          />
        )}
      />
      <TextField
        variant="standard"
        label="Arguments"
        value={args}
        disabled={disabled}
        onChange={(e) => onChange(joinCommand(command, e.target.value))}
        multiline
        helperText={argumentsHelp ?? 'Quote an argument that holds spaces: "warn=load > 80%".'}
        slotProps={{ htmlInput: { style: { fontFamily: "monospace" } } }}
      />
      {help && help.parameters.length > 0 && (
        <Box>
          <Typography variant="caption" color="text.secondary">
            Options of {command}:
          </Typography>
          <Box sx={{ display: "flex", flexWrap: "wrap", gap: 0.5, mt: 0.5 }}>
            {help.parameters.map((p) => (
              <Tooltip key={p.name} title={p.short_description} describeChild>
                <Chip
                  size="small"
                  variant="outlined"
                  label={p.name}
                  onClick={disabled ? undefined : () => addOption(p.name)}
                />
              </Tooltip>
            ))}
          </Box>
        </Box>
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
