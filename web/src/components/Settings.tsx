import Stack from "@mui/material/Stack";
import {
  nsclientApi,
  useGetSettingsDescriptionsQuery,
  useGetSettingsStatusQuery,
  useSettingsCommandMutation,
} from "../api/api.ts";
import { Accordion, AccordionDetails, AccordionSummary, Alert, List, Snackbar } from "@mui/material";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import Button from "@mui/material/Button";
import Divider from "@mui/material/Divider";
import Typography from "@mui/material/Typography";
import SettingsItem from "./atoms/SettingsItem.tsx";
import { useState } from "react";

export default function Settings() {
  const dispatch = useAppDispatch();
  const [busy, setBusy] = useState<boolean>(false);
  const [error, setError] = useState<string | undefined>(undefined);

  const { data: settings } = useGetSettingsDescriptionsQuery();
  const [settingsCommand] = useSettingsCommandMutation();
  const { data: settingsStatus } = useGetSettingsStatusQuery();

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Settings", "SettingsDescriptions"]));
  };

  const paths = [...new Set(settings?.map((settings) => settings.path))];

  const getKeysForPath = (path: string) => {
    return settings?.filter((setting) => setting.path === path);
  };

  const saveSettings = async () => {
    setBusy(true);
    try {
      await settingsCommand({ command: "save" }).unwrap();
    } catch (error) {
      setError(`Failed to save settings: ${error instanceof Error ? error.message : String(error)}`);
    }
    setBusy(false);
  };
  const loadSettings = async () => {
    setBusy(true);
    try {
      await settingsCommand({ command: "load" }).unwrap();
    } catch (error) {
      setError(`Failed to load settings: ${error instanceof Error ? error.message : String(error)}`);
    }
    setBusy(false);
  };
  const reloadSettings = async () => {
    setBusy(true);
    try {
      await settingsCommand({ command: "reload" }).unwrap();
    } catch (error) {
      console.log(error);
      setError(`Failed to reload settings: ${error instanceof Error ? error.message : String(error)}`);
    }
    setBusy(false);
  };
  const handleCloseError = () => setError(undefined);

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Typography>{settingsStatus?.context}</Typography>
        <Divider orientation="vertical" flexItem />
        <Button onClick={loadSettings} loading={busy}>
          Load
        </Button>
        <Button onClick={saveSettings} loading={busy}>
          Save
        </Button>
        <Spacing />
        <Button onClick={reloadSettings} loading={busy}>
          Reload service
        </Button>
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <List sx={{ width: "100%" }}>
        {paths?.map((path) => (
          <Accordion key={path}>
            <AccordionSummary expandIcon={<ExpandMoreIcon />}>{path}</AccordionSummary>
            <AccordionDetails>
              <List dense>
                {getKeysForPath(path)?.map((setting) => (
                  <SettingsItem key={setting.key} path={path} setting={setting} />
                ))}
              </List>
            </AccordionDetails>
          </Accordion>
        ))}
      </List>
      <Snackbar open={!!error} autoHideDuration={6000} onClose={handleCloseError}>
        <Alert onClose={handleCloseError} severity="error">
          {error}
        </Alert>
      </Snackbar>
    </Stack>
  );
}
