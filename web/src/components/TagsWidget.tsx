import { Card, CardContent, Chip, Stack, Tooltip } from "@mui/material";
import Grid from "@mui/material/Grid";
import Typography from "@mui/material/Typography";
import { useGetTagsQuery } from "../api/api.ts";

/**
 * Host tags: key=value facts contributed by modules through the tag API
 * (e.g. `drives=c:,d:` from CheckDisk, `sqlserver=detected` from
 * CheckSystem). Renders its own grid cell so the dashboard shows no gap
 * while no module has published a tag.
 */
export default function TagsWidget() {
  const { data: tags } = useGetTagsQuery();

  const entries = Object.entries(tags ?? {}).sort(([a], [b]) => a.localeCompare(b));
  if (entries.length === 0) return null;

  return (
    <Grid size={{ xs: 12, md: 6 }}>
      <Card variant="outlined" sx={{ height: "100%" }}>
        <CardContent>
          <Typography gutterBottom variant="h5" component="div">
            Tags
          </Typography>
          <Stack direction="row" spacing={1} useFlexGap sx={{ flexWrap: "wrap" }}>
            {entries.map(([key, value]) => (
              <Tooltip key={key} title={`${key}=${value}`} placement="top">
                <Chip label={`${key}: ${value}`} variant="outlined" size="small" />
              </Tooltip>
            ))}
          </Stack>
        </CardContent>
      </Card>
    </Grid>
  );
}
