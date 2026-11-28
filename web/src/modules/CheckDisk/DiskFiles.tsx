// CheckDisk's named paths: name = path. A caller's file=<name> (or path=)
// uses the path, whatever the access mode allows.
import { Stack, Typography } from "@mui/material";
import { useGetSettingsQuery } from "../../api/api.ts";
import KeyValueCard from "../kit/KeyValueCard.tsx";
import RunCheckButton from "../kit/RunCheckButton.tsx";
import ValueField from "../kit/ValueField.tsx";
import { DISK_FILES } from "./disk.ts";

export default function DiskFiles() {
  const { data: stored } = useGetSettingsQuery();
  const rows = (stored ?? [])
    .filter((s) => s.path === DISK_FILES && s.key !== "")
    .map((s) => ({ key: s.key, value: s.value }))
    .sort((a, b) => a.key.localeCompare(b.key));
  return (
    <KeyValueCard
      collectionPath={DISK_FILES}
      title="Named paths"
      subheader="Files and folders callers use by name (file=<name> or path=<name>), whatever the access setting allows."
      noun="path"
      keyLabel="Name"
      keyHelp="What a caller passes as file= or path=."
      valueLabel="Path"
      emptyText="No named paths are defined."
      rows={rows}
      rowAction={(row) => (
        // By name, as a caller uses it: what the agent has loaded.
        <RunCheckButton
          query="check_single_file"
          args={[`file=${row.key}`]}
          note="Run by name, as the agent loaded it: a path saved since the last reload is not known yet."
        />
      )}
      problem={(_, v) => (v.trim() ? null : "Give the path.")}
      renderValue={(value, onChange) => (
        <Stack spacing={1}>
          <ValueField
            label="Path"
            kind="text"
            value={value}
            onChange={onChange}
            help="A file or a folder: C:\Logs\app.log, /var/log"
          />
          <Typography variant="caption" color="text.secondary">
            The name works with check_files and check_largest_files too, as path=&lt;name&gt;.
          </Typography>
        </Stack>
      )}
    />
  );
}
