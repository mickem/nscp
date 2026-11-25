// The allowed-hosts field of a server module, with where it is kept; the
// reading and writing rules are in allowedHosts.ts.
import { Stack } from "@mui/material";
import { Scope } from "./changes.ts";
import { isOpenToAll } from "./allowedHosts.ts";
import RiskBanner from "./RiskBanner.tsx";
import ValueField from "./ValueField.tsx";

interface Props {
  value: string;
  scope: Scope;
  onChange: (value: string) => void;
  onScopeChange: (scope: Scope) => void;
  disabled?: boolean;
}

export default function AllowedHostsSection({
  value,
  scope,
  onChange,
  onScopeChange,
  disabled,
}: Props) {
  return (
    <Stack spacing={1}>
      <ValueField
        label="Allowed hosts"
        kind="text"
        value={value}
        onChange={onChange}
        scope={scope}
        onScopeChange={onScopeChange}
        help="Comma-separated hosts, netmasks (/ syntax) or *."
        disabled={disabled}
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
