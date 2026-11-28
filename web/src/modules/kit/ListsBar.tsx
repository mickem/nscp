// The bar at the bottom of a module page that is only lists (the clients'
// targets, Check helpers' aliases, ...): nothing waits to be saved, since
// each list saves from its dialog, but Show config prints the module.
import { Button, Paper, Stack, Typography } from "@mui/material";
import { useState } from "react";
import { useGetSettingsQuery } from "../../api/api.ts";
import { toIni } from "./changes.ts";
import ConfigDialog from "./ConfigDialog.tsx";
import { UNSTORED_TEXT, useStoreSettings } from "./useStoreSettings.ts";
import { listsIni } from "./listsIni.ts";

export default function ListsBar({ moduleId, paths }: { moduleId: string; paths: string[] }) {
  const { data: stored } = useGetSettingsQuery();
  const [open, setOpen] = useState(false);
  const agent = useStoreSettings();
  return (
    <Paper
      elevation={6}
      role="region"
      aria-label="Module configuration"
      sx={{ p: 1.5, mt: 1, borderTop: 2, borderColor: "divider" }}
    >
      <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
        <Typography variant="body2" sx={{ flexGrow: 1 }}>
          {agent.unstored ? UNSTORED_TEXT : "Each list saves its changes from its own dialog."}
        </Typography>
        <Button size="small" onClick={() => setOpen(true)}>
          Show config
        </Button>
        {agent.unstored && (
          <Button
            variant="contained"
            size="small"
            onClick={() => void agent.store()}
            loading={agent.busy}
          >
            Save and reload
          </Button>
        )}
      </Stack>
      <ConfigDialog
        open={open}
        onClose={() => setOpen(false)}
        title={`${moduleId} configuration`}
        note="Paste it into nsclient.ini on another agent, or into a fleet bundle."
        render={() => {
          const lists = listsIni(stored ?? [], paths);
          const head = toIni([["/modules", [[moduleId, "enabled"]]]]);
          return lists ? `${head}\n\n${lists}` : head;
        }}
      />
    </Paper>
  );
}
