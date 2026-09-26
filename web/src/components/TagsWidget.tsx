import { Card, CardContent, Chip, Stack, Tooltip } from "@mui/material";
import Grid from "@mui/material/Grid";
import Typography from "@mui/material/Typography";
import { useGetTagsQuery } from "../api/api.ts";
import { ShowMore } from "./atoms/ShowMore.tsx";
import { useCapped } from "./atoms/useCapped.ts";

/**
 * Host tags: key=value facts contributed by modules through the tag API
 * (e.g. `drives=c:,d:` from CheckDisk, `sqlserver=detected` from
 * CheckSystem). Renders its own grid cell so the dashboard shows no gap
 * while no module has published a tag.
 */
/** How many tags the widget shows before it offers the rest. */
const PREVIEW_TAGS = 24;

export default function TagsWidget() {
  const { data: tags } = useGetTagsQuery();

  const entries = Object.entries(tags ?? {}).sort(([a], [b]) => a.localeCompare(b));
  // Modules publish as many tags as they like, and a host with a hundred of
  // them would own the whole dashboard.
  const { shown, hidden, showAll, toggle } = useCapped(entries, PREVIEW_TAGS);

  if (entries.length === 0) return null;

  return (
    <Grid size={{ xs: 12, md: 6 }}>
      <Card variant="outlined" sx={{ height: "100%" }}>
        <CardContent>
          <Typography gutterBottom variant="h5" component="div">
            Tags
          </Typography>
          <Stack direction="row" spacing={1} useFlexGap sx={{ flexWrap: "wrap", alignItems: "center" }}>
            {shown.map(([key, value]) => (
              <Tooltip key={key} title={`${key}=${value}`} placement="top">
                <Chip label={`${key}: ${value}`} variant="outlined" size="small" />
              </Tooltip>
            ))}
            <ShowMore hidden={hidden} showAll={showAll} onToggle={toggle} />
          </Stack>
        </CardContent>
      </Card>
    </Grid>
  );
}
