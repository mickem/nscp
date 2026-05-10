import { SettingsDescription } from "../api/api.ts";
import { Accordion, AccordionDetails, AccordionSummary, Card, List, Stack } from "@mui/material";
import Typography from "@mui/material/Typography";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import SettingsItem from "./atoms/SettingsItem.tsx";

interface Props {
  settings: SettingsDescription[];
  forceExpanded?: boolean;
  emptyMessage?: string;
  // When true, render each path's section inline (header + items) instead
  // of wrapping it in an Accordion. Useful when the caller has only a few
  // sections and the collapse machinery just adds clicks.
  flat?: boolean;
}

export default function SettingsList({
  settings,
  forceExpanded = false,
  emptyMessage,
  flat = false,
}: Props) {
  const getKeysForPath = (path: string) => settings.filter((s) => s.path === path && s.key !== "");
  const getSectionInfo = (path: string) => settings.find((s) => s.path === path && s.key === "");
  // Skip paths that have no visible keys — collapsed accordions with empty
  // bodies are confusing and add nothing.
  const paths = [...new Set(settings.map((s) => s.path))].filter(
    (p) => getKeysForPath(p).length > 0,
  );

  if (paths.length === 0) {
    return (
      <Card>
        <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
          {emptyMessage ?? "No settings."}
        </Typography>
      </Card>
    );
  }

  const renderHeader = (title: string, path: string) => (
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
  );

  if (flat) {
    return (
      <Card>
        {paths.map((path, idx) => {
          const info = getSectionInfo(path);
          const title = info?.title || path;
          return (
            <Stack
              key={path}
              sx={{
                px: 2,
                py: 1.5,
                borderTop: idx === 0 ? 0 : 1,
                borderColor: "divider",
              }}
            >
              {renderHeader(title, path)}
              <List dense disablePadding>
                {getKeysForPath(path).map((setting) => (
                  <SettingsItem
                    key={`${setting.path}-${setting.key}`}
                    path={setting.path}
                    setting={setting}
                  />
                ))}
              </List>
            </Stack>
          );
        })}
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
                {renderHeader(title, path)}
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
