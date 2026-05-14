import Stack from "@mui/material/Stack";
import {
  nsclientApi,
  useDisableModuleMutation,
  useEnableModuleMutation,
  useGetAliasesQuery,
  useGetModuleQuery,
  useGetQueriesQuery,
  useGetSettingsDescriptionsQuery,
  useLoadModuleMutation,
  useUnloadModuleMutation,
} from "../api/api.ts";
import {
  Box,
  Card,
  CardActions,
  CardContent,
  Chip,
  FormControlLabel,
  Switch,
  Tooltip,
} from "@mui/material";
import { useNavigate, useParams } from "react-router";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import { useAppDispatch, useAppSelector } from "../store/store.ts";
import { setHideDefaults } from "../common/dashboardSlice.ts";
import { useState } from "react";
import Typography from "@mui/material/Typography";
import Button from "@mui/material/Button";
import ModuleSettings from "../components/ModuleSettings.tsx";
import NscpAlert from "../components/atoms/NscpAlert.tsx";
import Trail from "../components/atoms/Trail.tsx";

export default function Module() {
  const { id = "" } = useParams();
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: module, isFetching: isFetchingModules } = useGetModuleQuery(id || "");
  const { data: queries, isFetching: isFetchingQueries } = useGetQueriesQuery();
  const { data: aliases, isFetching: isFetchingAliases } = useGetAliasesQuery();
  const { data: settings, isFetching: isFetchingSettings } = useGetSettingsDescriptionsQuery();
  const [unloadModule] = useUnloadModuleMutation();
  const [loadModule] = useLoadModuleMutation();
  const [enableModule] = useEnableModuleMutation();
  const [disableModule] = useDisableModuleMutation();
  const [busy, setBusy] = useState(false);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const hideDefaults = useAppSelector((state) => state.dashboard.hideDefaults);

  const isFetching =
    isFetchingModules || isFetchingQueries || isFetchingAliases || isFetchingSettings;

  // Hide legacy `checkXXX` aliases â€” the canonical names use the underscored
  // `check_XXX` form. Anything that starts with `check` but isn't `check_â€¦`
  // is treated as a legacy alias and excluded from the list.
  const isLegacyCheckAlias = (name: string) =>
    name.startsWith("check") && !name.startsWith("check_");
  const myQueries = queries?.filter(
    (query) => query.plugin === id && !isLegacyCheckAlias(query.name),
  );
  const myAliases = aliases?.filter((alias) => alias.plugin === id && !isLegacyCheckAlias(alias.name));
  const relevantSettings =
    settings?.filter(
      (setting) =>
        (showAdvanced || !setting.is_advanced_key) &&
        !setting.is_sample_key &&
        !setting.is_template_key,
    ) || [];
  const mySettings = relevantSettings.filter((setting) => setting.plugins.includes(id));

  const onRefresh = () => {
    dispatch(
      nsclientApi.util.invalidateTags([
        "Module",
        "Queries",
        "Aliases",
        "SettingsDescriptions",
      ]),
    );
  };

  const doEnable = async () => {
    setBusy(true);
    await enableModule(id || "").unwrap();
    dispatch(nsclientApi.util.invalidateTags(["SettingsStatus"]));
    setBusy(false);
  };
  const doDisable = async () => {
    setBusy(true);
    await disableModule(id || "").unwrap();
    dispatch(nsclientApi.util.invalidateTags(["SettingsStatus"]));
    setBusy(false);
  };

  const doUnload = async () => {
    setBusy(true);
    await unloadModule(id || "").unwrap();
    setBusy(false);
  };
  const doLoad = async () => {
    setBusy(true);
    await loadModule(id || "").unwrap();
    setBusy(false);
  };
  const doLoadAndEnable = async () => {
    setBusy(true);
    await loadModule(id || "").unwrap();
    await enableModule(id || "").unwrap();
    dispatch(nsclientApi.util.invalidateTags(["SettingsStatus"]));
    setBusy(false);
  };

  const isBusy = busy || isFetching;
  const showInfo = !module?.loaded || !module?.enabled;

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Trail trail={[{ link: "/modules", title: "Modules" }]} title={module?.title || id} />
        <Spacing />
        <FormControlLabel
          control={
            <Switch
              size="small"
              checked={showAdvanced}
              onChange={(e) => setShowAdvanced(e.target.checked)}
            />
          }
          label="Show advanced"
        />
        <FormControlLabel
          control={
            <Switch
              size="small"
              checked={hideDefaults}
              onChange={(e) => dispatch(setHideDefaults(e.target.checked))}
            />
          }
          label="Hide defaults"
        />
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <Card>
        <CardContent>
          <Typography gutterBottom sx={{ color: "text.secondary", fontSize: 14 }}>
            {module?.title}
          </Typography>
          <Typography variant="body2" component="div">
            {module?.description}
          </Typography>
        </CardContent>
        <CardActions>
          <Stack direction="row" spacing={1}>
            {module?.loaded && (
              <Button onClick={doUnload} disabled={isBusy}>
                Unload
              </Button>
            )}
            {!module?.loaded && (
              <Button onClick={doLoad} disabled={isBusy}>
                Load
              </Button>
            )}
            {!module?.loaded && (
              <Button onClick={doLoadAndEnable} disabled={isBusy}>
                Load & Enable
              </Button>
            )}
            {module?.enabled && (
              <Button onClick={doDisable} disabled={isBusy}>
                Disable
              </Button>
            )}
            {module?.loaded && !module?.enabled && (
              <Button onClick={doEnable} disabled={isBusy}>
                Enable
              </Button>
            )}
            {showInfo && (
              <NscpAlert
                severity="info"
                text="A loaded module can be used, an enabled module is loaded if you restart."
              />
            )}
          </Stack>
        </CardActions>
      </Card>
      {!module?.loaded && (
        <NscpAlert severity="warning" text="Cannot show queries and settings for unloaded modules." />
      )}

      {myQueries && myQueries.length > 0 && (
        <Card>
          <CardContent>
            <Typography gutterBottom sx={{ color: "text.secondary", fontSize: 14 }}>
              Queries provided by this module
            </Typography>
            <Box sx={{ display: "flex", flexWrap: "wrap", gap: 1, paddingBottom: 2 }}>
              {myQueries?.map((query) => (
                <Tooltip key={query.name} title={query.description || ""} arrow>
                  <Chip label={query.name} size="small" onClick={() => navigate("/queries/" + query.name)} />
                </Tooltip>
              ))}
            </Box>
          </CardContent>
        </Card>
      )}
      {myAliases && myAliases.length > 0 && (
        <Card>
          <CardContent>
            <Typography gutterBottom sx={{ color: "text.secondary", fontSize: 14 }}>
              Aliases provided by this module
            </Typography>
            <Box sx={{ display: "flex", flexWrap: "wrap", gap: 1, paddingBottom: 2 }}>
              {myAliases.map((alias) => (
                <Tooltip key={alias.name} title={alias.description || ""} arrow>
                  <Chip
                    label={alias.name}
                    size="small"
                    onClick={() => navigate("/queries/" + alias.name)}
                  />
                </Tooltip>
              ))}
            </Box>
          </CardContent>
        </Card>
      )}
      <ModuleSettings settings={mySettings} moduleId={id} showAdvanced={showAdvanced} />
    </Stack>
  );
}
