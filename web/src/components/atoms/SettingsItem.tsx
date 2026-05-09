import { Settings, useGetSettingsDescriptionsQuery } from "../../api/api.ts";
import { ListItem, ListItemButton, ListItemText, Stack, Switch, Typography } from "@mui/material";
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

  const obfuscate = (v: string) => v.replace(/./g, "*");
  const title = description?.title || setting.key;
  const helpText = description?.description;
  const displayValue = description?.type === "password" ? obfuscate(value) : value;

  const popup = show && (
    <SettingsDialog
      onClose={() => setShow(false)}
      path={setting.path}
      keyName={setting.key}
      value={setting.value}
      description={description}
    />
  );

  return (
    <>
      {popup}
      <ListItem key={setting.key} alignItems="flex-start" dense={dense} disableGutters>
        <ListItemButton onClick={() => setShow(true)}>
          <ListItemText
            primary={title}
            secondary={helpText}
            slotProps={{
              secondary: { sx: { whiteSpace: "pre-wrap" } },
            }}
          />
          <Stack direction="row" spacing={1} alignItems="center" sx={{ ml: 2, flexShrink: 0 }}>
            <Typography
              variant="body2"
              color="text.secondary"
              sx={{
                maxWidth: 240,
                overflow: "hidden",
                textOverflow: "ellipsis",
                whiteSpace: "nowrap",
                fontFamily: "monospace",
              }}
            >
              {displayValue}
            </Typography>
            {description?.type === "bool" && <Switch edge="end" checked={value === "true"} />}
          </Stack>
        </ListItemButton>
      </ListItem>
    </>
  );
}
