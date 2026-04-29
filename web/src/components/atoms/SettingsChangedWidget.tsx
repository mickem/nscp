import { useGetSettingsStatusQuery, useSettingsCommandMutation } from "../../api/api.ts";
import { Alert, Box, Button, Stack } from "@mui/material";
import WarningIcon from "@mui/icons-material/Warning";
import { useAppSelector } from "../../store/store.ts";
import { useState } from "react";
import SettingsDiffDialog from "./SettingsDiffDialog.tsx";

export default function SettingsChangedWidget() {
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data: status } = useGetSettingsStatusQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const [settingsCommand] = useSettingsCommandMutation();
  const [diffOpen, setDiffOpen] = useState(false);

  const hasChanged = status?.has_changed || false;

  const saveSettings = async () => {
    await settingsCommand({ command: "save" }).unwrap();
    await settingsCommand({ command: "reload" }).unwrap();
  };
  return (
    <>
      {hasChanged && (
        <Box sx={{ paddingBottom: 3 }}>
          <Alert
            icon={<WarningIcon fontSize="inherit" />}
            severity="warning"
            action={
              <Stack direction="row" spacing={1}>
                <Button
                  color="inherit"
                  size="small"
                  onClick={() => setDiffOpen(true)}
                >
                  Show changes
                </Button>
                <Button
                  color="inherit"
                  size="small"
                  onClick={saveSettings}
                >
                  Save and reload
                </Button>
              </Stack>
            }
          >
            You have unsaved configuration
          </Alert>
        </Box>
      )}
      <SettingsDiffDialog open={diffOpen} onClose={() => setDiffOpen(false)} />
    </>
  );
}
