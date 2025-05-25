import { SettingsDescription, useUpdateSettingsMutation } from "../../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  List,
  ListItem,
  ListItemText,
} from "@mui/material";
import Button from "@mui/material/Button";
import { useEffect, useState } from "react";
import SettingsDialogValue from "./SettingsDialogValue.tsx";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";

interface Params {
  onClose: () => void;
  path: string;
  keyName: string;
  value: string;
  description?: SettingsDescription;
}

export default function SettingsDialog({ onClose, path, keyName, value, description }: Params) {
  const key = keyName;
  const descriptionText = description?.description || "No description available.";
  const plugins = description?.plugins || [];
  const title = description?.title || `Settings for ${path} and ${key}`;

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

  return (
    <Dialog open={true} onClose={onClose}>
      <DialogTitle>{title}</DialogTitle>
      <DialogContent>
        <DialogContentText>{descriptionText}</DialogContentText>
        <List>
          <SettingsDialogValue value={newValue} description={description} onChange={setNewValue} />
        </List>
        <Accordion elevation={0}>
          <AccordionSummary expandIcon={<ExpandMoreIcon />}>Details</AccordionSummary>
          <AccordionDetails>
            <List dense>
              <ListItem>
                <ListItemText primary="Path" secondary={path} />
              </ListItem>
              <ListItem>
                <ListItemText primary="Key" secondary={key} />
              </ListItem>
              <ListItem>
                <ListItemText primary="Default value" secondary={description?.default_value || ""} />
              </ListItem>
              {plugins?.length > 0 && (
                <ListItem>
                  <ListItemText primary="Plugins" secondary={plugins} />
                </ListItem>
              )}
            </List>
          </AccordionDetails>
        </Accordion>
      </DialogContent>
      <DialogActions>
        <Button onClick={onClose}>Cancel</Button>
        <Button onClick={onHandleSave} autoFocus>
          Update
        </Button>
      </DialogActions>
    </Dialog>
  );
}
