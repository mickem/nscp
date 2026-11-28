import Stack from "@mui/material/Stack";
import { ModuleListItem, nsclientApi, useGetModulesQuery } from "../api/api.ts";
import { Chip, List, ListItem, ListItemButton, ListItemIcon, ListItemText, Typography } from "@mui/material";
import { useNavigate, useSearchParams } from "react-router";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import CheckIcon from "@mui/icons-material/Check";
import DoneAllIcon from "@mui/icons-material/DoneAll";
import CheckBoxOutlineBlankIcon from "@mui/icons-material/CheckBoxOutlineBlank";
import Trail from "../components/atoms/Trail.tsx";
import ExperimentalChip from "../components/atoms/ExperimentalChip.tsx";
import FilterField from "../components/atoms/FilterField.tsx";
import { useMemo, useState } from "react";
import { groupsOf, MODULE_GROUPS, OTHER_GROUP } from "../modules/groups.ts";

export default function Modules() {
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: modules } = useGetModulesQuery({ all: true });
  const [filter, setFilter] = useState<string>("");
  // The groups picked, kept in the URL so coming back from a module keeps them.
  const [params, setParams] = useSearchParams();
  const picked = useMemo(
    () => new Set((params.get("group") ?? "").split(",").filter(Boolean)),
    [params],
  );
  const toggleGroup = (id: string) => {
    const next = new Set(picked);
    if (next.has(id)) next.delete(id);
    else next.add(id);
    setParams(next.size > 0 ? { group: [...next].join(",") } : {}, { replace: true });
  };

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
  // Groups shown: those with a module present, and Other only when needed.
  const groups = useMemo(() => {
    const present = new Set((modules ?? []).flatMap((m) => groupsOf(m.metadata?.category)));
    return [...MODULE_GROUPS, OTHER_GROUP]
      .filter((g) => present.has(g.id))
      .map((g) => ({
        ...g,
        count: (modules ?? []).filter((m) => groupsOf(m.metadata?.category).includes(g.id)).length,
      }));
  }, [modules]);
  const filtered = useMemo(() => {
    if (!modules) return [];
    const inGroup =
      picked.size === 0 ? modules : modules.filter((m) => groupsOf(m.metadata?.category).some((g) => picked.has(g)));
    if (!needle) return inGroup;
    return inGroup.filter((m) =>
      // "experimental" is matched as if it were a word in the row, so the
      // filter field doubles as a way to list what is still moving.
      [m.name, m.id, m.description, m.experimental ? "experimental" : ""].some((f) =>
        (f ?? "").toLowerCase().includes(needle),
      ),
    );
  }, [modules, needle, picked]);

  return (
    <Stack direction="column">
      <Toolbar>
        <Trail title="Modules" />
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter modules" />
        {(needle || picked.size > 0) && (
          <Typography variant="body2" color="text.secondary">
            {filtered.length}/{modules?.length ?? 0}
          </Typography>
        )}
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <Stack
        direction="row"
        spacing={1}
        useFlexGap
        sx={{ flexWrap: "wrap", px: 2, pt: 1 }}
        role="group"
        aria-label="Module groups"
      >
        {groups.map((g) => (
          <Chip
            key={g.id}
            label={`${g.label} (${g.count})`}
            color={picked.has(g.id) ? "primary" : "default"}
            variant={picked.has(g.id) ? "filled" : "outlined"}
            onClick={() => toggleGroup(g.id)}
            aria-pressed={picked.has(g.id)}
          />
        ))}
        {picked.size > 0 && (
          <Chip label="All" variant="outlined" onClick={() => setParams({}, { replace: true })} />
        )}
      </Stack>
      <List sx={{ width: "100%" }}>
        {filtered.length === 0 && (needle || picked.size > 0) && (
          <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
            {needle ? `No modules match “${filter}”.` : "No modules in the groups picked."}
          </Typography>
        )}
        {filtered.map((module) => (
          <ListItem key={module.id} alignItems="flex-start">
            <ListItemButton onClick={() => navigate(`/modules/${module.id}`)} dense>
              <ListItemIcon>{getIcon(module)}</ListItemIcon>
              <ListItemText
                primary={
                  <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
                    <span>{module.name}</span>
                    {module.experimental && <ExperimentalChip dense />}
                  </Stack>
                }
                secondary={module.description}
              />
            </ListItemButton>
          </ListItem>
        ))}
      </List>
    </Stack>
  );
}
