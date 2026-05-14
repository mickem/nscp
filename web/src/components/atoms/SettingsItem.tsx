import {
  Settings,
  useDeleteSettingsMutation,
  useGetSettingsDescriptionsQuery,
  useUpdateSettingsMutation,
} from "../../api/api.ts";
import {
  Chip,
  IconButton,
  ListItem,
  ListItemButton,
  ListItemText,
  Stack,
  Switch,
  Tooltip,
  Typography,
} from "@mui/material";
import RestoreIcon from "@mui/icons-material/Restore";
import SettingsDialog from "./SettingsDialog.tsx";
import { MouseEvent, useState } from "react";

interface Props {
  path: string;
  setting: Settings;
  dense?: boolean;
}

export default function SettingsItem({ path, setting, dense = false }: Props) {
  const [show, setShow] = useState(false);
  const [resetting, setResetting] = useState(false);

  const { data: settingsDescriptions } = useGetSettingsDescriptionsQuery();
  const [deleteSettings] = useDeleteSettingsMutation();
  const [updateSettings] = useUpdateSettingsMutation();
  const [togglingBool, setTogglingBool] = useState(false);

  const description = settingsDescriptions?.find((s) => s.path === path && s.key === setting.key);
  const defaultValue = description?.default_value ?? "";
  const storedValue = setting.value;
  // "modified" = there's a stored value AND it differs from the schema default.
  // An empty stored value is treated as "use the default" — not modified.
  const isModified = storedValue !== "" && storedValue !== defaultValue;
  const displayedValue = storedValue || defaultValue;
  const isBool = description?.type === "bool";
  const boolChecked = displayedValue.toLowerCase() === "true" || displayedValue === "1";

  const obfuscate = (v: string) => v.replace(/./g, "*");
  const title = description?.title || setting.key;
  const helpText = description?.description;
  const displayValue =
    description?.type === "password" ? obfuscate(displayedValue) : displayedValue;

  const onReset = async (e: MouseEvent) => {
    // Don't open the edit dialog when the user clicks the reset icon.
    e.stopPropagation();
    setResetting(true);
    try {
      await deleteSettings({ path: setting.path, key: setting.key }).unwrap();
    } finally {
      setResetting(false);
    }
  };

  const onToggleBool = async (next: boolean) => {
    setTogglingBool(true);
    try {
      await updateSettings({
        path: setting.path,
        key: setting.key,
        value: next ? "true" : "false",
      }).unwrap();
    } finally {
      setTogglingBool(false);
    }
  };

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
            {isBool ? (
              <Switch
                edge="end"
                checked={boolChecked}
                disabled={togglingBool}
                onClick={(e) => e.stopPropagation()}
                onChange={(e) => {
                  e.stopPropagation();
                  void onToggleBool(e.target.checked);
                }}
              />
            ) : (
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
            )}
            <Chip
              size="small"
              label={isModified ? "modified" : "default"}
              variant={isModified ? "filled" : "outlined"}
              color={isModified ? "primary" : "default"}
            />
            {isModified && (
              <Tooltip title="Reset to default">
                <span>
                  <IconButton size="small" onClick={onReset} disabled={resetting}>
                    <RestoreIcon fontSize="small" />
                  </IconButton>
                </span>
              </Tooltip>
            )}
          </Stack>
        </ListItemButton>
      </ListItem>
    </>
  );
}
