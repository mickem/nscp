// Wrapped scripts: a script file and its arguments, run through the wrapping
// for the file's extension - the runtime (powershell, cscript, ...) lives in
// the wrapping, so each script is just a name, a file and arguments.
import { Stack, Typography } from "@mui/material";
import KeyValueCard from "../kit/KeyValueCard.tsx";
import ValueField from "../kit/ValueField.tsx";
import { EXT_WRAPPED } from "./ext.ts";
import {
  joinWrapped,
  readWrapped,
  readWrappings,
  splitWrapped,
  wrappedCommand,
} from "./wrapped.ts";
import { useStagedSettings } from "../kit/staging.ts";

export default function WrappedScriptsEditor() {
  const { stored } = useStagedSettings();
  const wrappings = readWrappings(stored);
  const known = wrappings.map((w) => w.ext).join(", ");
  const missing = (value: string) => {
    const { ext, command } = wrappedCommand(value, wrappings);
    return command === null
      ? `No wrapping for .${ext} (there are ${known}; the match is case sensitive): the agent registers an empty command.`
      : null;
  };

  return (
    <KeyValueCard
      collectionPath={EXT_WRAPPED}
      title="Wrapped scripts"
      subheader="Scripts run through the wrapping for their extension: name the file and its arguments, the wrapping supplies the runtime."
      noun="wrapped script"
      keyLabel="Name"
      keyHelp="What the monitoring server calls."
      valueLabel="Script and arguments"
      emptyText="No wrapped scripts are defined."
      rows={readWrapped(stored).map(([key, value]) => {
        const { ext, command } = wrappedCommand(value, wrappings);
        return {
          key,
          value,
          chip: command === null ? { label: `no .${ext} wrapping`, color: "error" } : undefined,
          detail: command ?? undefined,
        };
      })}
      problem={(_, value) =>
        splitWrapped(value).script === "" ? "Give the script file." : missing(value)
      }
      renderValue={(value, onChange) => {
        const { script, args } = splitWrapped(value);
        const { ext, command } = wrappedCommand(value, wrappings);
        return (
          <Stack spacing={2}>
            <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
              <ValueField
                label="Script file"
                kind="text"
                value={script}
                onChange={(s) => onChange(joinWrapped(s.replace(/\s/g, ""), args))}
                help="Under the scripts folder, e.g. check_updates.ps1. No spaces: the first space ends it."
              />
              <ValueField
                label="Arguments"
                kind="text"
                value={args}
                onChange={(a) => onChange(joinWrapped(script, a))}
                help="Fixed arguments, $ARG1$ for the caller's (only while arguments are allowed)."
              />
            </Stack>
            {script && (
              <Typography variant="body2" component="div">
                {command === null ? (
                  `No wrapping for .${ext}.`
                ) : (
                  <>
                    Runs with the .{ext} wrapping:
                    <Typography
                      variant="body2"
                      component="div"
                      sx={{ fontFamily: "monospace", wordBreak: "break-all", mt: 0.5 }}
                    >
                      {command}
                    </Typography>
                  </>
                )}
              </Typography>
            )}
          </Stack>
        );
      }}
    />
  );
}
