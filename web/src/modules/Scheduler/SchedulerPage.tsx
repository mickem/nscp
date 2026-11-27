// The scheduler page: how many checks run at once, and when. The schedules
// themselves follow it.
import { Stack } from "@mui/material";
import PanelShell, { KeyField, PanelSection } from "../kit/PanelShell.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import { SCHEDULER_MODULE, SCHEDULER_PATH, SCHEDULER_TABLE, schedulerIni } from "./scheduler.ts";

export default function SchedulerPage() {
  const panel = usePanel(SCHEDULER_MODULE, SCHEDULER_TABLE);
  const changes = panel.pending();
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={SCHEDULER_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  return (
    <PanelShell
      title="Scheduler"
      subheader="Runs checks on a schedule and submits their results, for passive monitoring."
      panel={panel}
      changes={changes}
      problem={null}
      ini={schedulerIni}
    >
      <PanelSection title="Running">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "threads",
            label: "Checks at once",
            kind: "number",
            help: "Worker threads; a slow check holds one.",
          })}
          {f({
            key: "startup window",
            label: "Spread startup runs over",
            kind: "text",
            help: "Schedules that run on startup are spaced evenly over this time (30s, 5m).",
          })}
          {f({
            key: "timezone",
            label: "Timezone",
            kind: "text",
            help: "Of the cron schedules: local, utc, or a POSIX TZ string.",
          })}
        </Stack>
      </PanelSection>
    </PanelShell>
  );
}
