import { SettingsDescription } from "../../api/api.ts";
import {
  Box,
  FormControlLabel,
  IconButton,
  InputAdornment,
  Stack,
  Switch,
  TextField,
  Typography,
} from "@mui/material";
import VisibilityIcon from "@mui/icons-material/VisibilityOutlined";
import VisibilityOffIcon from "@mui/icons-material/VisibilityOffOutlined";
import { useState } from "react";

interface Params {
  value: string;
  description?: SettingsDescription;
  onChange: (value: string) => void;
}

const boolValue = (value: string): boolean => value.toLowerCase() === "true" || value === "1";
const enableValue = (value: string): boolean => value.toLowerCase() === "enabled" || value === "1";

export default function SettingsDialogValue({ value, description, onChange }: Params) {
  const [revealPassword, setRevealPassword] = useState(false);

  if (description?.type === "bool") {
    const checked = boolValue(value);
    return (
      <Box
        sx={{
          p: 2,
          borderRadius: 2,
          border: 1,
          borderColor: "divider",
          bgcolor: "background.default",
        }}
      >
        <FormControlLabel
          control={
            <Switch
              checked={checked}
              onChange={(e) => onChange(e.target.checked ? "true" : "false")}
            />
          }
          label={
            <Stack direction="row" spacing={1} alignItems="baseline">
              <Typography variant="body1">{checked ? "Enabled" : "Disabled"}</Typography>
              <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
                ({value || "false"})
              </Typography>
            </Stack>
          }
        />
      </Box>
    );
  }

  if (description?.path === "/modules") {
    const checked = enableValue(value);
    return (
      <Box
        sx={{
          p: 2,
          borderRadius: 2,
          border: 1,
          borderColor: "divider",
          bgcolor: "background.default",
        }}
      >
        <FormControlLabel
          control={
            <Switch
              checked={checked}
              onChange={(e) => onChange(e.target.checked ? "enabled" : "disabled")}
            />
          }
          label={
            <Stack direction="row" spacing={1} alignItems="baseline">
              <Typography variant="body1">{checked ? "Module enabled" : "Module disabled"}</Typography>
              <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
                ({value || "disabled"})
              </Typography>
            </Stack>
          }
        />
      </Box>
    );
  }

  const isPassword = description?.type === "password";
  const inputType = isPassword && !revealPassword ? "password" : "text";

  return (
    <TextField
      label="Value"
      fullWidth
      autoFocus
      variant="outlined"
      type={inputType}
      value={value}
      onChange={(e) => onChange(e.target.value)}
      slotProps={{
        input: {
          sx: { fontFamily: "monospace" },
          endAdornment: isPassword ? (
            <InputAdornment position="end">
              <IconButton
                size="small"
                aria-label={revealPassword ? "Hide password" : "Show password"}
                onClick={() => setRevealPassword((v) => !v)}
                edge="end"
              >
                {revealPassword ? <VisibilityOffIcon /> : <VisibilityIcon />}
              </IconButton>
            </InputAdornment>
          ) : undefined,
        },
      }}
    />
  );
}
