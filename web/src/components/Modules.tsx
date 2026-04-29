import Stack from "@mui/material/Stack";
import { ModuleListItem, nsclientApi, useGetModulesQuery } from "../api/api.ts";
import { List, ListItem, ListItemButton, ListItemIcon, ListItemText, Typography } from "@mui/material";
import { useNavigate } from "react-router";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import CheckIcon from "@mui/icons-material/Check";
import DoneAllIcon from "@mui/icons-material/DoneAll";
import CheckBoxOutlineBlankIcon from "@mui/icons-material/CheckBoxOutlineBlank";
import Trail from "./atoms/Trail.tsx";
import FilterField from "./atoms/FilterField.tsx";
import { useMemo, useState } from "react";

export default function Modules() {
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: modules } = useGetModulesQuery({ all: true });
  const [filter, setFilter] = useState<string>("");

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

  const needle = filter.trim().toLowerCase();
  const filtered = useMemo(() => {
    if (!modules) return [];
    if (!needle) return modules;
    return modules.filter((m) =>
      [m.name, m.id, m.description].some((f) => (f ?? "").toLowerCase().includes(needle)),
    );
  }, [modules, needle]);

  return (
    <Stack direction="column">
      <Toolbar>
        <Trail title="Modules" />
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter modules…" />
        {needle && (
          <Typography variant="body2" color="text.secondary">
            {filtered.length}/{modules?.length ?? 0}
          </Typography>
        )}
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <List sx={{ width: "100%" }}>
        {filtered.length === 0 && needle && (
          <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
            No modules match “{filter}”.
          </Typography>
        )}
        {filtered.map((module) => (
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
