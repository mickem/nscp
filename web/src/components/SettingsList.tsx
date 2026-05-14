import { SettingsDescription } from "../api/api.ts";
import { Accordion, AccordionDetails, AccordionSummary, Box, Card, List, Stack } from "@mui/material";
import Typography from "@mui/material/Typography";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import SettingsItem from "./atoms/SettingsItem.tsx";
import NscpAlert from "./atoms/NscpAlert.tsx";
import { useAppSelector } from "../store/store.ts";

// Paths that warrant a "this affects everything" warning at the top of the
// section because their values feed into every module via inheritance.
// Also pulled out of the regular list and rendered in their own card at the
// top so they're not mixed in with per-module sections.
const GLOBAL_DEFAULT_PATHS = new Set<string>(["/settings/default"]);
const GLOBAL_DEFAULT_WARNING =
  "These are inherited defaults — changing a value here affects every module that doesn't override it explicitly.";

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
  const hideDefaults = useAppSelector((state) => state.dashboard.hideDefaults);
  // Matches SettingsItem's "modified" definition: a stored value that is both
  // non-empty AND different from the schema default. An empty stored value
  // means "use the default" and counts as unmodified for the toggle.
  const isDefault = (s: SettingsDescription) => {
    const stored = s.value ?? "";
    const def = s.default_value ?? "";
    return stored === "" || stored === def;
  };
  const visibleSettings = hideDefaults
    ? settings.filter((s) => s.key === "" || !isDefault(s))
    : settings;
  const getKeysForPath = (path: string) =>
    visibleSettings.filter((s) => s.path === path && s.key !== "");
  // Section headers always come from the unfiltered list so titles/help stay
  // available even when every child has been filtered out.
  const getSectionInfo = (path: string) => settings.find((s) => s.path === path && s.key === "");
  // Skip paths that have no visible keys — collapsed accordions with empty
  // bodies are confusing and add nothing.
  const paths = [...new Set(visibleSettings.map((s) => s.path))].filter(
    (p) => getKeysForPath(p).length > 0,
  );

  if (paths.length === 0) {
    // Distinguish "filter hid everything" from "this section really has
    // nothing" so the user knows the toggle is the reason the page is empty.
    const message =
      hideDefaults && settings.some((s) => s.key !== "")
        ? "No modified settings — all values are at their schema default."
        : (emptyMessage ?? "No settings.");
    return (
      <Card>
        <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
          {message}
        </Typography>
      </Card>
    );
  }

  const defaultPaths = paths.filter((p) => GLOBAL_DEFAULT_PATHS.has(p));
  const otherPaths = paths.filter((p) => !GLOBAL_DEFAULT_PATHS.has(p));

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

  const renderFlatCard = (pathsToRender: string[]) => (
    <Card>
      {pathsToRender.map((path, idx) => {
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
            {GLOBAL_DEFAULT_PATHS.has(path) && (
              <Box sx={{ mt: 1 }}>
                <NscpAlert severity="warning" text={GLOBAL_DEFAULT_WARNING} />
              </Box>
            )}
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

  const renderAccordionCard = (pathsToRender: string[]) => (
    <Card>
      <List sx={{ width: "100%" }} disablePadding>
        {pathsToRender.map((path) => {
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
                {GLOBAL_DEFAULT_PATHS.has(path) && (
                  <Box sx={{ mb: 1 }}>
                    <NscpAlert severity="warning" text={GLOBAL_DEFAULT_WARNING} />
                  </Box>
                )}
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

  const renderCard = flat ? renderFlatCard : renderAccordionCard;

  // Defaults render above the regular sections in their own card so the
  // "inherited" semantics are visually separated from module-specific config.
  if (defaultPaths.length > 0 && otherPaths.length > 0) {
    return (
      <Stack spacing={3}>
        {renderCard(defaultPaths)}
        {renderCard(otherPaths)}
      </Stack>
    );
  }
  return renderCard(defaultPaths.length > 0 ? defaultPaths : otherPaths);
}
