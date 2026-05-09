import { SettingsDescription } from "../api/api.ts";
import { Accordion, AccordionDetails, AccordionSummary, Card, List, Stack } from "@mui/material";
import Typography from "@mui/material/Typography";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import SettingsItem from "./atoms/SettingsItem.tsx";

interface Props {
  settings: SettingsDescription[];
  forceExpanded?: boolean;
  emptyMessage?: string;
}

export default function SettingsList({ settings, forceExpanded = false, emptyMessage }: Props) {
  const paths = [...new Set(settings.map((s) => s.path))];
  const getKeysForPath = (path: string) => settings.filter((s) => s.path === path && s.key !== "");
  const getSectionInfo = (path: string) => settings.find((s) => s.path === path && s.key === "");

  if (paths.length === 0) {
    return (
      <Card>
        <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
          {emptyMessage ?? "No settings."}
        </Typography>
      </Card>
    );
  }

  return (
    <Card>
      <List sx={{ width: "100%" }} disablePadding>
        {paths.map((path) => {
          const info = getSectionInfo(path);
          const title = info?.title || path;
          return (
          <Accordion
            key={path}
            defaultExpanded={forceExpanded}
            expanded={forceExpanded ? true : undefined}
          >
            <AccordionSummary expandIcon={<ExpandMoreIcon />}>
              <Stack spacing={0.25} sx={{ minWidth: 0 }}>
                <Typography variant="subtitle2">{title}</Typography>
                <Typography
                  variant="caption"
                  color="text.secondary"
                  sx={{
                    fontFamily: "monospace",
                    letterSpacing: 0.2,
                    overflow: "hidden",
                    textOverflow: "ellipsis",
                    whiteSpace: "nowrap",
                  }}
                >
                  {path}
                </Typography>
              </Stack>
            </AccordionSummary>
            <AccordionDetails>
              <List dense>
                {getKeysForPath(path).map((setting) => (
                  <SettingsItem
                    key={`${setting.path}-${setting.key}`}
                    path={setting.path}
                    setting={setting}
                  />
                ))}
              </List>
            </AccordionDetails>
          </Accordion>
          );
        })}
      </List>
    </Card>
  );
}
