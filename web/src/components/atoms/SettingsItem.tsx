import { Settings, useGetSettingsDescriptionsQuery } from "../../api/api.ts";
import { ListItem, ListItemButton, ListItemText, Switch } from "@mui/material";
import SettingsDialog from "./SettingsDialog.tsx";
import { useState } from "react";

interface Props {
  path: string;
  setting: Settings;
  dense?: boolean;
}

export default function SettingsItem({ path, setting, dense = false }: Props) {
  const [show, setShow] = useState(false);

  const { data: settingsDescriptions } = useGetSettingsDescriptionsQuery();

  const description = settingsDescriptions?.find((s) => s.path === path && s.key === setting.key);
  const value = setting.value || description?.default_value || "";

  const obfuscate = (value: string) => {
    return value.replace(/./g, "*");
  };
  const title = description?.title || setting.key;

  const popup = (
    <>
      {show && (
        <SettingsDialog
          onClose={() => setShow(false)}
          path={setting.path}
          keyName={setting.key}
          value={setting.value}
          description={description}
        />
      )}
    </>
  );
  if (description?.type === "bool") {
    return (
      <>
        {popup}
        <ListItem key={setting.key} alignItems="flex-start" dense={dense}>
          <ListItemButton onClick={() => setShow(true)}>
            <ListItemText primary={title} secondary={setting.value} />
            <Switch edge="end" checked={value === "true"} />
          </ListItemButton>
        </ListItem>
      </>
    );
  }
  if (description?.type === "password") {
    return (
      <>
        {popup}
        <ListItem key={setting.key} alignItems="flex-start" dense={dense}>
          <ListItemButton onClick={() => setShow(true)}>
            <ListItemText primary={setting.key} secondary={obfuscate(value)} />
          </ListItemButton>
        </ListItem>
      </>
    );
  }
  return (
    <>
      {popup}
      <ListItem key={setting.key} alignItems="flex-start" dense={dense}>
        <ListItemButton onClick={() => setShow(true)}>
          <ListItemText primary={setting.key} secondary={value} />
        </ListItemButton>
      </ListItem>
    </>
  );
}
