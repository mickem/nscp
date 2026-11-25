// One setting edited as a single value: text, a number, a switch, or a choice.
// Controlled: the page owns the value and turns it into a change with its own
// recipe. Says when a value comes from the shared default section.
import { FormControlLabel, MenuItem, Stack, Switch, TextField, Typography } from "@mui/material";
import { ReactNode } from "react";
import { DEFAULT_SECTION, Inherited } from "./changes.ts";

export type ValueKind = "text" | "number" | "bool" | { options: string[] };

interface Props {
  label: string;
  help?: ReactNode;
  kind: ValueKind;
  value: string;
  // What the value in effect is and where it comes from, before any edit.
  inherited?: Inherited;
  onChange: (value: string) => void;
  disabled?: boolean;
}

const isTrue = (v: string) => v.toLowerCase() === "true" || v === "1";

// Only a value inherited from the shared section is worth a note: changing it
// here overrides it for this module alone.
function origin(inherited: Inherited | undefined): string | null {
  return inherited?.source === "default section" ? `Inherited from [${DEFAULT_SECTION}].` : null;
}

export default function ValueField({
  label,
  help,
  kind,
  value,
  inherited,
  onChange,
  disabled,
}: Props) {
  const from = origin(inherited);
  const helper =
    help || from ? (
      <>
        {help}
        {help && from ? " " : null}
        {from}
      </>
    ) : undefined;

  if (kind === "bool") {
    return (
      <Stack>
        <FormControlLabel
          control={
            <Switch
              size="small"
              checked={isTrue(value)}
              onChange={(e) => onChange(e.target.checked ? "true" : "false")}
              disabled={disabled}
            />
          }
          label={label}
        />
        {helper && (
          <Typography variant="caption" color="text.secondary" sx={{ pl: 5.5, mt: -0.5 }}>
            {helper}
          </Typography>
        )}
      </Stack>
    );
  }
  if (typeof kind === "object") {
    // A stored value outside the list is shown as it is, never replaced.
    const options = kind.options.includes(value) ? kind.options : [value, ...kind.options];
    return (
      <TextField
        select
        label={label}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        variant="standard"
        fullWidth
        disabled={disabled}
        helperText={helper}
      >
        {options.map((o) => (
          <MenuItem key={o} value={o}>
            {o === "" ? <em>(empty)</em> : o}
          </MenuItem>
        ))}
      </TextField>
    );
  }
  return (
    <TextField
      label={label}
      value={value}
      onChange={(e) => onChange(e.target.value)}
      variant="standard"
      fullWidth
      disabled={disabled}
      helperText={helper}
      slotProps={kind === "number" ? { htmlInput: { inputMode: "numeric" } } : undefined}
    />
  );
}
