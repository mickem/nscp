import Stack from "@mui/material/Stack";
import {
  nsclientApi,
  useDisableModuleMutation,
  useEnableModuleMutation,
  useGetModuleQuery,
  useGetQueriesQuery,
  useGetSettingsDescriptionsQuery,
  useLoadModuleMutation,
  useUnloadModuleMutation,
} from "../api/api.ts";
import {
  Accordion,
  AccordionDetails,
  AccordionSummary,
  Card,
  CardActions,
  CardContent,
  List,
  ListItem,
  ListItemButton,
} from "@mui/material";
import { useNavigate, useParams } from "react-router";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { useState } from "react";
import Typography from "@mui/material/Typography";
import Button from "@mui/material/Button";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import ModuleSettings from "./ModuleSettings.tsx";
import NscpAlert from "./atoms/NscpAlert.tsx";

export default function Module() {
  const { id = "" } = useParams();
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: module, isFetching: isFetchingModules } = useGetModuleQuery(id || "");
  const { data: queries, isFetching: isFetchingQueries } = useGetQueriesQuery();
  const { data: settings, isFetching: isFetchingSettings } = useGetSettingsDescriptionsQuery();
  const [unloadModule] = useUnloadModuleMutation();
  const [loadModule] = useLoadModuleMutation();
  const [enableModule] = useEnableModuleMutation();
  const [disableModule] = useDisableModuleMutation();
  const [busy, setBusy] = useState(false);

  const isFetching = isFetchingModules || isFetchingQueries || isFetchingSettings;

  const myQueries = queries?.filter((query) => query.plugin === id);
  const relevantSettings =
    settings?.filter((setting) => !setting.is_advanced_key && !setting.is_sample_key && !setting.is_template_key) || [];
  const mySettings = relevantSettings.filter((setting) => setting.plugins.includes(id));

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Module", "Queries", "SettingsDescriptions"]));
  };

  const doEnable = async () => {
    setBusy(true);
    await enableModule(id || "").unwrap();
    setBusy(false);
  };
  const doDisable = async () => {
    setBusy(true);
    await disableModule(id || "").unwrap();
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
    setBusy(false);
  };

  const isBusy = busy || isFetching;

  return (
    <Stack direction="column" spacing={3}>
      <Toolbar>
        <Spacing />
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
              <Button onClick={doLoadAndEnable} disabled={isBusy}>
                Load & Enable
              </Button>
            )}
            {!module?.loaded && (
              <Button onClick={doLoad} disabled={isBusy}>
                Load
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
          </Stack>
        </CardActions>
      </Card>
      {!module?.loaded && (
        <NscpAlert severity="warning" text="Cannot show queries and settings for unloaded modules." />
      )}

      {myQueries && myQueries.length > 0 && (
        <Accordion>
          <AccordionSummary expandIcon={<ExpandMoreIcon />}>
            <Typography>Queries</Typography>
          </AccordionSummary>
          <AccordionDetails>
            <List>
              {myQueries?.map((query) => (
                <ListItem key={query.name}>
                  <ListItemButton onClick={() => navigate(`/queries/${query.name}`)}>
                    <Typography>{query.name}</Typography>
                  </ListItemButton>
                </ListItem>
              ))}
            </List>
          </AccordionDetails>
        </Accordion>
      )}
      <ModuleSettings settings={mySettings} />
    </Stack>
  );
}
