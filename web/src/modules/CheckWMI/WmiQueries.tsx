// WMI's named queries: name = WQL. A caller's query=<name> runs it as
// written, whatever the access mode allows.
import { useGetSettingsQuery } from "../../api/api.ts";
import KeyValueCard from "../kit/KeyValueCard.tsx";
import ValueField from "../kit/ValueField.tsx";
import { WMI_QUERIES } from "./wmi.ts";

export default function WmiQueries() {
  const { data: stored } = useGetSettingsQuery();
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
      problem={(_, v) =>
        /^\s*(select|associators|references)\b/i.test(v) ? null : "A WQL query: SELECT ... FROM ..."
      }
      renderValue={(value, onChange) => (
        <ValueField
          label="WQL"
          kind="text"
          value={value}
          onChange={onChange}
          help="SELECT Name, State FROM Win32_Service WHERE StartMode = 'Auto'"
        />
      )}
    />
  );
}
