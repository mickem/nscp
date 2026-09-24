import { Alert, Card, CardContent, Chip, Stack, Tooltip, Typography } from "@mui/material";
import Grid from "@mui/material/Grid";
import { useGetFactsQuery, useRefreshFactsMutation } from "../api/api.ts";
import { Toolbar } from "../components/atoms/Toolbar.tsx";
import { Spacing } from "../components/atoms/Spacing.tsx";
import { RefreshButton } from "../components/atoms/RefreshButton.tsx";
import FactTree from "../components/FactTree.tsx";
import { FactValue } from "../api/api.ts";

/**
 * Host facts: the inventory the agent collects about the machine it runs on —
 * what OS it is, what hardware it sits on, what it has attached.
 *
 * Distinct from the tags on the dashboard, and the page says so, because the
 * difference decides what an operator should look for where: a tag is a flat
 * value a fleet groups hosts by, while a fact set is a document, and it is
 * collected only once someone turns it on.
 */

function timeLabel(iso: string): string {
  if (!iso) return "never";
  const when = new Date(iso);
  return isNaN(when.getTime()) ? iso : when.toLocaleString();
}

export default function Facts() {
  const { data: facts, isFetching, refetch } = useGetFactsQuery();
  const [refreshFacts, { isLoading: isRefreshing }] = useRefreshFactsMutation();

  const document = (facts?.facts ?? {}) as { [set: string]: FactValue };
  const sets = Object.entries(document).sort(([a], [b]) => a.localeCompare(b));
  const errors = Object.entries(facts?.errors ?? {});

  return (
    <Stack sx={{ width: "100%" }}>
      <Toolbar>
        <Typography variant="h6">Facts</Typography>
        {facts !== undefined && (
          <>
            <Chip size="small" variant="outlined" label={`revision ${facts.revision}`} />
            {/* When the core last asked, which is not when the values were
                read - a producer that caches answers every round with the
                snapshot it took earlier. Each card carries the age that
                actually matters, so this one says "checked". */}
            <Tooltip title="When the agent last asked its modules for facts" placement="bottom">
              <Chip size="small" variant="outlined" label={`checked ${timeLabel(facts.collected)}`} />
            </Tooltip>
          </>
        )}
        <Spacing />
        <RefreshButton
          onRefresh={() => {
            // Ask the agent to collect now, not just re-read what it stored:
            // an inventory refreshes on the hour by default, so re-fetching
            // alone would show the same document and look like a broken button.
            refreshFacts()
              .unwrap()
              .catch(() => refetch());
          }}
          isFetching={isFetching || isRefreshing}
        />
      </Toolbar>

      {errors.length > 0 && (
        <Alert severity="warning" sx={{ mx: 2, mb: 2 }}>
          {errors.map(([set, message]) => (
            <Stack key={set} direction="row" spacing={1}>
              <Typography variant="body2" sx={{ fontWeight: "bold" }}>
                {set}
              </Typography>
              <Typography variant="body2">{message}</Typography>
            </Stack>
          ))}
          {/* A failing set keeps the value it last collected, so what is shown
              below is still real — just older than the timestamp suggests. */}
          These sets kept the values from their last successful collection.
        </Alert>
      )}

      {sets.length === 0 && (
        <Alert severity="info" sx={{ mx: 2 }}>
          No facts collected. A fact set is enabled in the module that produces it — for example{" "}
          <code>[/settings/system/windows/facts] os = true</code> for the OS and hardware of this host. Nothing is
          collected until you turn a set on.
        </Alert>
      )}

      <Grid container spacing={2} sx={{ px: 2, pb: 2 }}>
        {sets.map(([name, value]) => (
          <Grid key={name} size={{ xs: 12, md: 6 }}>
            <Card variant="outlined" sx={{ height: "100%" }}>
              <CardContent>
                <Stack direction="row" spacing={1} sx={{ alignItems: "center", mb: 1 }}>
                  <Typography variant="h6" component="div">
                    {name}
                  </Typography>
                  {facts?.errors?.[name] !== undefined && <Chip size="small" color="warning" label="stale" />}
                  <Spacing />
                  {/* The age of these values, not of the round that carried
                      them. Falls back to the round for a producer that does
                      not say, which is right: it read them just now. */}
                  <Tooltip title="When these values were read from the machine" placement="bottom">
                    <Typography variant="caption" color="text.secondary">
                      gathered {timeLabel(facts?.gathered?.[name] ?? facts?.collected ?? "")}
                    </Typography>
                  </Tooltip>
                </Stack>
                <FactTree node={value} />
              </CardContent>
            </Card>
          </Grid>
        ))}
      </Grid>
    </Stack>
  );
}
