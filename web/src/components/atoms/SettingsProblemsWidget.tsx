import { useGetSettingsProblemsQuery } from "../../api/api.ts";
import { Alert, Box, Button } from "@mui/material";
import { useAppSelector } from "../../store/store.ts";
import { useState } from "react";
import SettingsProblemsDialog from "./SettingsProblemsDialog.tsx";

// Shown above every page while the saved configuration holds entries nothing
// reads - the same report as the startup log line and `nscp settings
// --validate`. Info-only findings never raise it: they are what a disabled
// module's leftover section looks like.
export default function SettingsProblemsWidget() {
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data } = useGetSettingsProblemsQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const [open, setOpen] = useState(false);

  const warnings = data?.warnings ?? 0;
  return (
    <>
      {warnings > 0 && (
        <Box sx={{ paddingBottom: 3 }}>
          <Alert
            severity="warning"
            action={
              <Button color="inherit" size="small" onClick={() => setOpen(true)}>
                Show details
              </Button>
            }
          >
            {warnings} entr{warnings === 1 ? "y" : "ies"} in the configuration {warnings === 1 ? "is" : "are"} ignored
            because nothing reads {warnings === 1 ? "it" : "them"}.
          </Alert>
        </Box>
      )}
      <SettingsProblemsDialog open={open} onClose={() => setOpen(false)} />
    </>
  );
}
