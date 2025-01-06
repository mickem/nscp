import { SettingsDescription, useUpdateSettingsMutation } from "../api/api.ts";
import {
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  FormControl,
  IconButton,
  Input,
  InputAdornment,
  InputLabel,
  List,
  ListItem,
  ListItemText,
} from "@mui/material";
import Button from "@mui/material/Button";
import { useEffect, useState } from "react";
import CloseIcon from "@mui/icons-material/Close";

interface Params {
  onClose: () => void;
  path: string;
  keyName: string;
  value: string;
  pathDescription?: SettingsDescription;
  keyDescription?: SettingsDescription;
}

export default function SettingsDialog({ onClose, path, keyName, value, keyDescription, pathDescription }: Params) {
  const key = keyName;
  const description = keyDescription?.description || pathDescription?.description;
  const plugins = keyDescription?.plugins || pathDescription?.plugins || [];
  const title = keyDescription?.title || pathDescription?.title || `Settings for ${path} and ${key}`;

  const [newValue, setNewValue] = useState(value);

  const [saveSettings] = useUpdateSettingsMutation();

  useEffect(() => {
    setNewValue(value);
  }, [value]);

  const onHandleSave = async () => {
    await saveSettings({
      path,
      key: keyName,
      value: newValue,
    }).unwrap();
    onClose();
  };

  const restoreDefault = () => {
    setNewValue(keyDescription?.default_value || "");
  };

  return (
    <Dialog open={true} onClose={onClose}>
      <DialogTitle>{title}</DialogTitle>
      <DialogContent>
        <DialogContentText>{description}</DialogContentText>
        <List>
          <ListItem>
            <ListItemText primary="Path" secondary={path} />
          </ListItem>
          <ListItem>
            <ListItemText primary="Key" secondary={key} />
          </ListItem>

          {plugins?.length > 0 && (
            <ListItem>
              <ListItemText primary="Plugins" secondary={plugins} />
            </ListItem>
          )}
          {keyDescription?.default_value && (
            <ListItem>
              <ListItemText primary="Default Value" secondary={keyDescription.default_value} />
            </ListItem>
          )}
          {keyDescription?.sample_usage && (
            <ListItem>
              <ListItemText primary="Sample Usage" secondary={keyDescription.sample_usage} />
            </ListItem>
          )}
          <ListItem>
            <FormControl variant="standard" fullWidth>
              <InputLabel htmlFor="value">Value</InputLabel>
              <Input
                id="value"
                value={newValue}
                onChange={(e) => setNewValue(e.target.value)}
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
        </List>
      </DialogContent>
      <DialogActions>
        <Button onClick={onClose}>Close</Button>
        <Button onClick={onHandleSave} autoFocus>
          Update
        </Button>
      </DialogActions>
    </Dialog>
  );
}
