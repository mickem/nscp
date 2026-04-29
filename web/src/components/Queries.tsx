import Stack from "@mui/material/Stack";
import { nsclientApi, useGetQueriesQuery } from "../api/api.ts";
import { List, ListItem, ListItemButton, ListItemText, Typography } from "@mui/material";
import { useNavigate } from "react-router";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import Trail from "./atoms/Trail.tsx";
import FilterField from "./atoms/FilterField.tsx";
import { useMemo, useState } from "react";

export default function Queries() {
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: queries } = useGetQueriesQuery();
  const [filter, setFilter] = useState<string>("");

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Queries"]));
  };

  const needle = filter.trim().toLowerCase();
  const filtered = useMemo(() => {
    if (!queries) return [];
    if (!needle) return queries;
    return queries.filter((q) =>
      [q.name, q.description].some((f) => (f ?? "").toLowerCase().includes(needle)),
    );
  }, [queries, needle]);

  return (
    <Stack direction="column">
      <Toolbar>
        <Trail title="Queries" />
        <Spacing />
        <FilterField value={filter} onChange={setFilter} placeholder="Filter checks…" />
        {needle && (
          <Typography variant="body2" color="text.secondary">
            {filtered.length}/{queries?.length ?? 0}
          </Typography>
        )}
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <List sx={{ width: "100%" }}>
        {filtered.length === 0 && needle && (
          <Typography variant="body2" color="text.secondary" sx={{ p: 2 }}>
            No checks match “{filter}”.
          </Typography>
        )}
        {filtered.map((query) => (
          <ListItem key={query.name} alignItems="flex-start">
            <ListItemButton onClick={() => navigate(`/queries/${query.name}`)} dense>
              <ListItemText primary={query.name} secondary={query.description} />
            </ListItemButton>
          </ListItem>
        ))}
      </List>
    </Stack>
  );
}
