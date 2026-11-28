import { Alert, Button, Chip, Stack, Tooltip, Typography } from "@mui/material";
import { useState } from "react";
import CollapsibleCard from "./atoms/CollapsibleCard.tsx";
import { Spacing } from "./atoms/Spacing.tsx";
import FactTree from "./FactTree.tsx";
import { Facts, OnDemandFactSet, useGetFactSetsQuery, useLazyGetFactPathQuery } from "../api/api.ts";

/**
 * The fact sets the agent collects only when asked: the scripts each runtime
 * has, the performance counter catalog.
 *
 * They are not in the document the rest of the page shows, and that is the
 * point of them — a counter catalog is thousands of entries and the same on
 * every host of a kind, and a scripts folder changes whenever someone uploads
 * one. So nothing here is fetched until its button is pressed, and what comes
 * back was read off the machine for that press (or a few seconds before it).
 *
 * Each set takes a grant of its own (`facts.get.<set>`): being able to read
 * the inventory does not make a role able to read a script listing. A refusal
 * is therefore an ordinary answer here, shown with the grant it wants.
 */

function timeLabel(iso: string | undefined): string {
  if (!iso) return "";
  const when = new Date(iso);
  return isNaN(when.getTime()) ? iso : when.toLocaleString();
}

type Reading = { state: "loaded"; facts: Facts } | { state: "refused" } | { state: "failed" };

function OnDemandSet({ set }: { set: OnDemandFactSet }) {
  const [trigger, { isFetching }] = useLazyGetFactPathQuery();
  const [reading, setReading] = useState<Reading | undefined>(undefined);

  const load = () => {
    // Not from the client cache: pressing the button means "read it now", and
    // the agent keeps its own few seconds of cache for exactly this.
    trigger(set.id, false)
      .unwrap()
      .then((facts) => setReading({ state: "loaded", facts }))
      .catch((error: { status?: number }) => setReading({ state: error?.status === 403 ? "refused" : "failed" }));
  };

  const facts = reading?.state === "loaded" ? reading.facts : undefined;
  const error = facts?.errors?.[set.id];

  return (
    <Stack spacing={1} sx={{ py: 1 }}>
      <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
        <Typography variant="subtitle1" component="div">
          {set.id}
        </Typography>
        <Tooltip title="The privilege that reads this set" placement="bottom">
          <Chip size="small" variant="outlined" label={set.privilege} />
        </Tooltip>
        <Spacing />
        {facts?.gathered?.[set.id] !== undefined && (
          <Typography variant="caption" color="text.secondary">
            gathered {timeLabel(facts.gathered[set.id])}
          </Typography>
        )}
        <Button size="small" variant="outlined" onClick={load} loading={isFetching} aria-label={`Load ${set.id}`}>
          {reading === undefined ? "Load" : "Reload"}
        </Button>
      </Stack>
      {reading?.state === "refused" && (
        <Alert severity="info">
          Your role may not read this set. It takes the <code>{set.privilege}</code> privilege.
        </Alert>
      )}
      {reading?.state === "failed" && <Alert severity="error">Could not read {set.id}.</Alert>}
      {error !== undefined && <Alert severity="warning">{error}</Alert>}
      {facts !== undefined && !facts.found && error === undefined && (
        <Alert severity="info">Nothing produced {set.id}: its module is no longer loaded.</Alert>
      )}
      {facts !== undefined && facts.found && <FactTree node={facts.facts} />}
    </Stack>
  );
}

export default function OnDemandFactsWidget() {
  const { data: sets } = useGetFactSetsQuery();
  // A parameterized set is read for one object at a time
  // (`pdh.instances.<object>`), which is a picker's job, not a button's.
  const listed = (sets?.on_demand ?? []).filter((set) => !set.parameterized);

  // No loaded module declares one, or the role may not list them. An empty
  // card would only raise the question of what is supposed to be in it.
  if (listed.length === 0) return null;

  return (
    <CollapsibleCard title="On demand" storageKey="facts.on_demand" defaultExpanded={false}>
      <Typography variant="body2" color="text.secondary">
        Collected when you ask, and never stored or sent to a fleet server. There is nothing to turn on: a set is
        available while the module that produces it is loaded.
      </Typography>
      {listed.map((set) => (
        <OnDemandSet key={set.id} set={set} />
      ))}
    </CollapsibleCard>
  );
}
