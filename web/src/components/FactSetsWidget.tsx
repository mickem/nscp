import { Card, CardContent, Typography } from "@mui/material";
import { useMemo } from "react";
import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import SettingSwitchSections, { groupSwitchSections, SwitchSection } from "./SettingSwitchSections.tsx";

/**
 * The fact sets this agent can collect, and the switch that turns each on.
 *
 * Facts are opt-in per set, and each switch lives in the settings of the
 * module that produces it — `[/settings/disk/facts] storage.volumes` for
 * CheckDisk, `[/settings/system/windows/facts] os` for CheckSystem, and the
 * core's own `[/settings/facts] agent`. That is a good rule for the agent (a
 * switch cannot exist without the module that honours it) and a bad one for
 * an operator, who would otherwise have to visit one module page per set to
 * find out what this host could be reporting and does not.
 *
 * So the widget finds the sets rather than listing them: every settings
 * section called `facts`, whichever module registered it, with its boolean
 * keys as the rows. A module added later needs nothing here — if it registers
 * `[/settings/<something>/facts] <set> = true`, its set appears.
 */

function isFactSwitch(entry: SettingsDescription): boolean {
  // The section, not the key, is what marks a fact set: the key is the set's
  // own dotted id (`software.installed`), which is not a name this can match
  // on. Only the bools are switches - the same section carries the core's
  // `interval` and `max size`, which are settings about facts rather than
  // sets of them.
  return entry.path.endsWith("/facts") && entry.key !== "" && entry.type === "bool";
}

/** The fact switches, grouped by the section they live in. */
export function groupFactSections(descriptions: SettingsDescription[]): SwitchSection[] {
  return groupSwitchSections(descriptions, isFactSwitch);
}

export default function FactSetsWidget() {
  const { data: descriptions } = useGetSettingsDescriptionsQuery();
  const sections = useMemo(() => groupFactSections(descriptions ?? []), [descriptions]);

  // Nothing loaded produces facts. Rendering an empty card would leave an
  // operator looking for the switch that is not there.
  if (sections.length === 0) return null;

  return (
    <Card variant="outlined">
      <CardContent>
        <Typography gutterBottom variant="h6" component="div">
          Fact sets
        </Typography>
        {/* Writing the setting is not collecting: the module re-reads which
            sets it produces when the configuration is reloaded. The unsaved
            configuration banner at the top of the page is how that is done,
            so this says what to expect rather than offering a second button
            for it. */}
        <Typography variant="body2" color="text.secondary">
          Nothing is collected until you turn a set on. A set starts (or stops) collecting when the configuration is
          saved and reloaded.
        </Typography>
        <SettingSwitchSections sections={sections} />
      </CardContent>
    </Card>
  );
}
