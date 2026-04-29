import Stack from "@mui/material/Stack";
import {
  nsclientApi,
  useGetSettingsDescriptionsQuery,
  useGetSettingsStatusQuery,
  useSettingsCommandMutation,
} from "../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Alert,
  ButtonGroup,
  Chip,
  IconButton,
  InputAdornment,
  List,
  Snackbar,
  TextField,
  Typography,
} from "@mui/material";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import SearchIcon from "@mui/icons-material/Search";
import ClearIcon from "@mui/icons-material/Clear";
import Button from "@mui/material/Button";
import SettingsItem from "./atoms/SettingsItem.tsx";
import { useMemo, useState } from "react";

export default function Settings() {
  const dispatch = useAppDispatch();
  const [busy, setBusy] = useState<boolean>(false);
  const [error, setError] = useState<string | undefined>(undefined);
  const [filter, setFilter] = useState<string>("");

  const { data: settings } = useGetSettingsDescriptionsQuery();
  const [settingsCommand] = useSettingsCommandMutation();
  const { data: settingsStatus } = useGetSettingsStatusQuery();

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Settings", "SettingsDescriptions"]));
  };

  // Filter settings on path/key/title/description (case-insensitive).
  const needle = filter.trim().toLowerCase();
  const filteredSettings = useMemo(() => {
    if (!settings) return [];
    if (!needle) return settings;
    return settings.filter((s) =>
      [s.path, s.key, s.title, s.description].some((f) => (f ?? "").toLowerCase().includes(needle)),
    );
  }, [settings, needle]);

  const paths = [...new Set(filteredSettings.map((s) => s.path))];
  const totalPaths = new Set(settings?.map((s) => s.path) ?? []).size;

  const getKeysForPath = (path: string) => filteredSettings.filter((setting) => setting.path === path);

  const saveSettings = async () => {
    setBusy(true);
    try {
      await settingsCommand({ command: "save" }).unwrap();
    } catch (error) {
      setError(`Failed to save settings: ${error instanceof Error ? error.message : String(error)}`);
    }
    setBusy(false);
  };
  const loadSettings = async () => {
    setBusy(true);
    try {
      await settingsCommand({ command: "load" }).unwrap();
    } catch (error) {
      setError(`Failed to load settings: ${error instanceof Error ? error.message : String(error)}`);
    }
    setBusy(false);
  };
  const reloadSettings = async () => {
    setBusy(true);
    try {
      await settingsCommand({ command: "reload" }).unwrap();
    } catch (error) {
      console.log(error);
      setError(`Failed to reload settings: ${error instanceof Error ? error.message : String(error)}`);
    }
    setBusy(false);
  };
  const handleCloseError = () => setError(undefined);

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Chip label={settingsStatus?.context} size="small" variant="outlined" />
        <ButtonGroup size="small" variant="outlined">
          <Button onClick={loadSettings} loading={busy}>
            Load
          </Button>
          <Button onClick={saveSettings} loading={busy}>
            Save
          </Button>
        </ButtonGroup>
        <Spacing />
        <TextField
          size="small"
          placeholder="Filter settings…"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          sx={{ minWidth: 260 }}
          slotProps={{
            input: {
              startAdornment: (
                <InputAdornment position="start">
                  <SearchIcon fontSize="small" />
                </InputAdornment>
              ),
              endAdornment: filter ? (
                <InputAdornment position="end">
                  <IconButton size="small" onClick={() => setFilter("")} aria-label="clear filter">
                    <ClearIcon fontSize="small" />
                  </IconButton>
                </InputAdornment>
              ) : null,
            },
          }}
        />
        {needle && (
          <Typography variant="body2" color="text.secondary">
            {paths.length}/{totalPaths} sections · {filteredSettings.length} matches
          </Typography>
        )}
        <Button size="small" variant="outlined" onClick={reloadSettings} loading={busy}>
          Reload service
        </Button>
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <List sx={{ width: "100%" }}>
        {paths.length === 0 && needle && (
          <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
            No settings match “{filter}”.
          </Typography>
        )}
        {paths.map((path) => (
          <Accordion key={path} defaultExpanded={!!needle} expanded={needle ? true : undefined}>
            <AccordionSummary expandIcon={<ExpandMoreIcon />}>{path}</AccordionSummary>
            <AccordionDetails>
              <List dense>
                {getKeysForPath(path).map((setting) => (
                  <SettingsItem key={setting.key} path={path} setting={setting} />
                ))}
              </List>
            </AccordionDetails>
          </Accordion>
        ))}
      </List>
      <Snackbar open={!!error} autoHideDuration={6000} onClose={handleCloseError}>
        <Alert onClose={handleCloseError} severity="error">
          {error}
        </Alert>
      </Snackbar>
    </Stack>
  );
}
