import { useGetInfoQuery } from "../api/api.ts";
import { List, ListItem, ListItemText } from "@mui/material";

export default function Welcome() {
  const { data: info } = useGetInfoQuery();
  return (
    <List>
      <ListItem>
        <ListItemText primary={`${info?.name}`} />
      </ListItem>
      <ListItem>
        <ListItemText primary="Version" secondary={info?.version} />
      </ListItem>
    </List>
  );
}
