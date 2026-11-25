// The allowed-hosts field of a server module; the reading and writing rules
// are in allowedHosts.ts.
import { Stack, TextField } from "@mui/material";
import { AllowedHosts, DEFAULT_SECTION, isOpenToAll } from "./allowedHosts.ts";
import RiskBanner from "./RiskBanner.tsx";

interface Props {
  value: string;
  inherited: AllowedHosts;
  onChange: (value: string) => void;
  disabled?: boolean;
}

export default function AllowedHostsSection({ value, inherited, onChange, disabled }: Props) {
  const helper =
    inherited.source === "own"
      ? "Comma-separated hosts, netmasks (/ syntax) or *."
      : inherited.source === "default section"
        ? `Comma-separated hosts, netmasks (/ syntax) or *. Currently inherited from [${DEFAULT_SECTION}]; saving here sets it for this server only.`
        : "Comma-separated hosts, netmasks (/ syntax) or *. Currently the built-in default.";
  return (
    <Stack spacing={1}>
      <TextField
        label="Allowed hosts"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        variant="standard"
        fullWidth
        disabled={disabled}
        helperText={helper}
      />
      {isOpenToAll(value) && (
        <RiskBanner
          risk={{
            severity: "error",
            title: "Any host can connect",
            reason:
              "The allowed-hosts list lets in every address. List the monitoring servers instead.",
          }}
        />
      )}
    </Stack>
  );
}
