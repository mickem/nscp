import Stack from "@mui/material/Stack";
import { ModuleListItem, nsclientApi, useGetModulesQuery } from "../api/api.ts";
import { List, ListItem, ListItemButton, ListItemIcon, ListItemText } from "@mui/material";
import { useNavigate } from "react-router";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import CheckIcon from "@mui/icons-material/Check";
import DoneAllIcon from "@mui/icons-material/DoneAll";
import CheckBoxOutlineBlankIcon from "@mui/icons-material/CheckBoxOutlineBlank";

export default function Modules() {
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: modules } = useGetModulesQuery({ all: true });

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Modules"]));
  };
  const getIcon = (module: ModuleListItem) => {
    return module?.loaded && module?.enabled ? (
      <DoneAllIcon color="success" />
    ) : module?.loaded ? (
      <CheckIcon color="warning" />
    ) : (
      <CheckBoxOutlineBlankIcon />
    );
  };

  return (
    <Stack direction="column">
      <Toolbar>
        <Spacing />
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <List sx={{ width: "100%" }}>
        {modules?.map((module) => (
          <ListItem key={module.id} alignItems="flex-start">
            <ListItemButton onClick={() => navigate(`/modules/${module.id}`)} dense>
              <ListItemIcon>{getIcon(module)}</ListItemIcon>
              <ListItemText primary={module.name} secondary={module.description} />
            </ListItemButton>
          </ListItem>
        ))}
      </List>
    </Stack>
  );
}
