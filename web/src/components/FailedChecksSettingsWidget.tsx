import { List, Typography } from "@mui/material";
import { useMemo } from "react";
import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import CollapsibleCard from "./atoms/CollapsibleCard.tsx";
import SettingsItem from "./atoms/SettingsItem.tsx";

/**
 * The core's `[/settings/core/failed checks]` section, editable in place on
 * the page that shows the log it configures. Unlike the fact sets and the
 * diagnostics tables these are not all switches - how many to keep, which
 * statuses count, which arguments to record, the snapshot budget - so each key
 * gets the same editor it has on the settings page.
 */

export const FAILED_CHECKS_PATH = "/settings/core/failed checks";

/** The keys of the failed-check section, in the order the core registers them. */
export function failedCheckSettings(descriptions: SettingsDescription[]): SettingsDescription[] {
  return descriptions.filter((entry) => entry.path === FAILED_CHECKS_PATH && entry.key !== "");
}

export default function FailedChecksSettingsWidget() {
  const { data: descriptions } = useGetSettingsDescriptionsQuery();
  const keys = useMemo(() => failedCheckSettings(descriptions ?? []), [descriptions]);

  // The descriptions have not arrived or could not be read (a role without
  // settings access): the page still says which keys to set by hand.
  if (keys.length === 0) return null;

  return (
    // Folded by default: the log is what this page is for, and the settings
    // are set once.
    <CollapsibleCard title="Failed-check log settings" storageKey="failures.settings" defaultExpanded={false}>
      <Typography variant="body2" color="text.secondary">
        A change takes effect when the configuration is saved and reloaded.
      </Typography>
      <List dense disablePadding sx={{ mt: 1 }}>
        {keys.map((entry) => (
          <SettingsItem key={entry.key} path={entry.path} setting={entry} />
        ))}
      </List>
    </CollapsibleCard>
  );
}
