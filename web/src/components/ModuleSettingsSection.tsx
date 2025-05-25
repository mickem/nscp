import { SettingsDescription } from "../api/api.ts";
import { Collapse, List, ListItem, ListItemButton, ListItemText } from "@mui/material";
import { useState } from "react";
import { ExpandLess, ExpandMore } from "@mui/icons-material";
import SettingsItem from "./atoms/SettingsItem.tsx";

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

  return (
    <>
      <ListItemButton onClick={handleClick} sx={{ p: 0 }}>
        <ListItem key={`${section}`}>
          <ListItemText
            primary={section}
            slotProps={{
              primary: {
                color: "secondary",
                fontWeight: "bold",
              },
            }}
          />
          {open ? <ExpandLess /> : <ExpandMore />}
        </ListItem>
      </ListItemButton>
      <Collapse in={open} timeout="auto" unmountOnExit>
        <List dense>
          {keys.map((setting) => (
            <SettingsItem key={`${setting.path}-${setting.key}`} dense={true} path={setting.path} setting={setting} />
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
