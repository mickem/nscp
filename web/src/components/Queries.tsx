import Stack from "@mui/material/Stack";
import { nsclientApi, useGetQueriesQuery } from "../api/api.ts";
import { List, ListItem, ListItemButton, ListItemText } from "@mui/material";
import { useNavigate } from "react-router";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import Trail from "./atoms/Trail.tsx";

export default function Queries() {
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const { data: queries } = useGetQueriesQuery();

  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Queries"]));
  };

  return (
    <Stack direction="column">
      <Toolbar>
        <Trail title="Queries"/>
        <Spacing />
        <RefreshButton onRefresh={onRefresh} />
      </Toolbar>
      <List sx={{ width: "100%" }}>
        {queries?.map((query) => (
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
