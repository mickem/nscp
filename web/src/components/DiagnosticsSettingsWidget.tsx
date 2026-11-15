import { Card, CardContent, Typography } from "@mui/material";
import { useMemo } from "react";
import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import SettingSwitchSections, { groupSwitchSections, isOn, SwitchSection } from "./SettingSwitchSections.tsx";

/**
 * The diagnostics tables this agent can collect, and the switches that turn
 * them on.
 *
 * There are two levels. The core's `[/settings/core/failed checks] capture
 * diagnostics` decides whether diagnostics are collected at all, and each
 * table is then switched on in the module that produces it -
 * `[/settings/system/windows/diagnostics] processes` for CheckSystem,
 * `[/settings/disk/diagnostics] growth spots` for CheckDisk. As with the fact
 * sets, the widget finds the tables rather than listing them: every settings
 * section called `diagnostics`, whichever module registered it, with its
 * boolean keys as the rows.
 */

const CAPTURE_PATH = "/settings/core/failed checks";
const CAPTURE_KEY = "capture diagnostics";

function isCaptureSwitch(entry: SettingsDescription): boolean {
  return entry.path === CAPTURE_PATH && entry.key === CAPTURE_KEY;
}

function isTableSwitch(entry: SettingsDescription): boolean {
  // Only the bools are tables: the same section carries settings about a
  // table, such as CheckDisk's `growth spots interval`.
  return entry.path.endsWith("/diagnostics") && entry.key !== "" && entry.type === "bool";
}

/**
 * The capture switch first, in a section of its own, then the table switches
 * grouped by the section they live in.
 */
export function groupDiagnosticsSections(descriptions: SettingsDescription[]): SwitchSection[] {
  return [...groupSwitchSections(descriptions, isCaptureSwitch), ...groupSwitchSections(descriptions, isTableSwitch)];
}

export default function DiagnosticsSettingsWidget() {
  const { data: descriptions } = useGetSettingsDescriptionsQuery();
  const sections = useMemo(() => groupDiagnosticsSections(descriptions ?? []), [descriptions]);

  // No settings to show (the descriptions have not arrived, or could not be
  // read): the page says how to configure it by hand.
  if (sections.length === 0) return null;

  const capture = (descriptions ?? []).find(isCaptureSwitch);
  const tablesOn = sections.some((section) => section.keys.some((entry) => isTableSwitch(entry) && isOn(entry)));

  return (
    <Card variant="outlined">
      <CardContent>
        <Typography gutterBottom variant="h6" component="div">
          Diagnostics tables
        </Typography>
        {/* Writing the setting is not collecting: the core and the modules
            re-read it when the configuration is reloaded, which the unsaved
            configuration banner at the top of the page offers. */}
        <Typography variant="body2" color="text.secondary">
          Nothing is collected until <code>capture diagnostics</code> is on, which alone gives the last failed checks;
          every other table is turned on in the module that produces it. A change takes effect when the configuration
          is saved and reloaded.
        </Typography>
        {capture !== undefined && !isOn(capture) && tablesOn && (
          <Typography variant="body2" color="warning.main" sx={{ mt: 1 }}>
            Tables are turned on, but <code>capture diagnostics</code> is off: none of them is collected.
          </Typography>
        )}
        <SettingSwitchSections
          sections={sections}
          // `errors` and `services` are keys in more than one module.
          label={(section, entry) => `${section.owner}: ${entry.key}`}
        />
      </CardContent>
    </Card>
  );
}
