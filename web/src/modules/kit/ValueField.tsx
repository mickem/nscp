// One setting edited as a single value: text, a number, a switch, or a choice.
// Controlled: the page owns the value and turns it into a change with its own
// recipe.
//
// A setting the agent can inherit from [/settings/default] also gets a
// Default / Local switch: Default keeps the value in the shared section, used
// by every module that does not set its own; Local keeps it in this module's
// section only.
import {
  FormControlLabel,
  MenuItem,
  Stack,
  Switch,
  TextField,
  ToggleButton,
  ToggleButtonGroup,
  Typography,
} from "@mui/material";
import { ReactNode } from "react";
import { DEFAULT_SECTION, Scope } from "./changes.ts";

export type ValueKind = "text" | "number" | "bool" | { options: string[] };

interface Props {
  label: string;
  help?: ReactNode;
  kind: ValueKind;
  value: string;
  onChange: (value: string) => void;
  // Inheritable settings only: where the value is kept.
  scope?: Scope;
  onScopeChange?: (scope: Scope) => void;
  disabled?: boolean;
}

const isTrue = (v: string) => v.toLowerCase() === "true" || v === "1";

export function ScopeToggle({
  label,
  scope,
  onChange,
  disabled,
}: {
  label: string;
  scope: Scope;
  onChange: (scope: Scope) => void;
  disabled?: boolean;
}) {
  return (
    <ToggleButtonGroup
      size="small"
      exclusive
      value={scope}
      onChange={(_, next: Scope | null) => next && onChange(next)}
      aria-label={`Where ${label} is kept`}
      disabled={disabled}
      sx={{ "& .MuiToggleButton-root": { py: 0, px: 1, textTransform: "none", fontSize: 12 } }}
    >
      <ToggleButton value="default">Default</ToggleButton>
      <ToggleButton value="local">Local</ToggleButton>
    </ToggleButtonGroup>
  );
}

function scopeNote(scope: Scope): string {
  return scope === "default"
    ? `Shared: kept in [${DEFAULT_SECTION}] and used by every module that does not set its own.`
    : "Local: kept for this module only.";
}

export default function ValueField({
  label,
  help,
  kind,
  value,
  onChange,
  scope,
  onScopeChange,
  disabled,
}: Props) {
  const note = scope ? scopeNote(scope) : null;
  const helper =
    help || note ? (
      <>
        {help}
        {help && note ? " " : null}
        {note}
      </>
    ) : undefined;

  let control: ReactNode;
  if (kind === "bool") {
    control = (
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
  } else if (typeof kind === "object") {
    // A stored value outside the list is shown as it is, never replaced.
    const options = kind.options.includes(value) ? kind.options : [value, ...kind.options];
    control = (
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
  } else {
    control = (
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
  if (!scope || !onScopeChange) return control;
  return (
    <Stack direction="row" spacing={1.5} sx={{ alignItems: "flex-start", width: "100%" }}>
      <BoxGrow>{control}</BoxGrow>
      <Stack sx={{ pt: kind === "bool" ? 0.5 : 2 }}>
        <ScopeToggle label={label} scope={scope} onChange={onScopeChange} disabled={disabled} />
      </Stack>
    </Stack>
  );
}

function BoxGrow({ children }: { children: ReactNode }) {
  return <Stack sx={{ flexGrow: 1, minWidth: 0 }}>{children}</Stack>;
}
