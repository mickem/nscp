import { Settings, useGetSettingsDescriptionsQuery, useUpdateSettingsMutation } from "../../api/api.ts";
import { Box, Chip, Stack, Switch, Tooltip, Typography } from "@mui/material";
import EditIcon from "@mui/icons-material/EditOutlined";
import LockIcon from "@mui/icons-material/LockOutlined";
import SettingsDialog from "./SettingsDialog.tsx";
import { useState } from "react";

interface Props {
  path: string;
  setting: Settings;
}

const obfuscate = (value: string) => "•".repeat(Math.min(value.length, 12));

const isBoolish = (type?: string) => type === "bool";
const isModuleSwitch = (path: string) => path === "/modules";
const boolValue = (value: string) => value.toLowerCase() === "true" || value === "1";
const enableValue = (value: string) => value.toLowerCase() === "enabled" || value === "1";

export default function SettingsItem({ path, setting }: Props) {
  const [show, setShow] = useState(false);

  const { data: settingsDescriptions } = useGetSettingsDescriptionsQuery();
  const [saveSettings] = useUpdateSettingsMutation();

  const description = settingsDescriptions?.find((s) => s.path === path && s.key === setting.key);
  const value = setting.value || description?.default_value || "";
  const title = description?.title || setting.key;
  const type = description?.type || "string";
  const isPassword = type === "password";
  const isDefault = !setting.value || setting.value === description?.default_value;

  const onInlineToggle = async (checked: boolean) => {
    const next = isModuleSwitch(path)
      ? checked
        ? "enabled"
        : "disabled"
      : checked
        ? "true"
        : "false";
    await saveSettings({ path, key: setting.key, value: next }).unwrap();
  };

  const renderValue = () => {
    if (isPassword) {
      return (
        <Stack direction="row" spacing={0.5} alignItems="center">
          <LockIcon sx={{ fontSize: 14, color: "text.secondary" }} />
          <Typography variant="body2" sx={{ fontFamily: "monospace" }}>
            {value ? obfuscate(value) : "(not set)"}
          </Typography>
        </Stack>
      );
    }
    if (!value) {
      return (
        <Typography variant="body2" color="text.disabled" fontStyle="italic">
          (not set)
        </Typography>
      );
    }
    return (
      <Typography
        variant="body2"
        sx={{
          fontFamily: "monospace",
          color: "text.primary",
          maxWidth: 400,
          overflow: "hidden",
          textOverflow: "ellipsis",
          whiteSpace: "nowrap",
        }}
      >
        {value}
      </Typography>
    );
  };

  const inlineToggle = isBoolish(type) || isModuleSwitch(path);

  return (
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
      <Box
        onClick={() => setShow(true)}
        sx={{
          px: 2,
          py: 1.5,
          cursor: "pointer",
          transition: "background-color 80ms ease",
          "&:hover": { bgcolor: "action.hover" },
          "&:hover .edit-hint": { opacity: 1 },
        }}
      >
        <Stack direction="row" spacing={2} alignItems="flex-start">
          <Box sx={{ flexGrow: 1, minWidth: 0 }}>
            <Stack direction="row" spacing={1} alignItems="center" flexWrap="wrap">
              <Typography variant="subtitle2" sx={{ fontWeight: 600 }}>
                {title}
              </Typography>
              {type && type !== "string" && (
                <Chip
                  label={type}
                  size="small"
                  variant="outlined"
                  sx={{ height: 18, fontSize: 10, textTransform: "uppercase", letterSpacing: 0.5 }}
                />
              )}
              {!isDefault && (
                <Chip
                  label="modified"
                  size="small"
                  color="warning"
                  variant="outlined"
                  sx={{ height: 18, fontSize: 10, textTransform: "uppercase", letterSpacing: 0.5 }}
                />
              )}
              {description?.is_advanced_key && (
                <Chip
                  label="advanced"
                  size="small"
                  variant="outlined"
                  sx={{ height: 18, fontSize: 10, textTransform: "uppercase", letterSpacing: 0.5 }}
                />
              )}
            </Stack>
            <Typography
              variant="caption"
              color="text.secondary"
              sx={{ display: "block", fontFamily: "monospace", mb: 0.5 }}
            >
              {setting.key}
            </Typography>
            {description?.description && (
              <Typography
                variant="body2"
                color="text.secondary"
                sx={{
                  mb: 0.75,
                  display: "-webkit-box",
                  WebkitLineClamp: 2,
                  WebkitBoxOrient: "vertical",
                  overflow: "hidden",
                }}
              >
                {description.description}
              </Typography>
            )}
            {renderValue()}
          </Box>
          <Stack direction="row" spacing={1} alignItems="center" sx={{ flexShrink: 0 }}>
            {inlineToggle ? (
              <Tooltip title={isModuleSwitch(path) ? "Enable / disable module" : "Toggle value"}>
                <Switch
                  size="small"
                  checked={isModuleSwitch(path) ? enableValue(value) : boolValue(value)}
                  onClick={(e) => e.stopPropagation()}
                  onChange={(e) => onInlineToggle(e.target.checked)}
                />
              </Tooltip>
            ) : (
              <EditIcon
                className="edit-hint"
                sx={{ fontSize: 18, color: "text.secondary", opacity: 0, transition: "opacity 80ms ease" }}
              />
            )}
          </Stack>
        </Stack>
      </Box>
    </>
  );
}
