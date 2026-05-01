import {
  nsclientApi,
  useGetSettingsDescriptionsQuery,
  useGetSettingsStatusQuery,
  useSettingsCommandMutation,
} from "../api/api.ts";
import {
  Alert,
  Box,
  Chip,
  IconButton,
  InputAdornment,
  Paper,
  Snackbar,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import SearchIcon from "@mui/icons-material/Search";
import ClearIcon from "@mui/icons-material/Clear";
import SaveIcon from "@mui/icons-material/SaveOutlined";
import DownloadIcon from "@mui/icons-material/CloudDownloadOutlined";
import RestartIcon from "@mui/icons-material/RestartAltOutlined";
import TuneIcon from "@mui/icons-material/TuneOutlined";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import ChevronRightIcon from "@mui/icons-material/ChevronRight";
import Button from "@mui/material/Button";
import SettingsItem from "./atoms/SettingsItem.tsx";
import { useMemo, useState } from "react";

export default function Settings() {
  const dispatch = useAppDispatch();
  const [busy, setBusy] = useState<boolean>(false);
  const [error, setError] = useState<string | undefined>(undefined);
  const [filter, setFilter] = useState<string>("");
  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});

  const { data: settings } = useGetSettingsDescriptionsQuery();
  const [settingsCommand] = useSettingsCommandMutation();
  const { data: settingsStatus } = useGetSettingsStatusQuery();

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Settings", "SettingsDescriptions"]));
  };

  const needle = filter.trim().toLowerCase();
  const filteredSettings = useMemo(() => {
    if (!settings) return [];
    if (!needle) return settings;
    return settings.filter((s) =>
      [s.path, s.key, s.title, s.description].some((f) => (f ?? "").toLowerCase().includes(needle)),
    );
  }, [settings, needle]);

  // The settings inventory returns one entry per path with an empty `key`
  // describing the path itself (title/description). Index those for headers
  // and exclude them from the per-section key lists.
  const pathDescriptions = useMemo(() => {
    const m = new Map<string, { title?: string; description?: string }>();
    for (const s of settings ?? []) {
      if (s.key === "") {
        m.set(s.path, { title: s.title, description: s.description });
      }
    }
    return m;
  }, [settings]);

  const keyEntries = useMemo(() => filteredSettings.filter((s) => s.key !== ""), [filteredSettings]);

  const paths = useMemo(() => [...new Set(filteredSettings.map((s) => s.path))].sort(), [filteredSettings]);
  const totalPaths = new Set(settings?.filter((s) => s.key === "").map((s) => s.path) ?? []).size;

  const getKeysForPath = (path: string) => keyEntries.filter((setting) => setting.path === path);

  const runCommand = async (command: "save" | "load" | "reload", verb: string) => {
    setBusy(true);
    try {
      await settingsCommand({ command }).unwrap();
    } catch (err) {
      setError(`Failed to ${verb} settings: ${err instanceof Error ? err.message : String(err)}`);
    }
    setBusy(false);
  };

  const handleCloseError = () => setError(undefined);
  const toggleCollapsed = (path: string) =>
    setCollapsed((prev) => ({ ...prev, [path]: !(prev[path] ?? false) }));

  const isOpen = (path: string) => (needle ? true : !(collapsed[path] ?? false));

  return (
    <Stack direction="column" spacing={3} sx={{ pb: 6 }}>
      <Paper
        variant="outlined"
        sx={{
          p: { xs: 2, sm: 3 },
          borderRadius: 2,
          background: (theme) =>
            `linear-gradient(135deg, ${theme.palette.background.paper} 0%, ${theme.palette.background.default} 100%)`,
        }}
      >
        <Stack
          direction={{ xs: "column", md: "row" }}
          alignItems={{ xs: "flex-start", md: "center" }}
          justifyContent="space-between"
          spacing={2}
        >
          <Stack direction="row" alignItems="center" spacing={2}>
            <Box
              sx={{
                width: 44,
                height: 44,
                borderRadius: 2,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
                bgcolor: "primary.main",
                color: "primary.contrastText",
              }}
            >
              <TuneIcon />
            </Box>
            <Box>
              <Typography variant="h5" sx={{ lineHeight: 1.1 }}>
                Settings
              </Typography>
              <Stack direction="row" spacing={1} sx={{ mt: 0.5 }} alignItems="center">
                {settingsStatus?.context && (
                  <Chip label={settingsStatus.context} size="small" variant="outlined" />
                )}
                <Typography variant="caption" color="text.secondary">
                  {totalPaths} sections · {settings?.length ?? 0} settings
                </Typography>
              </Stack>
            </Box>
          </Stack>
          <Stack direction="row" spacing={1} alignItems="center" flexWrap="wrap">
            <Button
              size="small"
              variant="outlined"
              startIcon={<DownloadIcon />}
              onClick={() => runCommand("load", "load")}
              loading={busy}
            >
              Load
            </Button>
            <Button
              size="small"
              variant="contained"
              startIcon={<SaveIcon />}
              onClick={() => runCommand("save", "save")}
              loading={busy}
            >
              Save
            </Button>
            <Button
              size="small"
              variant="outlined"
              color="secondary"
              startIcon={<RestartIcon />}
              onClick={() => runCommand("reload", "reload")}
              loading={busy}
            >
              Reload service
            </Button>
            <RefreshButton onRefresh={onRefresh} />
          </Stack>
        </Stack>
        <TextField
          fullWidth
          size="small"
          placeholder="Search by path, key, title, or description…"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          sx={{ mt: 2.5 }}
          slotProps={{
            input: {
              startAdornment: (
                <InputAdornment position="start">
                  <SearchIcon fontSize="small" />
                </InputAdornment>
              ),
              endAdornment: filter ? (
                <InputAdornment position="end">
                  <Stack direction="row" spacing={1} alignItems="center">
                    <Typography variant="caption" color="text.secondary">
                      {paths.length}/{totalPaths} sections · {filteredSettings.length} matches
                    </Typography>
                    <IconButton size="small" onClick={() => setFilter("")} aria-label="clear filter">
                      <ClearIcon fontSize="small" />
                    </IconButton>
                  </Stack>
                </InputAdornment>
              ) : null,
            },
          }}
        />
      </Paper>

      {paths.length === 0 && needle && (
        <Paper variant="outlined" sx={{ p: 4, textAlign: "center", borderRadius: 2 }}>
          <Typography variant="body1" color="text.secondary">
            No settings match “{filter}”.
          </Typography>
        </Paper>
      )}

      <Stack spacing={2}>
        {paths.map((path) => {
          const items = getKeysForPath(path);
          const open = isOpen(path);
          const meta = pathDescriptions.get(path);
          const sectionTitle = meta?.title;
          const sectionDescription = meta?.description;
          return (
            <Paper
              key={path}
              variant="outlined"
              sx={{
                borderRadius: 2,
                overflow: "hidden",
                transition: "border-color 120ms ease",
                "&:hover": { borderColor: "primary.main" },
              }}
            >
              <Stack
                direction="row"
                alignItems="flex-start"
                spacing={1.5}
                onClick={() => toggleCollapsed(path)}
                sx={{
                  px: 2,
                  py: 1.5,
                  cursor: "pointer",
                  userSelect: "none",
                  "&:hover": { bgcolor: "action.hover" },
                }}
              >
                {open ? (
                  <ExpandMoreIcon fontSize="small" sx={{ color: "text.secondary", mt: 0.5 }} />
                ) : (
                  <ChevronRightIcon fontSize="small" sx={{ color: "text.secondary", mt: 0.5 }} />
                )}
                <Box sx={{ flexGrow: 1, minWidth: 0 }}>
                  {sectionTitle ? (
                    <>
                      <Typography variant="subtitle1" sx={{ fontWeight: 600, lineHeight: 1.2 }}>
                        {sectionTitle}
                      </Typography>
                      <Typography
                        variant="caption"
                        color="text.secondary"
                        sx={{ display: "block", fontFamily: "monospace", mt: 0.25 }}
                      >
                        {path}
                      </Typography>
                    </>
                  ) : (
                    <Typography
                      variant="subtitle1"
                      sx={{ fontFamily: "monospace", fontWeight: 500 }}
                    >
                      {path}
                    </Typography>
                  )}
                  {sectionDescription && (
                    <Typography
                      variant="body2"
                      color="text.secondary"
                      sx={{
                        mt: 0.5,
                        display: "-webkit-box",
                        WebkitLineClamp: open ? undefined : 1,
                        WebkitBoxOrient: "vertical",
                        overflow: "hidden",
                      }}
                    >
                      {sectionDescription}
                    </Typography>
                  )}
                </Box>
                <Chip
                  size="small"
                  label={`${items.length} ${items.length === 1 ? "setting" : "settings"}`}
                  variant="outlined"
                  sx={{ height: 22, mt: 0.25 }}
                />
              </Stack>
              {open && (
                <Box sx={{ borderTop: 1, borderColor: "divider", bgcolor: "background.default" }}>
                  {items.length === 0 ? (
                    <Typography
                      variant="body2"
                      color="text.secondary"
                      sx={{ p: 2, fontStyle: "italic" }}
                    >
                      No settings in this section.
                    </Typography>
                  ) : (
                    <Stack divider={<Box sx={{ borderTop: 1, borderColor: "divider" }} />}>
                      {items.map((setting) => (
                        <SettingsItem key={setting.key} path={path} setting={setting} />
                      ))}
                    </Stack>
                  )}
                </Box>
              )}
            </Paper>
          );
        })}
      </Stack>

      <Snackbar open={!!error} autoHideDuration={6000} onClose={handleCloseError}>
        <Alert onClose={handleCloseError} severity="error">
          {error}
        </Alert>
      </Snackbar>
    </Stack>
  );
}
