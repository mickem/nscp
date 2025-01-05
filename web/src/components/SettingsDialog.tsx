import { SettingsDescription } from "../api/api.ts";
import {
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  List,
  ListItem,
  ListItemText,
  TextField,
} from "@mui/material";
import Button from "@mui/material/Button";
import { useEffect, useState } from "react";

interface Params {
  open: boolean;
  onClose: () => void;
  onSetValue: (value: string) => void;
  path: string;
  keyName: string;
  value: string;
  pathDescription?: SettingsDescription;
  keyDescription?: SettingsDescription;
}

export default function SettingsDialog({
  open,
  onClose,
  onSetValue,
  path,
  keyName,
  value,
  keyDescription,
  pathDescription,
}: Params) {
  const key = keyName;
  const description = keyDescription?.description || pathDescription?.description;
  const plugins = keyDescription?.plugins || pathDescription?.plugins || [];
  const title = keyDescription?.title || pathDescription?.title || `Settings for ${path} and ${key}`;

  const [newValue, setNewValue] = useState(value);

  useEffect(() => {
    setNewValue(value);
  }, [value]);

  const onHandleSave = () => {
    onSetValue(newValue);
    onClose();
  };

  return (
    <Dialog open={open} onClose={onClose}>
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
            <TextField
              label="Value"
              fullWidth
              value={newValue}
              onChange={(e) => setNewValue(e.target.value)}
              variant="standard"
            />
          </ListItem>
        </List>
      </DialogContent>
      <DialogActions>
        <Button onClick={onClose}>Cancel</Button>
        <Button onClick={onHandleSave} autoFocus>
          Save
        </Button>
      </DialogActions>
    </Dialog>
  );
}
