import { SettingsDescription } from "../../api/api.ts";
import {
  Autocomplete,
  FormControl,
  IconButton,
  Input,
  InputAdornment,
  InputLabel,
  ListItem,
  ListItemText,
  MenuItem,
  Select,
  Switch,
  TextField,
} from "@mui/material";
import CloseIcon from "@mui/icons-material/Close";
import { resolveEnumOptions } from "./fieldEnums.ts";

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

  // `verify mode` accepts arbitrary text and comma-separated combinations,
  // so a strict Select would be wrong — use the same freeSolo widget the
  // collection-instance editor uses with `none` / `peer-cert` as quick picks.
  if (description?.key === "verify mode") {
    const opts = ["none", "peer-cert"];
    return (
      <ListItem>
        <FormControl variant="standard" fullWidth>
          <Autocomplete<string, false, false, true>
            freeSolo
            size="small"
            options={opts}
            value={value}
            onChange={(_e, next) => onChange(next ?? "")}
            onInputChange={(_e, next, reason) => {
              if (reason !== "reset") onChange(next);
            }}
            renderInput={(params) => (
              <TextField {...params} variant="standard" label="Value" />
            )}
          />
        </FormControl>
      </ListItem>
    );
  }

  // Same enum resolution the inline editor uses (subsystem, type, severity, …).
  const enumOpts = description ? resolveEnumOptions(description, description.path) : undefined;
  if (enumOpts) {
    return (
      <ListItem>
        <FormControl variant="standard" fullWidth>
          <InputLabel id="value-label">Value</InputLabel>
          <Select
            labelId="value-label"
            id="value"
            value={value}
            label="Value"
            onChange={(e) => onChange(e.target.value)}
          >
            <MenuItem value="">
              <em style={{ color: "rgba(255,255,255,0.6)" }}>
                {description?.default_value
                  ? `(inherit default — ${description.default_value})`
                  : "(unset)"}
              </em>
            </MenuItem>
            {enumOpts.map((opt) => (
              <MenuItem key={opt.value} value={opt.value}>
                {opt.label}
              </MenuItem>
            ))}
          </Select>
        </FormControl>
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
