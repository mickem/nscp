// A password, token or key. The stored value is never shown back - often it
// is a hash (pbkdf2-sha256$...) that means nothing to read - only whether one
// is set. Typing a value replaces it; Clear removes it.
import { Button, Stack, TextField } from "@mui/material";
import { ReactNode } from "react";
import { Scope } from "./changes.ts";
import { ScopeToggle } from "./ValueField.tsx";
import { NO_PASSWORD_AUTOFILL } from "./noAutofill.ts";

interface Props {
  label: string;
  help?: ReactNode;
  // What is stored now, and the draft value.
  current: string;
  value: string;
  onChange: (value: string) => void;
  scope?: Scope;
  onScopeChange?: (scope: Scope) => void;
  disabled?: boolean;
}

function describe(current: string): string {
  if (current === "") return "Not set.";
  if (current.startsWith("pbkdf2-")) return "Set (stored hashed).";
  return "Set.";
}

export default function SecretField({
  label,
  help,
  current,
  value,
  onChange,
  scope,
  onScopeChange,
  disabled,
}: Props) {
  const changed = value !== current;
  const status = !changed
    ? describe(current)
    : value === ""
      ? "Will be cleared when saved."
      : "Will be replaced when saved.";
  return (
    <Stack spacing={0.5} sx={{ flexGrow: 1, width: "100%" }}>
      <Stack direction="row" spacing={1.5} sx={{ alignItems: "flex-start" }}>
        <TextField
          label={label}
          type="password"
          slotProps={{ htmlInput: NO_PASSWORD_AUTOFILL }}
          // Only what the reader types: emptying the field again keeps the
          // stored value.
          value={changed ? value : ""}
          placeholder={current === "" ? "" : "unchanged"}
          onChange={(e) => onChange(e.target.value === "" ? current : e.target.value)}
          variant="standard"
          fullWidth
          disabled={disabled}
          helperText={
            <>
              {status}
              {help ? " " : null}
              {help}
            </>
          }
        />
        {current !== "" && (
          <Button
            size="small"
            onClick={() => onChange("")}
            disabled={disabled || value === ""}
            sx={{ mt: 2 }}
          >
            Clear
          </Button>
        )}
        {scope && onScopeChange && (
          <Stack sx={{ pt: 2 }}>
            <ScopeToggle label={label} scope={scope} onChange={onScopeChange} disabled={disabled} />
          </Stack>
        )}
      </Stack>
    </Stack>
  );
}
