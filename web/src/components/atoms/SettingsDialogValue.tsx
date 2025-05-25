import { SettingsDescription } from "../../api/api.ts";
import {
  FormControl,
  IconButton,
  Input,
  InputAdornment,
  InputLabel,
  ListItem,
  ListItemText,
  Switch,
} from "@mui/material";
import CloseIcon from "@mui/icons-material/Close";

interface Params {
  value: string;
  description?: SettingsDescription;
  onChange: (value: string) => void;
}

export default function SettingsDialogValue({ value, description, onChange }: Params) {
  const restoreDefault = () => {
    onChange(description?.default_value || "");
  };

  const boolValue = (value: string): boolean => value.toLowerCase() === "true" || value === "1";
  const onBoolChange = (checked: boolean) => onChange(checked ? "true" : "false");
  const enableValue = (value: string): boolean => value.toLowerCase() === "enabled" || value === "1";
  const onEnableChange = (checked: boolean) => onChange(checked ? "enabled" : "disabled");

  if (description?.type === "bool") {
    return (
      <ListItem>
        <ListItemText primary={"Value"} secondary={value} />
        <Switch checked={boolValue(value)} onChange={(e) => onBoolChange(e.target.checked)} />
      </ListItem>
    );
  }

  if (description?.path === "/modules") {
    return (
      <ListItem>
        <ListItemText primary={"Value"} secondary={value} />
        <Switch checked={enableValue(value)} onChange={(e) => onEnableChange(e.target.checked)} />
      </ListItem>
    );
  }

  const type = description?.type === "password" ? "password" : "text";

  return (
    <ListItem>
      <FormControl variant="standard" fullWidth>
        <InputLabel htmlFor="value">Value</InputLabel>
        <Input
          id="value"
          type={type}
          value={value}
          onChange={(e) => onChange(e.target.value)}
          endAdornment={
            <InputAdornment position="end">
              <IconButton onClick={restoreDefault} edge="end">
                <CloseIcon />
              </IconButton>
            </InputAdornment>
          }
        />
      </FormControl>
    </ListItem>
  );
}
