// WMI's named queries: name = WQL. A caller's query=<name> runs it as
// written, whatever the access mode allows.
import { Stack, Typography } from "@mui/material";
import { readStored } from "../kit/changes.ts";
import RunCheckButton from "../kit/RunCheckButton.tsx";
import KeyValueCard from "../kit/KeyValueCard.tsx";
import ValueField from "../kit/ValueField.tsx";
import { queryAccessOf, WMI_PATH, WMI_QUERIES } from "./wmi.ts";
import { useStagedSettings } from "../kit/staging.ts";

export default function WmiQueries() {
  const { stored } = useStagedSettings();
  const restricted = queryAccessOf(readStored(stored, WMI_PATH, "query access") ?? "any") !== "any";
  const rows = (stored ?? [])
    .filter((s) => s.path === WMI_QUERIES && s.key !== "")
    .map((s) => ({ key: s.key, value: s.value }))
    .sort((a, b) => a.key.localeCompare(b.key));
  return (
    <KeyValueCard
      collectionPath={WMI_QUERIES}
      title="Named queries"
      subheader="Queries callers run by name (query=<name>, case sensitive). They run as written, whatever the access setting allows."
      noun="query"
      keyLabel="Name"
      keyHelp="What a caller passes as query=."
      valueLabel="Query"
      emptyText="No named queries are defined."
      rows={rows}
      rowAction={(row) => (
        // By name, as a caller runs it: what the agent has loaded, so a
        // query saved since the last reload is not known yet.
        <RunCheckButton
          query="check_wmi"
          args={[`query=${row.key}`]}
          note="Run by name, as the agent loaded it: a query saved since the last reload is not known yet."
        />
      )}
      problem={(_, v) =>
        /^\s*(select|associators|references)\b/i.test(v) ? null : "A WQL query: SELECT ... FROM ..."
      }
      renderValue={(value, onChange) => (
        <Stack spacing={1}>
          <ValueField
            label="WQL"
            kind="text"
            value={value}
            onChange={onChange}
            help="SELECT Name, State FROM Win32_Service WHERE StartMode = 'Auto'"
          />
          <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
            <RunCheckButton
              query="check_wmi"
              args={[`query=${value.trim()}`]}
              label="Run now"
              disabled={!value.trim()}
            />
            <Typography variant="caption" color="text.secondary">
              {restricted
                ? "Runs the query as typed, so the access setting above applies to it until it is saved and the agent reloaded."
                : "Runs the query as typed."}
            </Typography>
          </Stack>
        </Stack>
      )}
    />
  );
}
