import { SettingsDescription } from "../api/api.ts";
import { Collapse, List, ListItem, ListItemButton, ListItemText } from "@mui/material";
import { useState } from "react";
import { ExpandLess, ExpandMore } from "@mui/icons-material";

interface Props {
  section: string;
  settings: SettingsDescription[];
}

export default function ModuleSettingsSection({ section, settings }: Props) {
  const [open, setOpen] = useState(false);
  const handleClick = () => {
    setOpen(!open);
  };

  const keys = settings.filter((setting) => setting.key !== "");
  const getValue = (settings: SettingsDescription) => settings.value || settings.default_value || "?";

  return (
    <>
      <ListItemButton onClick={handleClick} sx={{ p: 0 }}>
        <ListItem key={`${section}`}>
          <ListItemText
            primary={section}
            slotProps={{
              primary: {
                  color: 'secondary',
                fontWeight: "bold",
              },
            }}
          />
          {open ? <ExpandLess /> : <ExpandMore />}
        </ListItem>
      </ListItemButton>
      <Collapse in={open} timeout="auto" unmountOnExit>
        <List>
          {keys.map((setting) => (
            <ListItem key={`${setting.path}-${setting.key}`} sx={{ paddingLeft: 3 }}>
              <ListItemText primary={setting.key} secondary={getValue(setting)} />
            </ListItem>
          ))}
          {keys.length === 0 && (
            <ListItem sx={{ paddingLeft: 3 }}>
              <ListItemText primary="No settings" />
            </ListItem>
          )}
        </List>
      </Collapse>
    </>
  );
}
