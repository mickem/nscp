import { useGetSettingsStatusQuery, useSettingsCommandMutation } from "../../api/api.ts";
import NscpAlert from "./NscpAlert.tsx";
import { Box } from "@mui/material";
import { useAppSelector } from "../../store/store.ts";

export default function SettingsChangedWidget() {
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data: status } = useGetSettingsStatusQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const [settingsCommand] = useSettingsCommandMutation();

  const hasChanged = status?.has_changed || false;

  const saveSettings = async () => {
    await settingsCommand({ command: "save" }).unwrap();
    await settingsCommand({ command: "reload" }).unwrap();
  };
  return (
    <>
      {hasChanged && (
        <Box sx={{ paddingBottom: 3 }}>
          <NscpAlert
            severity="warning"
            text="You have unsaved configuration"
            actionTitle="Save and reload"
            onClick={saveSettings}
          />
        </Box>
      )}
    </>
  );
}
