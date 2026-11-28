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
import ExperimentalChip, { EXPERIMENTAL_TOOLTIP } from "../components/atoms/ExperimentalChip.tsx";
import { getModulePage } from "../modules/registry.ts";

const EXPERT_TOOLTIP =
  "Show every setting the module registers, advanced ones included, as the schema describes it, " +
  "without the tailored layout.";

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
  // Expert mode is chosen per module: on by default only for a module that
  // has no tailored page, where the generic form is all there is.
  const hasPage = getModulePage(id) !== undefined;
  const [expertByModule, setExpertByModule] = useState<Record<string, boolean>>({});
  const expert = expertByModule[id] ?? !hasPage;
  const setExpert = (on: boolean) => setExpertByModule((m) => ({ ...m, [id]: on }));
  const hideDefaults = useAppSelector((state) => state.dashboard.hideDefaults);

  const isFetching =
    isFetchingModules || isFetchingQueries || isFetchingAliases || isFetchingSettings;

  // Hide legacy `checkXXX` aliases — the canonical names use the underscored
  // `check_XXX` form. Anything that starts with `check` but isn't `check_…`
  // is treated as a legacy alias and excluded from the list.
  const isLegacyCheckAlias = (name: string) =>
    name.startsWith("check") && !name.startsWith("check_");
  const myQueries = queries?.filter(
    (query) => query.plugin === id && !isLegacyCheckAlias(query.name),
  );
  const myAliases = aliases?.filter(
    (alias) => alias.plugin === id && !isLegacyCheckAlias(alias.name),
  );
  const relevantSettings =
    settings?.filter(
      (setting) =>
        (expert || !setting.is_advanced_key) && !setting.is_sample_key && !setting.is_template_key,
    ) || [];
  const mySettings = relevantSettings.filter((setting) => setting.plugins.includes(id));
  // A module whose checks take everything as arguments registers nothing:
  // no form, no expert mode, nothing to filter.
  const noSettings =
    !hasPage &&
    settings !== undefined &&
    !settings.some(
      (s) => s.plugins.includes(id) && !s.is_sample_key && !s.is_template_key,
    );

  const onRefresh = () => {
    dispatch(
      nsclientApi.util.invalidateTags(["Module", "Queries", "Aliases", "SettingsDescriptions"]),
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
        {!noSettings && (
          <Tooltip title={EXPERT_TOOLTIP} arrow describeChild>
            <FormControlLabel
              control={
                <Switch
                  size="small"
                  checked={expert}
                  onChange={(e) => setExpert(e.target.checked)}
                />
              }
              label="Expert mode"
            />
          </Tooltip>
        )}
        {/* Filters only the generic settings form: the one in expert mode,
            and the only one a module without a page of its own has. */}
        {(expert || !hasPage) && !noSettings && (
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
        )}
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <Card>
        <CardContent>
          <Stack direction="row" spacing={1} sx={{ alignItems: "center", paddingBottom: 1 }}>
            <Typography sx={{ color: "text.secondary", fontSize: 14 }}>{module?.title}</Typography>
            {module?.experimental && <ExperimentalChip />}
          </Stack>
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
        <NscpAlert
          severity="warning"
          text="Cannot show queries and settings for unloaded modules."
        />
      )}

      {myQueries && myQueries.length > 0 && (
        <Card>
          <CardContent>
            <Typography gutterBottom sx={{ color: "text.secondary", fontSize: 14 }}>
              Queries provided by this module
            </Typography>
            <Box sx={{ display: "flex", flexWrap: "wrap", gap: 1, paddingBottom: 2 }}>
              {myQueries?.map((query) => (
                <Tooltip
                  key={query.name}
                  title={
                    query.experimental
                      ? `${query.description || ""}\n${EXPERIMENTAL_TOOLTIP}`
                      : query.description || ""
                  }
                  arrow
                >
                  <Chip
                    label={query.experimental ? `${query.name} (experimental)` : query.name}
                    size="small"
                    color={query.experimental ? "warning" : "default"}
                    variant={query.experimental ? "outlined" : "filled"}
                    onClick={() => navigate("/queries/" + query.name)}
                  />
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
      {module?.loaded && expert && hasPage && (
        <NscpAlert
          severity="info"
          text="Expert mode: every setting this module registers, advanced ones included, without the tailored layout."
          actionTitle="Back to the tailored page"
          onClick={() => setExpert(false)}
        />
      )}
      {module?.loaded && noSettings && (
        <NscpAlert
          severity="info"
          text="This module has no settings: its checks take everything they need as arguments."
        />
      )}
      {module?.loaded && expert && !hasPage && !noSettings && (
        <NscpAlert
          severity="info"
          text="This module has no tailored settings page yet, so every setting it registers is shown as the schema describes it, advanced ones included."
        />
      )}
      {/* An unloaded module cannot be configured from here, in either view: its
          page would read and write settings nothing is using. */}
      {module?.loaded && !noSettings && (
        <ModuleSettings
          key={id}
          settings={mySettings}
          moduleId={id}
          expert={expert}
          onExpertMode={() => setExpert(true)}
        />
      )}
    </Stack>
  );
}
