import { SettingsDescription } from "../api/api.ts";
import { Accordion, AccordionDetails, AccordionSummary, List } from "@mui/material";
import Typography from "@mui/material/Typography";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import ModuleSettingsSection from "./ModuleSettingsSection.tsx";

interface Props {
  settings: SettingsDescription[];
}

export default function ModuleSettings({ settings }: Props) {
  const sections = settings.reduce((acc: { [key: string]: SettingsDescription[] }, setting: SettingsDescription) => {
    if (!acc[setting.path]) {
      acc[setting.path] = [];
    }
    acc[setting.path].push(setting);
    return acc;
  }, {});

  return (
    <>
      {settings && settings.length > 0 && (
        <Accordion>
          <AccordionSummary expandIcon={<ExpandMoreIcon />}>
            <Typography>Settings</Typography>
          </AccordionSummary>
          <AccordionDetails>
            <List disablePadding>
              {Object.entries(sections).map(([path, settings]) => (
                <ModuleSettingsSection key={path} section={path} settings={settings} />
              ))}
            </List>
          </AccordionDetails>
        </Accordion>
      )}
    </>
  );
}
