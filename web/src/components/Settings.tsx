import Stack from "@mui/material/Stack";
import {
  nsclientApi,
  SettingsDescription,
  useGetSettingsDescriptionsQuery,
  useGetSettingsQuery,
  useGetSettingsStatusQuery,
  useSettingsCommandMutation,
} from "../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  List,
  ListItem,
  ListItemButton,
  ListItemText,
} from "@mui/material";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import { useState } from "react";
import SettingsDialog from "./SettingsDialog.tsx";
import Button from "@mui/material/Button";
import NscpAlert from "./atoms/NscpAlert.tsx";
import Divider from "@mui/material/Divider";
import Typography from "@mui/material/Typography";

interface Selected {
  path: string;
  key: string;
  value: string;
  keyDescription?: SettingsDescription;
  pathDescription?: SettingsDescription;
}
export default function Settings() {
  const dispatch = useAppDispatch();

  const [selected, setSelected] = useState<Selected | undefined>(undefined);

  const { data: settings } = useGetSettingsQuery();
  const { data: settingsDescriptions } = useGetSettingsDescriptionsQuery();
  const [settingsCommand] = useSettingsCommandMutation();
  const { data: settingsStatus } = useGetSettingsStatusQuery();

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Settings", "SettingsDescriptions"]));
  };

  const paths = [...new Set(settings?.map((settings) => settings.path))];

  const getKeysForPath = (path: string) => {
    return settings?.filter((setting) => setting.path === path);
  };

  const findDescription = (path: string, key: string) => {
    return settingsDescriptions?.find((setting) => setting.path === path && setting.key === key);
  };
  const findValue = (path: string, key: string) => {
    return settings?.find((setting) => setting.path === path && setting.key === key)?.value;
  };

  const showKey = (path: string, key: string) => {
    const value = findValue(path, key) || "";
    const keyDescription = findDescription(path, key);
    const pathDescription = findDescription(path, "");
    setSelected({ path, key, value, keyDescription, pathDescription });
  };
  const onClose = () => {
    setSelected(undefined);
  };

  const saveSettings = async () => {
    await settingsCommand({ command: "save" }).unwrap();
  };
  const loadSettings = async () => {
    await settingsCommand({ command: "load" }).unwrap();
  };
  const reloadSettings = async () => {
    await settingsCommand({ command: "reload" }).unwrap();
  };

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Typography>{settingsStatus?.context}</Typography>
        <Divider orientation="vertical" flexItem />
        <Button onClick={loadSettings}>Load</Button>
        <Button onClick={saveSettings}>Save</Button>
        <Spacing />
        <Button onClick={reloadSettings}>Reload service</Button>
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      {selected && (
        <>
          <SettingsDialog
            onClose={onClose}
            path={selected.path}
            keyName={selected.key}
            value={selected.value}
            keyDescription={selected.keyDescription}
            pathDescription={selected.pathDescription}
          />
        </>
      )}
      {settingsStatus?.has_changed && <NscpAlert severity="warning" text="You have unsaved changes" />}
      <List sx={{ width: "100%" }}>
        {paths?.map((path) => (
          <Accordion key={path}>
            <AccordionSummary expandIcon={<ExpandMoreIcon />}>{path}</AccordionSummary>
            <AccordionDetails>
              <List dense>
                {getKeysForPath(path)?.map((setting) => (
                  <ListItem key={setting.key} alignItems="flex-start">
                    <ListItemButton onClick={() => showKey(path, setting.key)} dense>
                      <ListItemText primary={setting.key} secondary={setting.value} />
                    </ListItemButton>
                  </ListItem>
                ))}
              </List>
            </AccordionDetails>
          </Accordion>
        ))}
      </List>
    </Stack>
  );
}
