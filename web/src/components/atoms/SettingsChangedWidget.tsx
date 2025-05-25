import { nsclientApi, useGetSettingsStatusQuery, useSettingsCommandMutation } from "../../api/api.ts";
import NscpAlert from "./NscpAlert.tsx";
import { Box } from "@mui/material";
import { useEffect } from "react";
import { useAppDispatch } from "../../store/store.ts";

export default function SettingsChangedWidget() {
  const dispatch = useAppDispatch();
  const { data: status } = useGetSettingsStatusQuery();
  const [settingsCommand] = useSettingsCommandMutation();

  const hasChanged = status?.has_changed || false;

  useEffect(() => {
    const onRefresh = () => dispatch(nsclientApi.util.invalidateTags(["SettingsStatus"]));
    const interval = setInterval(onRefresh, 5_000);
    return () => {
      clearInterval(interval);
    };
  }, [dispatch]);

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
